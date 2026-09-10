// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/RpmGaugeWidget.h"
#include "Lee/widget/ReelRPMThresholdReader.h"
#include "Lee/settings/FishingLoadSettingsDeveloperSettings.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Styling/CoreStyle.h"
#include "VRFishingLog.h"

namespace
{
	/**
	 * @brief ゲージ角度（度・0=真上・正=時計回り）からローカル空間の方向ベクトルへ変換する。
	 * @note 画面座標は Y が下向きのため -Cos を使う。
	 *       検算：0°→(0,-1)=真上、90°→(1,0)=右、-120°→(-0.866,+0.5)=左下（メーターの零位）。
	 */
	FVector2f GaugeAngleToDir(float AngleDeg)
	{
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		return FVector2f(FMath::Sin(AngleRad), -FMath::Cos(AngleRad));
	}
}

URpmGaugeWidget::URpmGaugeWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** @brief 生成時処理。針テクスチャのフォールバック設定 → コンポーネント購読 → 閾値初期取得 */
void URpmGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 貼り付け針の設定漏れに備えた実行時フォールバック（アセットは書き換えない）
	if (Image_Needle)
	{
		Image_Needle->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
		Image_Needle->SetRenderTransformAngle(0.0f);
		LastAppliedNeedleImageAngle = 0.0f;
	}

	TryInitializeComponents();

	// 閾値の初期取得（最初の Tick 定期更新までフォールバック値のままになるのを防ぐ）
	RefreshThresholds();

	// 初期表示の BP 通知（FightMeter の OnRPMChanged(0, TooSlow) 初期化と同じ趣旨）
	OnGaugeRPMChanged(0.0f, EHandSpeedState::TooSlow);
}

void URpmGaugeWidget::NativeDestruct()
{
	if (ReelState)
	{
		ReelState->OnRPMCalculated.RemoveDynamic(this, &URpmGaugeWidget::HandleRPMCalculated);
	}
	if (StateManager)
	{
		StateManager->OnFishingStateChanged.RemoveDynamic(this, &URpmGaugeWidget::HandleFishingStateChanged);
	}
	Super::NativeDestruct();
}

/** @brief 毎フレーム処理。閾値定期更新 → 減衰判定 → 針の平滑追従 → 描画キャッシュ更新 */
void URpmGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bComponentsInitialized)
	{
		// 世界空間 UI では Pawn が遅れて就位することがあるため Tick で再試行する
		TryInitializeComponents();
		if (!bComponentsInitialized)
		{
			return;
		}
	}

	const double Now = FPlatformTime::Seconds();

	// ---- 閾値の定期更新 ----
	// LoadApplier が Pawn BeginPlay で閾値を上書きするため NativeConstruct 時点の 1 回きり読み取りは禁止。
	// 実行中の Project Settings 変更にも 0.25 秒以内に追従する。
	if (Now - LastThresholdRefreshTime >= ThresholdRefreshInterval)
	{
		LastThresholdRefreshTime = Now;
		RefreshThresholds();
	}

	// ---- 減衰判定 ----
	// リールフェーズ中は最後に受け取った RPM を保持する（入力が止まっても 0 に戻さない）。
	// RPM は 1 回転ごとの更新のため、無入力で 0 に戻すと低速回転時に針が往復してしまうため。
	// リールフェーズ外へ抜けたらセットの区切りとして 0 へ緩やかに戻す。
	if (bHasEverReceivedRPM && !bDecaying && bDisplayOnlyInReelPhase && !bGaugeActive)
	{
		TargetRPMForNeedle = 0.0f;
		bDecaying = true;
	}

	// ---- 針の平滑追従 ----
	const float InterpSpeed = bDecaying ? NeedleDecaySpeed : NeedleSmoothingSpeed;
	DisplayedRPM = FMath::FInterpTo(DisplayedRPM, TargetRPMForNeedle, InDeltaTime, InterpSpeed);
	DisplayedRPM = FMath::Clamp(DisplayedRPM, 0.0f, FMath::Max(DisplayRangeMaxRPM, 0.0f));
	if (bDecaying && DisplayedRPM < 0.05f)
	{
		DisplayedRPM = 0.0f; // 残差を打ち切って針を静止させる
	}

	// ---- 針角度の反映 ----
	NeedleAngleDeg = RPMToAngleDeg(DisplayedRPM);
	if (Image_Needle && FMath::Abs(NeedleAngleDeg - LastAppliedNeedleImageAngle) > 0.1f)
	{
		Image_Needle->SetRenderTransformAngle(NeedleAngleDeg);
		LastAppliedNeedleImageAngle = NeedleAngleDeg;
	}

	// ---- 描画キャッシュの更新（サイズ／パラメータ変化時のみ再構築） ----
	const FVector2f LocalSize(MyGeometry.GetLocalSize());
	const bool bSizeChanged =
		(FMath::Abs(LocalSize.X - CachedGeometrySize.X) > 0.5f) ||
		(FMath::Abs(LocalSize.Y - CachedGeometrySize.Y) > 0.5f);
	const uint32 ParamsHash = ComputeDrawParamsHash();
	if (bDrawCacheDirty || bSizeChanged || ParamsHash != CachedDrawParamsHash)
	{
		CachedGeometrySize = LocalSize;
		CachedDrawParamsHash = ParamsHash;
		RebuildDrawCache();
	}
}

