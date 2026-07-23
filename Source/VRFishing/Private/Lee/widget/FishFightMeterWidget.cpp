// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/widget/FishFightMeterWidget.h"
#include "Tanimura/Component/ReelSimulatorComponent.h"
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
		ReelSimulator = OwnerPawn->FindComponentByClass<UReelSimulatorComponent>();

		if (ReelSimulator)
		{
			ReelSimulator->OnRPMCalculated.AddDynamic(this, &UFishFightMeterWidget::OnRPMUpdated);
		}

		bComponentsInitialized = true;
	}

	// 0 なら最初から解禁
	if (RequiredCycles <= 0)
	{
		bReelUnlocked = true;
	}

	// RPM 表示を初期化
	OnRPMChanged(0.0f, EHandSpeedState::TooSlow);
}

void UFishFightMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bComponentsInitialized)
	{
		return;
	}

	const float HandPercent = HandHeightDetector ? HandHeightDetector->HandHeightPercent : 0.0f;

	// ---- 手の往復カウント（矢印の位置とは無関係） ----
	if (!bReelUnlocked)
	{
		if (HandPercent >= (1.0f - ArrowWaitThreshold))
		{
			bHandReachedTop = true;
		}
		if (bHandReachedTop && HandPercent <= ArrowWaitThreshold)
		{
			bHandReachedTop = false;
			CycleCount++;
			if (CycleCount >= RequiredCycles)
			{
				bReelUnlocked = true;
			}
		}
	}

	TickArrow(InDeltaTime, HandPercent);
	OnArrowUpdated(ArrowPosition, ArrowState);

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

		const FString ArrowMsg = FString::Printf(
			TEXT("[Arrow] Pos:%.0f%% %s | Hand:%.0f%% | %d/%d %s"),
			ArrowPosition * 100.0f, ArrowStateText, HandPercent * 100.0f,
			CycleCount, RequiredCycles,
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
	}
}

void UFishFightMeterWidget::TickArrow(float InDeltaTime, float HandPercent)
{
	switch (ArrowState)
	{
	case EFishArrowState::MovingUp:
	{
		ArrowPosition += RecommendedSpeed * InDeltaTime;
		if (ArrowPosition >= 1.0f)
		{
			ArrowPosition = 1.0f;
			ArrowState = EFishArrowState::WaitingAtTop;
		}
		break;
	}

	case EFishArrowState::WaitingAtTop:
	{
		if (HandPercent >= (1.0f - ArrowWaitThreshold))
		{
			ArrowState = EFishArrowState::MovingDown;
		}
		break;
	}

	case EFishArrowState::MovingDown:
	{
		ArrowPosition -= RecommendedSpeed * InDeltaTime;
		if (ArrowPosition <= 0.0f)
		{
			ArrowPosition = 0.0f;
			ArrowState = EFishArrowState::WaitingAtBottom;

		}
		break;
	}

	case EFishArrowState::WaitingAtBottom:
	{
		if (HandPercent <= ArrowWaitThreshold)
		{
			ArrowState = EFishArrowState::MovingUp;
		}
		break;
	}
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
