// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/FishFightMeterWidget.h"
#include "Lee/widget/ReelRPMThresholdReader.h"
// 2026.09.09 Lee startーーー ステップバー 3 状態表示 ーーー
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
// 2026.09.09 Lee endーーー
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "Tanimura/Component/FishingCatchingStateComponent.h"
#include "Tanimura/Component/FishingResultStateComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "VRFishingLog.h"

UFishFightMeterWidget::UFishFightMeterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 2026.09.09 Lee startーーー 未到達ドットの既定ブラシ（円形。WBP の Class Defaults で差し替え可） ーーー
	// HalfHeightRadius＝高さの半分を半径にするため、任意の寸法で常に真円になる
	UnreachedDotBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	UnreachedDotBrush.ImageSize = FVector2f(24.0f, 24.0f);
	UnreachedDotBrush.TintColor = FSlateColor(FLinearColor::White);
	UnreachedDotBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	// 2026.09.09 Lee endーーー
}

/** @brief 生成時処理。オーナー Pawn から各コンポーネントを解決してデリゲートを購読する */
void UFishFightMeterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!OwnerPawn)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			OwnerPawn = PC->GetPawn();
		}
	}

	if (OwnerPawn)
	{
		HandHeightDetector = OwnerPawn->FindComponentByClass<UHandHeightDetectorComponent>();
		HandUpDownState = OwnerPawn->FindComponentByClass<UFishingStateHandUpDown>();
		ReelSimulator = OwnerPawn->FindComponentByClass<UFishingReelStateComponent>();

		if (HandUpDownState)
		{
			HandUpDownState->OnFishingStateCompleted.AddDynamic(this, &UFishFightMeterWidget::OnHandUpDownCompleted);
		}

		if (ReelSimulator)
		{
			ReelSimulator->OnRPMCalculated.AddDynamic(this, &UFishFightMeterWidget::OnRPMUpdated);
		}

		StateManager  = OwnerPawn->FindComponentByClass<UFishingStateManagerComponent>();
		ReadyState    = OwnerPawn->FindComponentByClass<UFishingReadyStateComponent>();
		CatchingState = OwnerPawn->FindComponentByClass<UFishingCatchingStateComponent>();
		ResultState   = OwnerPawn->FindComponentByClass<UFishingResultStateComponent>();

		if (StateManager)
		{
			// 二重購読対策で AddUniqueDynamic を使用（VRPawn と同じ購読方法）
			StateManager->OnFishingStateChanged.AddUniqueDynamic(this, &UFishFightMeterWidget::HandleFishingStateChanged);
		}

		bComponentsInitialized = true;
	}

	// 2026.09.09 Lee startーーー 魚アイコンの初期設定 ーーー
	// 貼り付けTexture とスロット寸法を実行時に反映する（デザイナー既定値に依存しない）
	if (Image_PhaseFish)
	{
		// スロット寸法の基準値（Texture 未設定時は正方形のまま）
		float FishW = PhaseFishSize;
		float FishH = PhaseFishSize;
		if (PhaseFishTexture)
		{
			Image_PhaseFish->SetBrushFromTexture(PhaseFishTexture);
			// テクスチャのアスペクト比を保つ（Image はスロット寸法へ引き伸ばして描画するため。
			// 高さ = PhaseFishSize を基準に、幅をテクスチャ比率から自動算出する）
			const float TexW = static_cast<float>(PhaseFishTexture->GetSurfaceWidth());
			const float TexH = static_cast<float>(PhaseFishTexture->GetSurfaceHeight());
			if (TexW > 1.0f && TexH > 1.0f)
			{
				FishH = PhaseFishSize;
				FishW = PhaseFishSize * (TexW / TexH);
			}
		}
		if (UCanvasPanelSlot* FishSlot = Cast<UCanvasPanelSlot>(Image_PhaseFish->Slot))
		{
			FishSlot->SetSize(FVector2D(FishW, FishH));
		}
	}
	// 2026.09.09 Lee endーーー

	// RPM 表示を初期化
	OnRPMChanged(0.0f, EHandSpeedState::TooSlow);

	// フェーズ表示の初回同期：Widget がゲーム途中で生成されても現在フェーズを即時反映する
	if (StateManager && StateManager->GetCurrentState())
	{
		const EFishingPhase InitialPhase = ResolvePhase(StateManager->GetCurrentState());
		// RPM ロック状態も生成時点のフェーズから初期化（Reel 中の生成に対応）
		bReelUnlocked = (InitialPhase == EFishingPhase::Reel);
		ApplyPhase(InitialPhase, StateManager->GetCurrentStateName(), false);
	}
	else
	{
		ApplyPhase(EFishingPhase::Ready, TEXT("よーい！"), false);
	}
}