/** @brief 自前描画。遅すぎ弧 → 危険弧 → 安全弧 → 目盛り → 数字ラベル → 針 の順でレイヤを積む */
int32 URpmGaugeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// 文字盤テクスチャがある／自前描画オフの場合は何も描かない（子 Image は親の OnPaint より
	// 後に描かれるため、文字盤テクスチャ設置＝全面差し替え）
	if (!bSelfDrawDial || Image_DialFace || CachedRadius <= 0.0f)
	{
		return BaseLayer;
	}

	int32 CurrentLayer = BaseLayer;

	// ---- 遅すぎ弧（遅すぎ区間 [0, SafeMin]） ----
	if (TooSlowArcPoints.Num() >= 2)
	{
		++CurrentLayer;
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), TooSlowArcPoints, ESlateDrawEffect::None, TooSlowZoneColor, true, CachedBandThickness);
	}

	// ---- 危険弧（速すぎ区間 [SafeMax, RangeMax]） ----
	if (DangerArcPoints.Num() >= 2)
	{
		++CurrentLayer;
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), DangerArcPoints, ESlateDrawEffect::None, DangerZoneColor, true, CachedBandThickness);
	}

	// ---- 安全弧（適正区間 [SafeMin, SafeMax]） ----
	if (SafeArcPoints.Num() >= 2)
	{
		++CurrentLayer;
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), SafeArcPoints, ESlateDrawEffect::None, SafeZoneColor, true, CachedBandThickness);
	}

	// ---- 目盛り（分断された線分のため 2 点 1 組ずつ発射する） ----
	++CurrentLayer;
	for (int32 i = 0; i + 1 < MajorTickPoints.Num(); i += 2)
	{
		TArray<FVector2f> Segment;
		Segment.Add(MajorTickPoints[i]);
		Segment.Add(MajorTickPoints[i + 1]);
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Segment), ESlateDrawEffect::None, TickColor, true, MajorTickThickness);
	}
	for (int32 i = 0; i + 1 < MinorTickPoints.Num(); i += 2)
	{
		TArray<FVector2f> Segment;
		Segment.Add(MinorTickPoints[i]);
		Segment.Add(MinorTickPoints[i + 1]);
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Segment), ESlateDrawEffect::None, TickColor, true, MinorTickThickness);
	}

	// ---- 数字ラベル（数字のみ描くため言語ルールの対象外） ----
	if (bDrawScaleLabels)
	{
		++CurrentLayer;
		for (int32 i = 0; i < ScaleLabels.Num(); ++i)
		{
			const FVector2f LabelSize(static_cast<float>(ScaleLabelFontSize) * 1.5f, static_cast<float>(ScaleLabelFontSize) * 1.25f);
			const FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(ScaleLabelPositions[i] - LabelSize * 0.5f));
			FSlateDrawElement::MakeText(OutDrawElements, CurrentLayer, LabelGeometry, ScaleLabels[i], 0, ScaleLabels[i].Len(), LabelFontInfo, ESlateDrawEffect::None, ScaleLabelColor);
		}
	}

	// ---- 針（貼り付けテクスチャがない場合のみ自前描画・色は判定状態に連動） ----
	if (!Image_Needle)
	{
		++CurrentLayer;
		const FVector2f NeedleDir = GaugeAngleToDir(NeedleAngleDeg);
		TArray<FVector2f> NeedlePoints;
		NeedlePoints.Add(CachedCenter - NeedleDir * (CachedRadius * NeedleTailRatio));
		NeedlePoints.Add(CachedCenter + NeedleDir * (CachedRadius * NeedleLengthRatio));
		const FLinearColor NeedleTint = ResolveNeedleColor();
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), MoveTemp(NeedlePoints), ESlateDrawEffect::None, NeedleTint, true, NeedleThickness);

		// 針の根元を覆う中心キャップ
		const float CapSize = FMath::Max(CachedRadius * 0.10f, 4.0f);
		const FPaintGeometry CapGeometry = AllottedGeometry.ToPaintGeometry(FVector2f(CapSize, CapSize), FSlateLayoutTransform(CachedCenter - FVector2f(CapSize * 0.5f, CapSize * 0.5f)));
		FSlateDrawElement::MakeBox(OutDrawElements, CurrentLayer, CapGeometry, FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, NeedleTint);
	}

	return CurrentLayer;
}

