// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "FishFightMeterWidget.generated.h"

class UFishingReelStateComponent;
class UFishingStateManagerComponent;
class UFishingStateComponentBase;
class UFishingReadyStateComponent;
class UFishingCatchingStateComponent;
class UFishingResultStateComponent;

/**
 * @brief 釣りの進行フェーズ（ステップバー表示用）を定義する列挙型。
 * @note 値の順序＝ステップバーの表示順（左→右）。各ステートコンポーネントと 1:1 対応。
 */
UENUM(BlueprintType)
enum class EFishingPhase : uint8
{
	Ready      UMETA(DisplayName = "待機"),
	HandUpDown UMETA(DisplayName = "上下運動"),
	Reel       UMETA(DisplayName = "リール"),
	Catching   UMETA(DisplayName = "釣り上げ"),
	Result     UMETA(DisplayName = "釣り上げ結果")
};

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

	/** 表示用の目標 RPM。実行時は ReelState の判定閾値から算出された値で上書きされる（閾値が読み取れない場合のみこの設定値を使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float TargetRPM = 30.0f;

	/** 表示用の許容誤差。実行時は ReelState の判定閾値から算出された値で上書きされる（閾値が読み取れない場合のみこの設定値を使用） */
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

	// ==================== 出力（フェーズ／ステップバー） ====================

	/// @brief 現在の釣りフェーズ（ステップバーの強調位置）
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	EFishingPhase CurrentPhase = EFishingPhase::Ready;

	/// @brief 現在フェーズのインデックス（0～4）。ステップバー描画の便宜用
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	int32 CurrentPhaseIndex = 0;

	/// @brief 現在フェーズの表示名（GetStateDisplayName() の戻り値をそのまま表示）
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	FString CurrentPhaseName;

	/// @brief 直前の遷移で中間フェーズを飛ばしたか（例：リール失敗→釣り上げ結果）
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	bool bPhaseSkipped = false;

	/**
	 * @brief フェーズ変化時に発火する BP イベント。
	 * @param NewPhase  新しいフェーズ
	 * @param PhaseName 新フェーズの表示名
	 * @param bSkipped  遷移で中間フェーズを飛ばした場合 true
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Phase")
	void OnPhaseChanged(EFishingPhase NewPhase, const FString& PhaseName, bool bSkipped);

private:
	UFUNCTION()
	void OnRPMUpdated(float NewRPM);

	UFUNCTION()
	void OnHandUpDownCompleted(bool bIsSuccess);

	/// @brief StateManager の状態変更通知ハンドラ（OnFishingStateChanged 受信）
	UFUNCTION()
	void HandleFishingStateChanged(UFishingStateComponentBase* NewState);

	/// @brief ステートコンポーネントを対応フェーズへ変換（ポインタ比較）
	EFishingPhase ResolvePhase(const UFishingStateComponentBase* State) const;

	/// @brief フェーズを適用して BP イベントを発火（イベント駆動と初回同期で共用）
	void ApplyPhase(EFishingPhase NewPhase, const FString& PhaseName, bool bSkipped);

	UPROPERTY()
	TObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;

	UPROPERTY()
	TObjectPtr<UFishingStateHandUpDown> HandUpDownState;

	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelSimulator;

	/// @brief ステートマネージャ（購読と現在ステート取得用）
	UPROPERTY()
	TObjectPtr<UFishingStateManagerComponent> StateManager;

	/// @brief 各フェーズのステートコンポーネント参照（HandUpDown／Reel は既存メンバを流用）
	UPROPERTY()
	TObjectPtr<UFishingReadyStateComponent> ReadyState;

	UPROPERTY()
	TObjectPtr<UFishingCatchingStateComponent> CatchingState;

	UPROPERTY()
	TObjectPtr<UFishingResultStateComponent> ResultState;

	bool bComponentsInitialized = false;

	/// @brief 一つ前のフェーズ（スキップ判定用）
	EFishingPhase PreviousPhase = EFishingPhase::Ready;
};