/** @brief 毎フレーム処理。ステートコンポーネントから表示データを読み取り BP イベントへプッシュする */
void UFishFightMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bComponentsInitialized)
	{
		return;
	}

	const float HandPercent = HandHeightDetector ? HandHeightDetector->HandHeightPercent : 0.0f;

	// ---- コンポーネントから表示データを読み取り ----
	if (HandUpDownState)
	{
		ArrowPosition = HandUpDownState->ArrowPosition;
		ArrowState = HandUpDownState->ArrowState;
		CycleCount = HandUpDownState->CurrentUpAndDownCount;
		CurrentScore = HandUpDownState->CurrentScore;
		FinalScore = HandUpDownState->FinalScore;
	}

	// ---- BP イベント発火 ----
	OnArrowUpdated(ArrowPosition, ArrowState);
	OnScoreChanged(CurrentScore);

	// 2026.09.07 Lee startーーー 推奨範囲表示 ーーー
	// 手上下フェーズ中のみ矢印と同期して推奨範囲とフィル色を更新する
	// （他フェーズでは矢印ガイド群ごと非表示のため発火不要）
	if (CurrentPhase == EFishingPhase::HandUpDown)
	{
		PushArrowRange();
		UpdateHandRangeColor(false);
	}
	// 2026.09.07 Lee endーーー

	// 2026.09.09 Lee startーーー 魚アイコンの位置追従 ーーー
	// 現在ドットの中心は GetCachedGeometry の実寸から毎フレーム再計算する
	// （レイアウト変更に C++ を追従させないため。推奨範囲帯と同じ方式）
	UpdateStepFishPosition(InDeltaTime);
	// 2026.09.09 Lee endーーー

	// ==================== Debug ====================
	if (GEngine)
	{
		const TCHAR* ArrowStateText = TEXT("???");
		FColor ArrowColor = FColor::White;
		switch (ArrowState)
		{
		case EFishArrowState::MovingUp:			ArrowStateText = TEXT("↑");		ArrowColor = FColor::Cyan;		break;
		case EFishArrowState::WaitingAtTop:		ArrowStateText = TEXT("WAIT_TOP");	ArrowColor = FColor::Yellow;	break;
		case EFishArrowState::MovingDown:		ArrowStateText = TEXT("↓");		ArrowColor = FColor::Orange;	break;
		case EFishArrowState::WaitingAtBottom:	ArrowStateText = TEXT("WAIT_BOT");	ArrowColor = FColor::Yellow;	break;
		}

		const int32 TargetCount = HandUpDownState ? HandUpDownState->TargetUpAndDownCount : 0;
		const FString ArrowMsg = FString::Printf(
			TEXT("[Arrow] Pos:%.0f%% %s | Hand:%.0f%% | %d/%d %s"),
			ArrowPosition * 100.0f, ArrowStateText, HandPercent * 100.0f,
			CycleCount, TargetCount,
			bReelUnlocked ? TEXT("UNLOCKED") : TEXT("LOCKED"));

		GEngine->AddOnScreenDebugMessage(2, 0.0f, ArrowColor, ArrowMsg);

		const TCHAR* RPMText = TEXT("???");
		FColor RPMColor = FColor::White;
		switch (RPMState)
		{
		case EHandSpeedState::Good:		RPMText = TEXT("OK");	RPMColor = FColor::Green;	break;
		case EHandSpeedState::TooSlow:	RPMText = TEXT("SLOW");	RPMColor = FColor::Yellow;	break;
		case EHandSpeedState::TooFast:	RPMText = TEXT("FAST");	RPMColor = FColor::Red;		break;
		}

		const FString RPMMsg = FString::Printf(
			TEXT("[RPM] %.1f [%s] (%.0f+/-%.0f) %s"),
			CurrentRPM, RPMText, TargetRPM, RPMTolerance,
			bReelUnlocked ? TEXT("") : TEXT("| LOCKED"));

		GEngine->AddOnScreenDebugMessage(3, 0.0f, RPMColor, RPMMsg);

		const FColor ScoreColor = FinalScore >= 80.0f ? FColor::Green : (FinalScore >= 50.0f ? FColor::Yellow : FColor::Red);
		const FString ScoreMsg = FString::Printf(
			TEXT("[Score] Final: %.0f / 100 | Live: %.0f"),
			FinalScore, CurrentScore);
		GEngine->AddOnScreenDebugMessage(4, 0.0f, ScoreColor, ScoreMsg);
	}
}

