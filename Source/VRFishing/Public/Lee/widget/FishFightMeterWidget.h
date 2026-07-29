// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "FishFightMeterWidget.generated.h"

class UFishingReelStateComponent;

/**
 * @brief 釣りアトラクト／リールフェーズの表示専用 Widget。
 * @note ゲームプレイロジック（矢印ガイド・スコアリング）は FishingStateHandUpDown が管理し、
 *       本 Widget は表示データの読み取りと BP イベント発火のみを行う。
 */
UCLASS(BlueprintType, Blueprintable)
class VRFISHING_API UFishFightMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFishFightMeterWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ==================== 表示設定（RPM） ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float TargetRPM = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float RPMTolerance = 10.0f;

	// ==================== 出力（読み取り専用） ====================

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

	UPROPERTY(BlueprintReadOnly, Category = "Meter|Scoring")
	float CurrentScore = 0.0f;

	/**
	 * @brief 全回数終了後の総合得点（全フレーム平均 × 100）
	 * @note FishingStateHandUpDown で計算され、本 Widget は表示のみ。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Scoring")
	float FinalScore = 0.0f;

	// ==================== BP イベント ====================

	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Arrow")
	void OnArrowUpdated(float Position, EFishArrowState State);

	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|RPM")
	void OnRPMChanged(float RPM, EHandSpeedState State);

	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Scoring")
	void OnScoreChanged(float Score);

private:
	UFUNCTION()
	void OnRPMUpdated(float NewRPM);

	UFUNCTION()
	void OnHandUpDownCompleted(bool bIsSuccess);

	UPROPERTY()
	TObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;

	UPROPERTY()
	TObjectPtr<UFishingStateHandUpDown> HandUpDownState;

	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelSimulator;

	bool bComponentsInitialized = false;
};