// ==================== デリゲート ハンドラ ====================

void URpmGaugeWidget::HandleRPMCalculated(float NewRPM)
{
	if (!bDisplayOnlyInReelPhase || bGaugeActive)
	{
		TargetRPMForNeedle = NewRPM;
		bDecaying = false;
		bHasEverReceivedRPM = true;

		// 回転ごとに最新閾値を反映（負荷プリセットはゲーム中に変わり得るため）
		RefreshThresholds();

		// 実判定（JudgeRPM）と同一基準・同一優先順で分類する（共通実装。表示のみ）
		GaugeState = LeeReelRpm::ClassifyRPM(SafeMinRPM, SafeMaxRPM, NewRPM);

		// 値も状態も前回発火時とほぼ同じなら BP 通知を抑える
		if (FMath::Abs(NewRPM - LastNotifiedRPM) > 0.05f || GaugeState != LastNotifiedState)
		{
			LastNotifiedRPM = NewRPM;
			LastNotifiedState = GaugeState;
			OnGaugeRPMChanged(DisplayedRPM, GaugeState);
		}
	}
	else
	{
		// ロック中に RPM が届いた＝リール入力は生きているが表示はフェーズ外。
		// 毎回出すとログが流れるため 1 秒に 1 回だけ警告（FightMeter と同じ方式）
		static double LastLockedWarnTime = 0.0;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastLockedWarnTime >= 1.0)
		{
			LastLockedWarnTime = Now;
			UE_LOG(LogFishing, Warning, TEXT("[RpmGauge] 非リールフェーズ中の RPM 受信を破棄: RPM=%.1f"), NewRPM);
		}
	}
}

void URpmGaugeWidget::HandleFishingStateChanged(UFishingStateComponentBase* NewState)
{
	// EFishingPhase 列挙（FishFightMeterWidget 側の定義）に依存させず、
	// リールステートと同一インスタンスかどうかのポインタ比較だけで判定する
	const bool bNewActive = (NewState != nullptr && NewState == ReelState);
	if (bNewActive != bGaugeActive)
	{
		bGaugeActive = bNewActive;
		// フェーズへ入ったら閾値を即時更新する（フェーズ外では Tick 側の判定で 0 へ減衰する）
		if (bGaugeActive)
		{
			RefreshThresholds();
		}
	}
}

// ==================== 内部処理 ====================

void URpmGaugeWidget::TryInitializeComponents()
{
	APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!OwnerPawn)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			OwnerPawn = PC->GetPawn();
		}
	}
	if (!OwnerPawn)
	{
		return; // Pawn が未就位 → Tick で再試行
	}

	ReelState = OwnerPawn->FindComponentByClass<UFishingReelStateComponent>();
	StateManager = OwnerPawn->FindComponentByClass<UFishingStateManagerComponent>();

	if (ReelState)
	{
		// NativeConstruct が WidgetComponent の再構築で二度呼ばれても二重購読しない
		ReelState->OnRPMCalculated.AddUniqueDynamic(this, &URpmGaugeWidget::HandleRPMCalculated);
	}
	if (StateManager)
	{
		StateManager->OnFishingStateChanged.AddUniqueDynamic(this, &URpmGaugeWidget::HandleFishingStateChanged);
		// 生成時点のフェーズを初回同期（リール中に生成されたケースに対応）
		HandleFishingStateChanged(StateManager->GetCurrentState());
	}

	bComponentsInitialized = true;
	LastThresholdRefreshTime = -1.0e9; // 次の Tick で即座に 1 回目の閾値取得を行う
}