// 2026.09.07 Lee startーーー 破棄時のタイマー解除とデリゲート購読解除 ーーー
void UFishFightMeterWidget::NativeDestruct()
{
	// ステップバー自動隠蔽タイマーが破棄後に発火しないよう解除する
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepBarHideTimerHandle);
	}

	// 動的デリゲートはオブジェクトを GC から保護するため、破棄時に必ず解除する
	// （RpmGaugeWidget と同じ方式。解除漏れがあると Widget 破棄後も古いインスタンスが保持され続ける）
	if (HandUpDownState)
	{
		HandUpDownState->OnFishingStateCompleted.RemoveDynamic(this, &UFishFightMeterWidget::OnHandUpDownCompleted);
	}
	if (ReelSimulator)
	{
		ReelSimulator->OnRPMCalculated.RemoveDynamic(this, &UFishFightMeterWidget::OnRPMUpdated);
	}
	if (StateManager)
	{
		StateManager->OnFishingStateChanged.RemoveDynamic(this, &UFishFightMeterWidget::HandleFishingStateChanged);
	}

	Super::NativeDestruct();
}
// 2026.09.07 Lee endーーー

/** @brief OnRPMCalculated（1 回転ごと）受信ハンドラ。実判定と同じ閾値で RPM 判定表示を更新する */
void UFishFightMeterWidget::OnRPMUpdated(float NewRPM)
{
	// 規定回数完了まで RPM 入力を受け付けない
	if (!bReelUnlocked)
	{
		// 2026.08.20 Lee：ロック中に RPM が届いた=リール入力自体は生きているが表示だけロック中。
		// 切り分け用に 1 秒に 1 回だけ警告を出す（毎回出すとログが流れるため）
		static double LastLockedWarnTime = 0.0;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastLockedWarnTime >= 1.0)
		{
			LastLockedWarnTime = Now;
			UE_LOG(LogFishing, Warning, TEXT("[FightMeter] RPM 受信をロック中に破棄: RPM=%.1f (Phase=%d)"), NewRPM, static_cast<int32>(CurrentPhase));
		}
		return;
	}

	CurrentRPM = NewRPM;

	// ゲーム側判定（JudgeRPM）と同じ閾値を ReelState から読み取り、同一区間 [Min, Max] で分類する。
	// 閾値は負荷プリセット（ApplyRotationLoadLevel）で変わるため、回転ごとに最新値を読み取る。
	float MinRPM = 0.0f;
	float WheelMaxRPM = 0.0f;
	float StickMaxRPM = 0.0f;
	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// ホイール専用の下限を受け取る（判定区間の下端に使用）
	float WheelMinRPM = 0.0f;
	if (ReelSimulator && LeeReelRpm::ReadReelRPMThresholds(ReelSimulator, MinRPM, WheelMaxRPM, StickMaxRPM, WheelMinRPM))
	//if (ReelSimulator && LeeReelRpm::ReadReelRPMThresholds(ReelSimulator, MinRPM, WheelMaxRPM, StickMaxRPM))
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	{
		// 上限は「判定（JudgeRPM）が実際に使った値」を最優先する。未入力の間のみデバイス予測へ
		// フォールバック（非 VR 実行＋自転車デバイスのように、入力と予測が食い違う状況で判定と一致させるため）
		const float MaxRPM = LeeReelRpm::ResolveJudgedMaxAllowedRPM(ReelSimulator, WheelMaxRPM, StickMaxRPM);
		// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// 下限も同じ理由で判定側の実値を最優先する
		const float JudgedMinRPM = LeeReelRpm::ResolveJudgedMinAllowedRPM(ReelSimulator, MinRPM, WheelMinRPM);
		// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー

		// 分類は共通実装へ一本化（JudgeRPM と同一の区間・優先順）
		// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
		RPMState = LeeReelRpm::ClassifyRPM(JudgedMinRPM, MaxRPM, NewRPM);
		//RPMState = LeeReelRpm::ClassifyRPM(MinRPM, MaxRPM, NewRPM);
		// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー

		// デバッグ表示と BP 向けに、判定区間を中心＋半幅の形式で表示値へ反映する
		// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
		TargetRPM = (JudgedMinRPM + MaxRPM) * 0.5f;
		RPMTolerance = (MaxRPM - JudgedMinRPM) * 0.5f;
		//TargetRPM = (MinRPM + MaxRPM) * 0.5f;
		//RPMTolerance = (MaxRPM - MinRPM) * 0.5f;
		// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	}
	else
	{
		// フォールバック：ReelState の閾値が読み取れない場合はデザイナー設定値で分類（旧挙動）
		RPMState = LeeReelRpm::ClassifyRPM(TargetRPM - RPMTolerance, TargetRPM + RPMTolerance, NewRPM);
	}

	OnRPMChanged(CurrentRPM, RPMState);
}

