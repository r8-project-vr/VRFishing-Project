// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/widget/FishFightMeterWidget.h"
#include "Tanimura/Component/ReelSimulatorComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

UFishFightMeterWidget::UFishFightMeterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFishFightMeterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Owner Pawn からコンポーネントを自動取得
	APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!OwnerPawn)
	{
		// GetOwningPlayerPawn が null の場合、PlayerController 経由で再取得を試みる
		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			OwnerPawn = PC->GetPawn();
		}
	}

	if (OwnerPawn)
	{
		HandHeightDetector = OwnerPawn->FindComponentByClass<UHandHeightDetectorComponent>();
		ReelSimulator = OwnerPawn->FindComponentByClass<UReelSimulatorComponent>();

		// OnRPMCalculated デリゲートにバインド
		if (ReelSimulator)
		{
			ReelSimulator->OnRPMCalculated.AddDynamic(this, &UFishFightMeterWidget::OnRPMUpdated);
		}

		bComponentsInitialized = true;
	}
}

void UFishFightMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bComponentsInitialized)
	{
		return;
	}

	// 手の高さを取得
	const float HandPercent = HandHeightDetector ? HandHeightDetector->HandHeightPercent : 0.0f;

	// 矢印の状態遷移
	TickArrow(InDeltaTime, HandPercent);

	// BP に矢印の状態を通知
	OnArrowUpdated(ArrowPosition, ArrowState);

	// ==================== Debug 表示 ====================
	if (GEngine)
	{
		const TCHAR* ArrowStateText = TEXT("???");
		FColor ArrowColor = FColor::White;
		switch (ArrowState)
		{
		case EFishArrowState::MovingUp:			ArrowStateText = TEXT("↑ 上昇中");		ArrowColor = FColor::Cyan;		break;
		case EFishArrowState::WaitingAtTop:		ArrowStateText = TEXT("== 上部待機");	ArrowColor = FColor::Yellow;	break;
		case EFishArrowState::MovingDown:		ArrowStateText = TEXT("↓ 下降中");		ArrowColor = FColor::Orange;	break;
		case EFishArrowState::WaitingAtBottom:	ArrowStateText = TEXT("== 下部待機");	ArrowColor = FColor::Yellow;	break;
		}

		const FString ArrowMsg = FString::Printf(
			TEXT("[Arrow] Pos: %.2f%% | %s | Hand: %.2f%%"),
			ArrowPosition * 100.0f, ArrowStateText, HandPercent * 100.0f);

		GEngine->AddOnScreenDebugMessage(2, 0.0f, ArrowColor, ArrowMsg);

		// ---- RPM 状態（最終値を毎フレーム表示し続ける） ----
		const TCHAR* RPMStateText = TEXT("???");
		FColor RPMColor = FColor::White;
		switch (RPMState)
		{
		case EHandSpeedState::Good:		RPMStateText = TEXT("Good");	RPMColor = FColor::Green;	break;
		case EHandSpeedState::TooSlow:	RPMStateText = TEXT("Too Slow");	RPMColor = FColor::Yellow;	break;
		case EHandSpeedState::TooFast:	RPMStateText = TEXT("Too Fast");	RPMColor = FColor::Red;		break;
		}

		const FString RPMMsg = FString::Printf(
			TEXT("[RPM] %.1f RPM [%s]  (Target: %.0f +/- %.0f)"),
			CurrentRPM, RPMStateText, TargetRPM, RPMTolerance);

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
		// プレイヤーの手が上部（100% 付近）に到達したら下降開始
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
		// プレイヤーの手が下部（0% 付近）に到達したら上昇開始
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
	CurrentRPM = NewRPM;

	// RPM の適正判定（30±10 RPM）
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

	// BP に RPM 状態変化を通知
	OnRPMChanged(CurrentRPM, RPMState);
}