void URpmGaugeWidget::RefreshThresholds()
{
	// 満量程の基準（全プリセット最大の速すぎ閾値）は ReelState に依存しないため先に解決する。
	// Project Settings の実行中変更にも追従させたいので、CDO は毎回参照する。
	RangeBaseRPM = ResolveRangeBaseRPM(TableMaxAnyDeviceRPM);

	if (!ReelState)
	{
		// ReelState 未就位でも目盛りが潰れないよう、表の基準から暫定の満量程を作る（Tick で再試行される）。
		// FitToSafeWindow は閾値が無いと決められないため、ここでは表基準の暫定値で代用する
		const float PlaceholderRangeMax = FMath::Max(RangeBaseRPM, KINDA_SMALL_NUMBER) * FMath::Max(RangeHeadroom, 1.0f);
		DisplayRangeMaxRPM = bRoundRangeMaxToNiceValue ? SnapRangeMaxToNiceValue(PlaceholderRangeMax) : PlaceholderRangeMax;
		return;
	}

	// 閾値の読み取りは共通ユーティリティへ一本化（0.25 秒間隔なので反射の名前検索コストは無視できる）
	float MinRPM = 0.0f;
	float WheelMaxRPM = 0.0f;
	float StickMaxRPM = 0.0f;
	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// ホイール専用の下限を受け取る（SafeMinRPM の決定に使用）
	float WheelMinRPM = 0.0f;
	if (LeeReelRpm::ReadReelRPMThresholds(ReelState, MinRPM, WheelMaxRPM, StickMaxRPM, WheelMinRPM))
	//if (LeeReelRpm::ReadReelRPMThresholds(ReelState, MinRPM, WheelMaxRPM, StickMaxRPM))
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	{
		// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// 下限も「実判定（JudgeRPM）が実際に使った値」を最優先する（ホイールとスティックで下限が異なるため）
		SafeMinRPM = FMath::Max(LeeReelRpm::ResolveJudgedMinAllowedRPM(ReelState, MinRPM, WheelMinRPM), 0.0f);
		//SafeMinRPM = FMath::Max(MinRPM, 0.0f);
		// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// 上限は「実判定（JudgeRPM）が実際に使った値」を最優先する（未入力時のみデバイス予測）。
		// デザイン側の clamp は従来どおり維持する（共通ユーティリティは clamp しないため）
		SafeMaxRPM = FMath::Max(LeeReelRpm::ResolveJudgedMaxAllowedRPM(ReelState, WheelMaxRPM, StickMaxRPM), SafeMinRPM);
	}
	else
	{
		// フォールバック：閾値が読み取れない場合はデザイン既定値で表示する
		if (!bWarnedThresholdFallback)
		{
			bWarnedThresholdFallback = true;
			UE_LOG(LogFishing, Warning, TEXT("[RpmGauge] ReelState の RPM 閾値が読み取れないため、フォールバック値で表示します"));
		}
		SafeMinRPM = 0.0f;
		SafeMaxRPM = FallbackSafeMaxRPM;
	}

	// ---- 満量程の決定（モードごとに基準が異なる） ----
	float EffectiveBaseRPM = 0.0f;
	switch (RangeMode)
	{
	case ERpmGaugeRangeMode::FitToSafeWindow:
		// 適正区間 [Min, Max] の中心を盤面の 12 時方向（正規化 0.5）へ置く。RPM→角度は 0 起点の
		// 線形写像のため、中心が 0.5 に来る解は「満量程 = Min + Max」のみ。
		// RangeHeadroom と切り上げは掛けない（どちらも中心を 0.5 からずらすため）
		EffectiveBaseRPM = FMath::Max(SafeMinRPM + SafeMaxRPM, KINDA_SMALL_NUMBER);
		DisplayRangeMaxRPM = EffectiveBaseRPM;
		break;

	case ERpmGaugeRangeMode::FixedFullScale:
		{
			// 絶対目盛り。基準（全プリセット最大）へ現在の速すぎ閾値も下限として混ぜる安全弁付き
			// （ApplyRotationLoadLevel のハードコード値は Project Settings 表より大きく、表の要素数が
			//   不足するとその値が残り、混ぜないと適正帯・危険帯が盤面からはみ出すため）
			EffectiveBaseRPM = FMath::Max3(RangeBaseRPM, SafeMaxRPM, KINDA_SMALL_NUMBER);
			const float RawRangeMax = EffectiveBaseRPM * FMath::Max(RangeHeadroom, 1.0f);
			DisplayRangeMaxRPM = bRoundRangeMaxToNiceValue ? SnapRangeMaxToNiceValue(RawRangeMax) : RawRangeMax;
		}
		break;

	default: // ERpmGaugeRangeMode::FitToCurrentSafeMax
		EffectiveBaseRPM = FMath::Max(SafeMaxRPM, KINDA_SMALL_NUMBER);
		DisplayRangeMaxRPM = EffectiveBaseRPM * FMath::Max(RangeHeadroom, 1.0f);
		break;
	}

	// ---- 値が動いた瞬間だけログ（0.25 秒ごとに流さない。HMD 内でも確認できる検証手段） ----
	if (!FMath::IsNearlyEqual(DisplayRangeMaxRPM, LastLoggedRangeMaxRPM, 0.05f)
		|| !FMath::IsNearlyEqual(SafeMinRPM, LastLoggedSafeMinRPM, 0.05f)
		|| !FMath::IsNearlyEqual(SafeMaxRPM, LastLoggedSafeMaxRPM, 0.05f))
	{
		LastLoggedRangeMaxRPM = DisplayRangeMaxRPM;
		LastLoggedSafeMinRPM = SafeMinRPM;
		LastLoggedSafeMaxRPM = SafeMaxRPM;
		UE_LOG(LogFishing, Log, TEXT("[RpmGauge] 満量程=%.1f (実効基準=%.1f / 表基準=%.1f / 遅すぎ=%.1f / 速すぎ=%.1f / モード=%d)"),
			DisplayRangeMaxRPM, EffectiveBaseRPM, RangeBaseRPM, SafeMinRPM, SafeMaxRPM, static_cast<int32>(RangeMode));
	}

	// 表と実効閾値の不整合（表の要素数不足で ApplyRotationLoadLevel のハードコード値が残ったケース）を検出する。
	// 比較相手はデバイス非依存の表最大にする（実行デバイスによっては表の別列の値が正となるため。
	// 例：非 VR 実行＋自転車デバイス＝スティック扱い → 表の Wheel 列より Stick 列が大きい）
	if (TableMaxAnyDeviceRPM > 0.0f && SafeMaxRPM > TableMaxAnyDeviceRPM + 0.5f)
	{
		UE_LOG(LogFishing, Warning, TEXT("[RpmGauge] 実効速すぎ閾値 %.1f が Project Settings 表の最大 %.1f を超えています。表の要素数を確認してください"),
			SafeMaxRPM, TableMaxAnyDeviceRPM);
	}
}