/** @brief 上下運動完了受信ハンドラ。失敗時は表示をリセットし、成功時は最終表示へ反映する */
void UFishFightMeterWidget::OnHandUpDownCompleted(bool bIsSuccess)
{
	// 2026.08.20 Lee：bReelUnlocked の切り替えは HandleFishingStateChanged の状態駆動に一本化した。
	// 旧実装（ここで解除 + NativeTick で再ロック検出）はイベント順序と Widget 生成タイミングに
	// 依存し、2セット目以降にロックが外れない現象の温床になっていた。
	UE_LOG(LogFishing, Log, TEXT("[FightMeter] OnHandUpDownCompleted: bIsSuccess=%d"), bIsSuccess ? 1 : 0);

	if (!bIsSuccess)
	{
		// 失敗（過速・過遅）時は表示をリセットする
		CycleCount = 0;
		FinalScore = 0;
		return;
	}

	// 表示を 5/5 等に更新（コンポーネントの CurrentUpAndDownCount は既に目標値に達している）
	if (HandUpDownState)
	{
		CycleCount = HandUpDownState->CurrentUpAndDownCount;
		FinalScore = HandUpDownState->FinalScore;
	}

	OnScoreChanged(FinalScore);
}

/** @brief 状態遷移通知ハンドラ。フェーズ表示と RPM ロックをここで一括更新する */
void UFishFightMeterWidget::HandleFishingStateChanged(UFishingStateComponentBase* NewState)
{
	if (!NewState)
	{
		return;
	}

	const EFishingPhase NewPhase = ResolvePhase(NewState);

	// RPM 表示のロック/解放はステート遷移から直接導出する（2026.08.20 Lee）。
	// Reel フェーズに入った瞬間に解放、それ以外のフェーズでは必ずロック。
	// 旧実装（HandUpDown 完了イベントで解除＋Tick で再ロック検出）はイベント順序と
	// Widget の生成タイミングに依存し、2セット目以降のロック残留の原因になり得たため廃止。
	const bool bNewReelUnlocked = (NewPhase == EFishingPhase::Reel);
	if (bNewReelUnlocked != bReelUnlocked)
	{
		UE_LOG(LogFishing, Log, TEXT("[FightMeter] RPM 表示ロック切替: %d → %d (Phase=%d)"),
			bReelUnlocked ? 1 : 0, bNewReelUnlocked ? 1 : 0, static_cast<int32>(NewPhase));
	}
	bReelUnlocked = bNewReelUnlocked;

	// 中間フェーズを飛ぶ遷移（例：リール失敗→結果）を汎用に検出
	const bool bSkipped = static_cast<int32>(NewPhase) > static_cast<int32>(PreviousPhase) + 1;
	ApplyPhase(NewPhase, NewState->GetStateDisplayName(), bSkipped);
}

EFishingPhase UFishFightMeterWidget::ResolvePhase(const UFishingStateComponentBase* State) const
{
	// ポインタ比較で各ステートコンポーネントを識別（HandUpDown／Reel は既存メンバを流用）
	if (State == HandUpDownState)
	{
		return EFishingPhase::HandUpDown;
	}
	if (State == ReelSimulator)
	{
		return EFishingPhase::Reel;
	}
	if (State == CatchingState)
	{
		return EFishingPhase::Catching;
	}
	if (State == ResultState)
	{
		return EFishingPhase::Result;
	}
	// 待機ステートまたは未知のステート
	return EFishingPhase::Ready;
}

