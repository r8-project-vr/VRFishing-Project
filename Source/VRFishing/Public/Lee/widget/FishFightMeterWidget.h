// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "FishFightMeterWidget.generated.h"

class UFishingReelStateComponent;

UENUM(BlueprintType)
enum class EFishArrowState : uint8
{
	MovingUp		UMETA(DisplayName = "上昇中"),
	WaitingAtTop	UMETA(DisplayName = "上部で待機"),
	MovingDown		UMETA(DisplayName = "下降中"),
	WaitingAtBottom	UMETA(DisplayName = "下部で待機")
};

UCLASS(BlueprintType, Blueprintable)
class VRFISHING_API UFishFightMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFishFightMeterWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ==================== 設定 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Arrow")
	float RecommendedSpeed = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Arrow")
	float ArrowWaitThreshold = 0.20f;

	/** リール解禁に必要な矢印の往復回数（0 = 最初から解禁） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Arrow")
	int32 RequiredCycles = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float TargetRPM = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float RPMTolerance = 10.0f;

	// ==================== 出力 ====================

	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	float ArrowPosition = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	EFishArrowState ArrowState = EFishArrowState::MovingUp;

	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	int32 CycleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	bool bReelUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	float CurrentRPM = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	EHandSpeedState RPMState = EHandSpeedState::Good;

	// ==================== BP イベント ====================

	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Arrow")
	void OnArrowUpdated(float Position, EFishArrowState State);

	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|RPM")
	void OnRPMChanged(float RPM, EHandSpeedState State);

private:
	UFUNCTION()
	void OnRPMUpdated(float NewRPM);

	/** HandHeightDetector が目標回数に達したときのコールバック */
	UFUNCTION()
	void OnHandCyclesComplete();

	void TickArrow(float InDeltaTime, float HandPercent);

	UPROPERTY()
	TObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;

	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelSimulator;

	bool bComponentsInitialized = false;
};