float URpmGaugeWidget::ResolveRangeBaseRPM(float& OutTableMaxAnyDeviceRPM) const
{
	// DeveloperSettings の CDO を毎回参照する（実行中の Project Settings 変更に追従）。
	// ini 再読込で TArray の内部バッファが差し替わり得るため、配列ポインタのキャッシュは禁止。
	const TArray<FRPMPresetThresholds>& Table = GetDefault<UFishingLoadSettingsDeveloperSettings>()->RPMThresholdTable;

	// 列ごとではなくプリセットごとにデバイス解決してから最大を採る。
	// 列ごとに採ると（Wheel 最大 50 / Stick 最大 90）非 VR 実行時にスティック用の値で目盛りが伸びてしまう。
	float MaxResolvedRPM = 0.0f;
	OutTableMaxAnyDeviceRPM = 0.0f;
	for (const FRPMPresetThresholds& Preset : Table)
	{
		MaxResolvedRPM = FMath::Max(MaxResolvedRPM, LeeReelRpm::ResolveMaxAllowedRPM(Preset.WheelMaxAllowedRPM, Preset.StickMaxAllowedRPM));
		// デバイス非依存の表最大（実効閾値が表の想定を超えていないかの判定に使う）
		OutTableMaxAnyDeviceRPM = FMath::Max(OutTableMaxAnyDeviceRPM, FMath::Max(Preset.WheelMaxAllowedRPM, Preset.StickMaxAllowedRPM));
	}

	// 明示指定は最優先（実機を見ながら設計者が詰めるための逃げ道）
	if (FixedRangeMaxRPM > 0.0f)
	{
		return FixedRangeMaxRPM;
	}

	// 表が空／全要素 0 の場合はデザイン既定のフォールバック基準へ落とす（潰れた目盛りの防止）
	return (MaxResolvedRPM > KINDA_SMALL_NUMBER) ? MaxResolvedRPM : FMath::Max(FallbackSafeMaxRPM, KINDA_SMALL_NUMBER);
}