void UFishFightMeterWidget::ApplyPhase(EFishingPhase NewPhase, const FString& PhaseName, bool bSkipped)
{
	PreviousPhase     = NewPhase;
	CurrentPhase      = NewPhase;
	CurrentPhaseIndex = static_cast<int32>(NewPhase);
	CurrentPhaseName  = PhaseName;
	bPhaseSkipped     = bSkipped;

	// BP イベント発火（OnArrowUpdated と同じプッシュ方式）
	OnPhaseChanged(CurrentPhase, CurrentPhaseName, bPhaseSkipped);

	// 2026.09.07 Lee startーーー フェーズ連動の表示切替 ーーー
	// 矢印ガイド＝手上下フェーズ専用、RPM ゲージ一式＝リールフェーズ専用で表示する。
	// ApplyPhase は状態遷移イベントと NativeConstruct 初回同期の合流点のため、
	// ここで適用すればゲーム途中で生成された Widget でも正しい初期表示が得られる。
	ApplyPhaseVisibility(NewPhase == EFishingPhase::HandUpDown, NewPhase == EFishingPhase::Reel);

	// ステップバーは切替直後に表示し、StepBarDisplaySeconds 秒後に自動隠蔽する。
	// 同一ハンドルへの再 SetTimer はカウントダウンをリセットするため、
	// 短時間で連続遷移しても最後の切替から計時される。
	if (!bStepBarVisible)
	{
		bStepBarVisible = true;
		OnStepBarVisibilityChanged(true);
	}
	if (UWorld* World = GetWorld())
	{
		if (StepBarDisplaySeconds > 0.0f)
		{
			World->GetTimerManager().SetTimer(StepBarHideTimerHandle, this, &UFishFightMeterWidget::HideStepBar, StepBarDisplaySeconds, false);
		}
		else
		{
			// 0 以下＝常時表示モード（レイアウト調整用）。保留中の自動隠蔽があれば解除する
			World->GetTimerManager().ClearTimer(StepBarHideTimerHandle);
		}
	}
	// 2026.09.07 Lee endーーー

	// 2026.09.09 Lee startーーー ステップバー 3 状態の再適用 ーーー
	// ApplyPhase は状態遷移と NativeConstruct 初回同期の合流点のため、
	// ここで適用すればゲーム途中で生成された Widget でも正しい初期状態が得られる
	UpdateStepBarVisuals();
	// 2026.09.09 Lee endーーー

	// VR テスト用の画面デバッグ表示（VRPawn の [Fishing Mode] と同じ形式）
	if (GEngine)
	{
		const FString PhaseMsg = FString::Printf(
			TEXT("[Phase] %d/5 %s%s"),
			CurrentPhaseIndex + 1, *CurrentPhaseName,
			bPhaseSkipped ? TEXT(" (skipped)") : TEXT(""));
		GEngine->AddOnScreenDebugMessage(5, 3600.0f, FColor::Cyan, PhaseMsg);
	}
}

// 2026.09.07 Lee startーーー フェーズ連動表示切替 ーーー
void UFishFightMeterWidget::ApplyPhaseVisibility(bool bNewArrowGuideVisible, bool bNewRpmGaugeVisible)
{
	// 初回適用はデザイナー既定値に依存せず強制発火（途中生成でも正しい初期表示を保証）、以降は変化時のみ発火
	if (!bPhaseVisibilityApplied || bNewArrowGuideVisible != bArrowGuideVisible)
	{
		bArrowGuideVisible = bNewArrowGuideVisible;
		OnArrowGuideVisibilityChanged(bArrowGuideVisible);
	}

	// 2026.09.07 Lee startーーー 推奨範囲表示 ーーー
	// 表示へ切替った直後に現在値を強制同期する
	// （帯位置のデザイナー既定値チラつき防止と、前セットで赤くなったフィル色の残留防止）
	if (bNewArrowGuideVisible)
	{
		PushArrowRange();
		UpdateHandRangeColor(true);
	}
	// 2026.09.07 Lee endーーー
	if (!bPhaseVisibilityApplied || bNewRpmGaugeVisible != bRpmGaugeVisible)
	{
		bRpmGaugeVisible = bNewRpmGaugeVisible;
		OnRpmGaugeVisibilityChanged(bRpmGaugeVisible);
	}
	bPhaseVisibilityApplied = true;
}

