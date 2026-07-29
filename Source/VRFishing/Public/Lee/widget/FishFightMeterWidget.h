// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "FishFightMeterWidget.generated.h"

class UFishingReelStateComponent;
class UFishingStateHandUpDown;

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

	// ==================== スコアリング ====================

	/** 手と矢印の位置誤差がこれ以下なら満点扱い (0.0〜1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScoringPerfectThreshold = 0.05f;

	/** 手と矢印の位置誤差がこれ以上なら0点扱い (0.0〜1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScoringFailThreshold = 0.3f;

	/** スコアの平滑化係数（大きいほど変動が速い） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Scoring", meta = (ClampMin = "0.1"))
	float ScoreSmoothingFactor = 5.0f;

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

	UPROPERTY(BlueprintReadOnly, Category = "Meter|Scoring")
	float CurrentScore = 0.0f;

	// ================================================================
	// ★ FinalScore — 全回数終了後の総合得点（全フレーム平均 × 100）
	//   以降のフェーズ（Catching / リザルト表示 etc）で参照する
	// ================================================================
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

	/** HandHeightDetector が目標回数に達したときのコールバック */
	UFUNCTION()
	void OnHandUpDownCompleted(bool bIsSuccess);

	void TickArrow(float InDeltaTime, float HandPercent);

	UPROPERTY()
	TObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;

	/// @brief 上下運動プレイステート（カウント取得・完了イベント受信用）
	UPROPERTY()
	TObjectPtr<UFishingStateHandUpDown> HandUpDownState;

	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelSimulator;

	bool bComponentsInitialized = false;

	// スコアの累積値（全フレーム平均算出用）
	float TotalQualitySum = 0.0f;
	int32 TotalFrameCount = 0;
};
