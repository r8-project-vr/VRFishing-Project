// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/widget/FishFightMeterWidget.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "Tanimura/Component/FishingCatchingStateComponent.h"
#include "Tanimura/Component/FishingResultStateComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

UFishFightMeterWidget::UFishFightMeterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

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

	// RPM 表示を初期化
	OnRPMChanged(0.0f, EHandSpeedState::TooSlow);

	// フェーズ表示の初回同期：Widget がゲーム途中で生成されても現在フェーズを即時反映する
	if (StateManager && StateManager->GetCurrentState())
	{
		ApplyPhase(ResolvePhase(StateManager->GetCurrentState()), StateManager->GetCurrentStateName(), false);
	}
	else
	{
		ApplyPhase(EFishingPhase::Ready, TEXT("待機"), false);
	}
}

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

		// リロック検出：次の Attract フェーズでカウントが動き始めたら再ロック
		if (bReelUnlocked && CycleCount > 0)
		{
			bReelUnlocked = false;
		}
	}

	// ---- BP イベント発火 ----
	OnArrowUpdated(ArrowPosition, ArrowState);
	OnScoreChanged(CurrentScore);

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

void UFishFightMeterWidget::OnRPMUpdated(float NewRPM)
{
	// 規定回数完了まで RPM 入力を受け付けない
	if (!bReelUnlocked)
	{
		return;
	}

	CurrentRPM = NewRPM;

	if (NewRPM < TargetRPM - RPMTolerance)
	{
		RPMState = EHandSpeedState::TooSlow;
	}
	else if (NewRPM > TargetRPM + RPMTolerance)
	{
		RPMState = EHandSpeedState::TooFast;
	}
	else
	{
		RPMState = EHandSpeedState::Good;
	}

	OnRPMChanged(CurrentRPM, RPMState);
}

void UFishFightMeterWidget::OnHandUpDownCompleted(bool bIsSuccess)
{
	if (!bIsSuccess)
	{
		// 失敗（過速・過遅）時はリールを明示的にロックし、表示をリセットする
		bReelUnlocked = false;
		CycleCount = 0;
		FinalScore = 0;
		return;
	}

	bReelUnlocked = true;

	// 表示を 5/5 等に更新（コンポーネントの CurrentUpAndDownCount は既に目標値に達している）
	if (HandUpDownState)
	{
		CycleCount = HandUpDownState->CurrentUpAndDownCount;
		FinalScore = HandUpDownState->FinalScore;
	}

	OnScoreChanged(FinalScore);
}

void UFishFightMeterWidget::HandleFishingStateChanged(UFishingStateComponentBase* NewState)
{
	if (!NewState)
	{
		return;
	}

	const EFishingPhase NewPhase = ResolvePhase(NewState);
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