void UFishFightMeterWidget::HideStepBar()
{
	if (bStepBarVisible)
	{
		bStepBarVisible = false;
		OnStepBarVisibilityChanged(false);

		// 2026.09.09 Lee startーーー 魚アイコンもバーと一緒に隠す ーーー
		// 魚アイコンは PhasePanel の外（CanvasPanel 直下）にいるため、
		// PhasePanel 側の隠蔽とは連動しない。ここで明示的に隠す
		if (Image_PhaseFish)
		{
			Image_PhaseFish->SetVisibility(ESlateVisibility::Collapsed);
		}
		// 2026.09.09 Lee endーーー
	}
}
// 2026.09.07 Lee endーーー

// 2026.09.07 Lee startーーー 推奨範囲表示 ーーー

/**
 * @brief 矢印位置を中心に評点閾値と同幅の帯区間を算出して BP へ通知する。
 * @note 正規化値（0.0～1.0）のみを渡す。WBP 側は GetCachedGeometry で軌道の実寸を
 *       動的に取得して描画するため、ここにデザイナー座標の定数は存在しない
 *       （WBP のレイアウト変更に C++ を追従させる必要がない二重管理防止）。
 */
void UFishFightMeterWidget::PushArrowRange()
{
	if (!HandUpDownState)
	{
		return;
	}

	// 矢印位置を中心に、評点閾値と同一の幅の帯を 2 重に算出する
	const float Center = HandUpDownState->ArrowPosition;
	const float GoodHalf    = HandUpDownState->ScoringFailThreshold;    // 有得帯（誤差これ以下で減点なし）
	const float PerfectHalf = HandUpDownState->ScoringPerfectThreshold; // 満点帯

	// 軌道（0.0～1.0）からはみ出す分はクランプ（BP 側の描画計算を単純化し、帯が軌道外へ出ないようにする）
	const float GoodBottom    = FMath::Clamp(Center - GoodHalf,    0.0f, 1.0f);
	const float GoodTop       = FMath::Clamp(Center + GoodHalf,    0.0f, 1.0f);
	const float PerfectBottom = FMath::Clamp(Center - PerfectHalf, 0.0f, 1.0f);
	const float PerfectTop    = FMath::Clamp(Center + PerfectHalf, 0.0f, 1.0f);

	OnArrowRangeUpdated(GoodBottom, GoodTop, PerfectBottom, PerfectTop);
}

void UFishFightMeterWidget::UpdateHandRangeColor(bool bForce)
{
	if (!HandHeightDetector || !HandUpDownState)
	{
		return;
	}

	// 待機位相の評点判定（|手 - 矢印| と ScoringFailThreshold の比較）と同一式で帯内外を判定する
	const float Hand = HandHeightDetector->HandHeightPercent;
	const bool bInRange = FMath::Abs(Hand - HandUpDownState->ArrowPosition) <= HandUpDownState->ScoringFailThreshold;

	if (bForce || bInRange != bHandInRange)
	{
		bHandInRange = bInRange;
		OnHandRangeColorChanged(bInRange ? HandRangeNormalColor : HandRangeWarningColor);
	}
}
// 2026.09.07 Lee endーーー

// 2026.09.09 Lee startーーー ステップバー 3 状態表示＋魚アイコン指示器 ーーー