float URpmGaugeWidget::SnapRangeMaxToNiceValue(float RawRangeMax) const
{
	// 主目盛り 1 本ぶんの生値を 5 の倍数へ切り上げ、それを分割数倍して満量程へ戻す。
	// こうすると数字ラベルが必ず整数（例：9 本・112.5 → 1 目盛り 15 → 満量程 120）になり、
	// RebuildDrawCache のラベル式（DisplayRangeMaxRPM × 目盛り番号比）は無変更で済む。
	const int32 NumSpans = FMath::Max(MajorTickCount - 1, 1);
	const float RawStep = RawRangeMax / static_cast<float>(NumSpans);
	const float Step = FMath::Max(FMath::CeilToFloat(RawStep / 5.0f) * 5.0f, 1.0f);
	const float Snapped = Step * static_cast<float>(NumSpans);

	// 主目盛りが多いと切り上げが過大になる（例：21 本だと 112.5 → 200）ため、1.5 倍を超えるなら切り上げない
	return (Snapped <= RawRangeMax * 1.5f) ? Snapped : RawRangeMax;
}

void URpmGaugeWidget::RebuildDrawCache()
{
	CachedRadius = FMath::Max(FMath::Min(CachedGeometrySize.X, CachedGeometrySize.Y) * GaugeRadiusRatio, 1.0f);
	CachedCenter = FVector2f(static_cast<float>(CachedGeometrySize.X * GaugeCenterRatio.X), static_cast<float>(CachedGeometrySize.Y * GaugeCenterRatio.Y));

	const int32 NumMajors = FMath::Max(MajorTickCount, 2);
	const int32 NumMinors = FMath::Max(MinorTicksPerMajor, 0);
	const float SweepDeg = GaugeEndAngleDeg - GaugeStartAngleDeg;
	const float MajorLen = CachedRadius * MajorTickLengthRatio;
	const float MinorLen = CachedRadius * MinorTickLengthRatio;
	const float OuterRadius = CachedRadius;

	// ---- 目盛り（2 点 1 組のフラット配列へ格納） ----
	MajorTickPoints.Reset();
	MinorTickPoints.Reset();
	for (int32 i = 0; i < NumMajors; ++i)
	{
		const float TickT = static_cast<float>(i) / static_cast<float>(NumMajors - 1);
		const FVector2f Dir = GaugeAngleToDir(GaugeStartAngleDeg + SweepDeg * TickT);
		MajorTickPoints.Add(CachedCenter + Dir * (OuterRadius - MajorLen));
		MajorTickPoints.Add(CachedCenter + Dir * OuterRadius);

		if (NumMinors > 0 && i < NumMajors - 1)
		{
			for (int32 j = 1; j <= NumMinors; ++j)
			{
				const float MinorT = (static_cast<float>(i) + static_cast<float>(j) / static_cast<float>(NumMinors + 1)) / static_cast<float>(NumMajors - 1);
				const FVector2f MinorDir = GaugeAngleToDir(GaugeStartAngleDeg + SweepDeg * MinorT);
				MinorTickPoints.Add(CachedCenter + MinorDir * (OuterRadius - MinorLen));
				MinorTickPoints.Add(CachedCenter + MinorDir * OuterRadius);
			}
		}
	}

	// ---- 判定区間の弧（帯は目盛りリングの内側に描く） ----
	CachedBandThickness = OuterRadius * ZoneBandRatio;
	const float BandRadius = OuterRadius * (1.0f - ZoneBandRatio);
	const int32 Segments = FMath::Max(ArcSegmentCount, 2);

	// 区間境界の正規化位置。折れ線モードでは閾値に依存しない固定割合（＝色帯が動かない）、
	// それ以外は閾値から算出する。閾値が未取得で折れ線写像が成立しない間は後者へ退避する
	float SafeStartNorm = 0.0f;
	float SafeEndNorm = 0.0f;
	if (IsSafeWindowScaleValid())
	{
		SafeStartNorm = GetSafeWindowSide();
		SafeEndNorm = SafeStartNorm + GetSafeWindowShare();
	}
	else
	{
		const float RangeMax = FMath::Max(DisplayRangeMaxRPM, KINDA_SMALL_NUMBER);
		SafeStartNorm = FMath::Clamp(SafeMinRPM / RangeMax, 0.0f, 1.0f);
		SafeEndNorm = FMath::Clamp(SafeMaxRPM / RangeMax, 0.0f, 1.0f);
	}

	auto BuildArcPoints = [this, BandRadius, Segments](TArray<FVector2f>& OutPoints, float StartNorm, float EndNorm)
	{
		// 区間が空でも必ず頂点を捨てる。旧実装は早期 return のみで前回の頂点が残留し、
		// NativePaint が消えるべき帯を描き続けていた（遅すぎ帯は SafeMin==0 の降格時に必ず空になる）
		OutPoints.Reset();
		if (EndNorm <= StartNorm)
		{
			return;
		}
		for (int32 k = 0; k <= Segments; ++k)
		{
			const float ArcT = StartNorm + (EndNorm - StartNorm) * static_cast<float>(k) / static_cast<float>(Segments);
			OutPoints.Add(CachedCenter + GaugeAngleToDir(GaugeStartAngleDeg + (GaugeEndAngleDeg - GaugeStartAngleDeg) * ArcT) * BandRadius);
		}
	};

	// RPM 軸の昇順（遅すぎ → 適正 → 危険）で積む。3 帯は境界を共有するだけで重ならない
	if (bDrawTooSlowZone)
	{
		BuildArcPoints(TooSlowArcPoints, 0.0f, SafeStartNorm);
	}
	else
	{
		// フラグを落としたときに古い頂点が残らないよう明示的に捨てる
		TooSlowArcPoints.Reset();
	}
	BuildArcPoints(SafeArcPoints, SafeStartNorm, SafeEndNorm);
	BuildArcPoints(DangerArcPoints, SafeEndNorm, 1.0f);

	// ---- 数字ラベル ----
	ScaleLabels.Reset();
	ScaleLabelPositions.Reset();
	if (bDrawScaleLabels)
	{
		if (ScaleLabelFontSize != CachedLabelFontSize)
		{
			CachedLabelFontSize = ScaleLabelFontSize;
			LabelFontInfo = FCoreStyle::GetDefaultFontStyle("Bold", CachedLabelFontSize);
		}
		const float LabelRadius = OuterRadius * 1.18f;
		for (int32 i = 0; i < NumMajors; ++i)
		{
			const float LabelT = static_cast<float>(i) / static_cast<float>(NumMajors - 1);
			const FVector2f Dir = GaugeAngleToDir(GaugeStartAngleDeg + SweepDeg * LabelT);
			// ラベルは写像の逆算で求める（折れ線モードでは 1/3・2/3 の目盛りが閾値そのものになる）
			ScaleLabels.Add(FString::Printf(TEXT("%d"), FMath::RoundToInt(NormToRPM(LabelT))));
			ScaleLabelPositions.Add(CachedCenter + Dir * LabelRadius);
		}
	}

	bDrawCacheDirty = false;
}

