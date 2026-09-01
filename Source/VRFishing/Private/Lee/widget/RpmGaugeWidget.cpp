// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/RpmGaugeWidget.h"
#include "Lee/widget/ReelRPMThresholdReader.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
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

/** @brief 自前描画。危険弧 → 安全弧 → 目盛り → 数字ラベル → 針 の順でレイヤを積む */
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

		// FishFightMeterWidget と同一の区間 [Min, Max] で分類する（表示のみ）
		if (NewRPM < SafeMinRPM)
		{
			GaugeState = EHandSpeedState::TooSlow;
		}
		else if (NewRPM > SafeMaxRPM)
		{
			GaugeState = EHandSpeedState::TooFast;
		}
		else
		{
			GaugeState = EHandSpeedState::Good;
		}

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
	if (!ReelState)
	{
		return;
	}

	// プロパティ ポインタは一度だけ解決する（UClass は不変。値そのものは実行時に変わるため都度読み取る）
	if (!bTriedResolveProps)
	{
		bTriedResolveProps = true;
		const UClass* ReelClass = ReelState->GetClass();
		MinRPMProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("MinAllowedRPM"));
		WheelMaxRPMProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("WheelMaxAllowedRPM"));
		StickMaxRPMProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("StickMaxAllowedRPM"));
		if (!MinRPMProp || !WheelMaxRPMProp || !StickMaxRPMProp)
		{
			UE_LOG(LogFishing, Warning, TEXT("[RpmGauge] ReelState の RPM 閾値が読み取れないため、フォールバック値で表示します"));
		}
	}

	if (MinRPMProp && WheelMaxRPMProp && StickMaxRPMProp)
	{
		SafeMinRPM = FMath::Max(MinRPMProp->GetPropertyValue_InContainer(ReelState), 0.0f);
		// 入力デバイスに応じた速すぎ閾値を使用（FishFightMeterWidget と同一基準）
		SafeMaxRPM = FMath::Max(LeeReelRpm::ResolveMaxAllowedRPM(WheelMaxRPMProp->GetPropertyValue_InContainer(ReelState), StickMaxRPMProp->GetPropertyValue_InContainer(ReelState)), SafeMinRPM);
	}
	else
	{
		// フォールバック：閾値が読み取れない場合はデザイン既定値で表示する
		SafeMinRPM = 0.0f;
		SafeMaxRPM = FallbackSafeMaxRPM;
	}
	DisplayRangeMaxRPM = FMath::Max(SafeMaxRPM, 0.0f) * RangeHeadroom;
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
	const float RangeMax = FMath::Max(DisplayRangeMaxRPM, KINDA_SMALL_NUMBER);
	const float SafeStartNorm = FMath::Clamp(SafeMinRPM / RangeMax, 0.0f, 1.0f);
	const float SafeEndNorm = FMath::Clamp(SafeMaxRPM / RangeMax, 0.0f, 1.0f);

	auto BuildArcPoints = [this, BandRadius, Segments](TArray<FVector2f>& OutPoints, float StartNorm, float EndNorm)
	{
		if (EndNorm <= StartNorm)
		{
			return;
		}
		OutPoints.Reset();
		for (int32 k = 0; k <= Segments; ++k)
		{
			const float ArcT = StartNorm + (EndNorm - StartNorm) * static_cast<float>(k) / static_cast<float>(Segments);
			OutPoints.Add(CachedCenter + GaugeAngleToDir(GaugeStartAngleDeg + (GaugeEndAngleDeg - GaugeStartAngleDeg) * ArcT) * BandRadius);
		}
	};
	BuildArcPoints(DangerArcPoints, SafeEndNorm, 1.0f);
	BuildArcPoints(SafeArcPoints, SafeStartNorm, SafeEndNorm);

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
			ScaleLabels.Add(FString::Printf(TEXT("%d"), FMath::RoundToInt(DisplayRangeMaxRPM * LabelT)));
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
	Hash = HashCombine(Hash, GetTypeHash(DisplayRangeMaxRPM));
	Hash = HashCombine(Hash, GetTypeHash(SafeMinRPM));
	Hash = HashCombine(Hash, GetTypeHash(GaugeStartAngleDeg));
	Hash = HashCombine(Hash, GetTypeHash(GaugeEndAngleDeg));
	Hash = HashCombine(Hash, GetTypeHash(GaugeRadiusRatio));
	Hash = HashCombine(Hash, GetTypeHash(MajorTickLengthRatio));
	Hash = HashCombine(Hash, GetTypeHash(MinorTickLengthRatio));
	return Hash;
}

float URpmGaugeWidget::RPMToAngleDeg(float RPM) const
{
	const float RangeMax = FMath::Max(DisplayRangeMaxRPM, KINDA_SMALL_NUMBER);
	const float Normalized = FMath::Clamp(RPM / RangeMax, 0.0f, 1.0f);
	return FMath::Lerp(GaugeStartAngleDeg, GaugeEndAngleDeg, Normalized);
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