void UFishFightMeterWidget::UpdateStepBarVisuals()
{
	UImage* Dots[4] = { Dot_1, Dot_2, Dot_3, Dot_4 };
	UImage* Lines[3] = { Line_1, Line_2, Line_3 };

	// バー本体が無い場合は何もしない（WBP 側未更新時のフォールバック）
	if (!Dots[0] || !Dots[1] || !Dots[2] || !Dots[3])
	{
		return;
	}

	// デザイナー設定ブラシを初回のみ退避する（未到達态で差し替えた後、完了态で復元するため）
	if (!bOriginalBrushCaptured)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			OriginalDotBrushes[i] = Dots[i]->GetBrush();
		}
		bOriginalBrushCaptured = true;
	}

	// 表示インデックス: enum 順＝ステップ順。Result(4) は最終段（3）へ丸め込む
	const int32 CurrentIdx = FMath::Min(static_cast<int32>(CurrentPhase), 3);
	const bool bUseFishIcon = (PhaseFishTexture != nullptr);

	for (int32 i = 0; i < 4; ++i)
	{
		UImage* Dot = Dots[i];
		if (i < CurrentIdx || (i == CurrentIdx && !bUseFishIcon))
		{
			// 完了（貼り付けTexture 未設定時は現在ドットもこの外観で代替）:
			// 元ブラシ（四角）＋等倍＋有得色
			Dot->SetVisibility(ESlateVisibility::Visible);
			Dot->SetBrush(OriginalDotBrushes[i]);
			Dot->SetRenderScale(FVector2D::UnitVector);
			Dot->SetColorAndOpacity(StepCompletedColor);
		}
		else if (i == CurrentIdx)
		{
			// 現在: 魚アイコンと置き換わるため透明で表示する
			// （Hidden/Collapsed にすると描画経路から外れ GetCachedGeometry が更新されず、
			//   魚アイコンの位置計算が破綻する。可視のまま完全透明＝幾何だけ生かす）
			Dot->SetVisibility(ESlateVisibility::Visible);
			Dot->SetBrush(OriginalDotBrushes[i]);
			Dot->SetRenderScale(FVector2D::UnitVector);
			Dot->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f));
		}
		else
		{
			// 未到達: 円ブラシへ差し替え＋縮小（RenderScale の原点はコントロール中心）
			Dot->SetVisibility(ESlateVisibility::Visible);
			Dot->SetBrush(UnreachedDotBrush);
			Dot->SetRenderScale(FVector2D(UnreachedDotScale, UnreachedDotScale));
			Dot->SetColorAndOpacity(StepUnreachedColor);
		}
	}

	// 接続線: Line i（0 始まり）は Dot i と Dot i+1 を結ぶ。現在段に届いた線のみ有得色
	for (int32 i = 0; i < 3; ++i)
	{
		if (Lines[i])
		{
			Lines[i]->SetColorAndOpacity(i < CurrentIdx ? StepLineActiveColor : StepLineInactiveColor);
		}
	}

	// 魚アイコン: 貼り付けTexture がありバー表示中のみ表示（Texture 未設定は常に非表示）
	if (Image_PhaseFish)
	{
		const bool bFishVisible = bUseFishIcon && bStepBarVisible;
		Image_PhaseFish->SetVisibility(bFishVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UFishFightMeterWidget::UpdateStepFishPosition(float DeltaTime)
{
	// 非表示中・Texture 未設定は位置更新も不要
	if (!Image_PhaseFish || !PhaseFishTexture ||
		Image_PhaseFish->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	UImage* Dots[4] = { Dot_1, Dot_2, Dot_3, Dot_4 };
	const int32 CurrentIdx = FMath::Min(static_cast<int32>(CurrentPhase), 3);
	UImage* CurrentDot = Dots[CurrentIdx];
	if (!CurrentDot)
	{
		return;
	}

	// 現在ドットの中心を自 Widget ローカル座標へ変換（DPI スケールは AbsoluteToLocal が吸収する）
	const FGeometry DotGeo = CurrentDot->GetCachedGeometry();
	if (DotGeo.GetLocalSize().IsNearlyZero())
	{
		return; // レイアウト未確定（初フレーム等）は前回位置を維持
	}
	const FVector2D DotCenterLocal = GetCachedGeometry().AbsoluteToLocal(DotGeo.GetAbsolutePosition())
		+ DotGeo.GetLocalSize() * 0.5f;
	// 魚アイコンの実描画寸法はスロットから取る（アスペクト比維持により正方形とは限らないため）
	FVector2D FishDrawSize(PhaseFishSize, PhaseFishSize);
	if (const UCanvasPanelSlot* FishSlot = Cast<UCanvasPanelSlot>(Image_PhaseFish->Slot))
	{
		FishDrawSize = FishSlot->GetSize();
	}
	StepFishTargetPos = DotCenterLocal - FishDrawSize * 0.5f;

	// 初回はスナップ、以降は補間で滑らかに追従する（フェーズ切替時は移動アニメになる）
	if (!bStepFishPosInitialized)
	{
		StepFishCurrentPos = StepFishTargetPos;
		bStepFishPosInitialized = true;
	}
	else
	{
		StepFishCurrentPos = FMath::Vector2DInterpTo(StepFishCurrentPos, StepFishTargetPos, DeltaTime, PhaseFishInterpSpeed);
	}
	Image_PhaseFish->SetRenderTranslation(StepFishCurrentPos);
}
// 2026.09.09 Lee endーーー
