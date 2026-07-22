// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "FishFightMeterWidget.generated.h"

class UReelSimulatorComponent;

/**
 * 矢印の移動状態
 */
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
	// ==================== UUserWidget オーバーライド ====================

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ==================== 設定パラメータ ====================

	/**
	 * 矢印の推奨移動速度（0.0〜1.0 範囲/秒）
	 * 0.2 の場合、0%→100% まで約 5 秒
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Arrow")
	float RecommendedSpeed = 0.2f;

	/** 手が端点に到達したと判定する許容範囲（0.05 = ±5%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Arrow")
	float ArrowWaitThreshold = 0.05f;

	/** 目標 RPM */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float TargetRPM = 30.0f;

	/** RPM 適正範囲（±RPM）。TargetRPM ± この値の範囲内を Good と判定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float RPMTolerance = 10.0f;

	// ==================== 出力（Blueprint 読み取り専用） ====================

	/** 矢印の現在位置（0.0〜1.0）。ProgressBar の fill % に対応 */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	float ArrowPosition = 0.0f;

	/** 矢印の現在の状態 */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	EFishArrowState ArrowState = EFishArrowState::MovingUp;

	/** 最新の RPM */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	float CurrentRPM = 0.0f;

	/** RPM の適正状態（Good / TooSlow / TooFast） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	EHandSpeedState RPMState = EHandSpeedState::Good;

	// ==================== Blueprint イベント（BP でビジュアル更新を実装） ====================

	/**
	 * 矢印の位置・状態が更新されたときに毎 Tick 呼ばれる。
	 * BP 側で矢印 Image の位置更新に使う。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Arrow")
	void OnArrowUpdated(float Position, EFishArrowState State);

	/**
	 * RPM が更新されたときに呼ばれる。
	 * BP 側で RPM テキストや状態表示を更新する。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|RPM")
	void OnRPMChanged(float RPM, EHandSpeedState State);

private:
	// ==================== 内部処理 ====================

	/** OnRPMCalculated のコールバック */
	UFUNCTION()
	void OnRPMUpdated(float NewRPM);

	/** 矢印の状態遷移を進める */
	void TickArrow(float InDeltaTime, float HandPercent);

	// ==================== コンポーネント参照 ====================

	UPROPERTY()
	TObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;

	UPROPERTY()
	TObjectPtr<UReelSimulatorComponent> ReelSimulator;

	/** NativeConstruct でコンポーネント取得済みか */
	bool bComponentsInitialized = false;
};