uint32 URpmGaugeWidget::ComputeDrawParamsHash() const
{
	uint32 Hash = GetTypeHash(MajorTickCount);
	Hash = HashCombine(Hash, GetTypeHash(MinorTicksPerMajor));
	Hash = HashCombine(Hash, GetTypeHash(bDrawScaleLabels));
	Hash = HashCombine(Hash, GetTypeHash(ArcSegmentCount));
	Hash = HashCombine(Hash, GetTypeHash(ZoneBandRatio));
	Hash = HashCombine(Hash, GetTypeHash(RangeHeadroom));
	// 量程・閾値の解決に効くスイッチ類（実際の値は DisplayRangeMaxRPM / SafeMinRPM にも現れるが、
	// 「値は同じで意味だけ変わった」ケースを取りこぼさないよう明示的に混ぜる）
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(RangeMode)));
	Hash = HashCombine(Hash, GetTypeHash(SafeWindowShareRatio));
	Hash = HashCombine(Hash, GetTypeHash(bRoundRangeMaxToNiceValue));
	Hash = HashCombine(Hash, GetTypeHash(bDrawTooSlowZone));
	Hash = HashCombine(Hash, GetTypeHash(DisplayRangeMaxRPM));
	Hash = HashCombine(Hash, GetTypeHash(SafeMinRPM));
	// 危険弧の始点（SafeEndNorm = SafeMaxRPM / DisplayRangeMaxRPM）は SafeMaxRPM に依存する。
	// 量程が同じ値に丸まった場合（手動指定時など）でも帯の境界が変わるため、必ず混ぜる
	Hash = HashCombine(Hash, GetTypeHash(SafeMaxRPM));
	Hash = HashCombine(Hash, GetTypeHash(GaugeStartAngleDeg));
	Hash = HashCombine(Hash, GetTypeHash(GaugeEndAngleDeg));
	Hash = HashCombine(Hash, GetTypeHash(GaugeRadiusRatio));
	Hash = HashCombine(Hash, GetTypeHash(MajorTickLengthRatio));
	Hash = HashCombine(Hash, GetTypeHash(MinorTickLengthRatio));
	return Hash;
}

float URpmGaugeWidget::RPMToAngleDeg(float RPM) const
{
	// 写像はモード依存（折れ線モードでは区間ごとに傾きが変わる）。針も目盛りラベルも同じ写像を使う
	return FMath::Lerp(GaugeStartAngleDeg, GaugeEndAngleDeg, RPMToNormValue(RPM));
}

bool URpmGaugeWidget::IsSafeWindowScaleValid() const
{
	// 折れ線写像は「Min > 0 かつ Max > Min」が前提。閾値未取得（0）や誤設定では成立しない
	return RangeMode == ERpmGaugeRangeMode::FitToSafeWindow
		&& SafeMinRPM > 0.0f
		&& SafeMaxRPM > SafeMinRPM + KINDA_SMALL_NUMBER;
}

float URpmGaugeWidget::GetSafeWindowShare() const
{
	return FMath::Clamp(SafeWindowShareRatio, 0.05f, 0.9f);
}

float URpmGaugeWidget::GetSafeWindowSide() const
{
	return (1.0f - GetSafeWindowShare()) * 0.5f;
}

float URpmGaugeWidget::RPMToNormValue(float RPM) const
{
	// 折れ線写像が使えないとき（他モード・閾値未取得）は 0 起点の線形へ退避する
	if (!IsSafeWindowScaleValid())
	{
		return FMath::Clamp(RPM / FMath::Max(DisplayRangeMaxRPM, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	}

	// 3 区間の折れ線写像。各区間をそれぞれ線形補間することで、色帯の角度が閾値に依らず一定になる
	const float Share = GetSafeWindowShare();
	const float Side = GetSafeWindowSide();

	if (RPM <= SafeMinRPM)
	{
		// 遅すぎ区間 [0, Min] → [0, Side]
		return FMath::Clamp(RPM / SafeMinRPM * Side, 0.0f, Side);
	}
	if (RPM <= SafeMaxRPM)
	{
		// 適正区間 [Min, Max] → [Side, Side + Share]
		return Side + (RPM - SafeMinRPM) / (SafeMaxRPM - SafeMinRPM) * Share;
	}
	// 速すぎ区間 [Max, 満量程 = Min + Max] → [Side + Share, 1]
	const float RedSpan = FMath::Max(DisplayRangeMaxRPM - SafeMaxRPM, KINDA_SMALL_NUMBER);
	return FMath::Clamp(Side + Share + (RPM - SafeMaxRPM) / RedSpan * Side, 0.0f, 1.0f);
}

float URpmGaugeWidget::NormToRPM(float Norm) const
{
	if (!IsSafeWindowScaleValid())
	{
		return FMath::Clamp(Norm, 0.0f, 1.0f) * DisplayRangeMaxRPM;
	}

	const float Share = GetSafeWindowShare();
	const float Side = GetSafeWindowSide();
	const float ClampedNorm = FMath::Clamp(Norm, 0.0f, 1.0f);

	if (ClampedNorm <= Side)
	{
		return ClampedNorm / Side * SafeMinRPM;
	}
	if (ClampedNorm <= Side + Share)
	{
		return SafeMinRPM + (ClampedNorm - Side) / Share * (SafeMaxRPM - SafeMinRPM);
	}
	const float RedSpan = FMath::Max(DisplayRangeMaxRPM - SafeMaxRPM, KINDA_SMALL_NUMBER);
	return SafeMaxRPM + (ClampedNorm - Side - Share) / Side * RedSpan;
}

FLinearColor URpmGaugeWidget::ResolveNeedleColor() const
{
	if (!bHasEverReceivedRPM)
	{
		return NeedleColor;
	}
	switch (GaugeState)
	{
	case EHandSpeedState::Good:    return StateGoodColor;
	case EHandSpeedState::TooSlow: return StateTooSlowColor;
	case EHandSpeedState::TooFast: return StateTooFastColor;
	default:                       return NeedleColor;
	}
}
