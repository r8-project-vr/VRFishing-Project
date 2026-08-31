// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

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
 *       DisplayName は子ども向けのひらがな励まし表現（2026.08.26 Lee、GetStateDisplayName() と同期）。
 */
UENUM(BlueprintType)
enum class EFishingPhase : uint8
{
	Ready      UMETA(DisplayName = "よーい！"),
	HandUpDown UMETA(DisplayName = "うでをあげさげ！"),
	Reel       UMETA(DisplayName = "ぐるぐるまわして！"),
	Catching   UMETA(DisplayName = "うんとひっぱって！"),
	Result     UMETA(DisplayName = "つれたかな？")
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

	/** @brief 矢印ガイドの縦位置（0.0=下端 〜 1.0=上端。センサの HandHeightPercent と同じ刻み） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	float ArrowPosition = 0.0f;

	/** @brief 矢印の現在状態（上昇中/下降中/リール解放など） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	EFishArrowState ArrowState = EFishArrowState::MovingUp;

	/** @brief 完了した上下往復回数（TargetUpAndDownCount までの進捗表示用） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	int32 CycleCount = 0;

	/** @brief 上下運動が完了しリールフェーズへ移行済みか */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	bool bReelUnlocked = false;

	/** @brief 現在のリール回転速度（RPM） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	float CurrentRPM = 0.0f;

	/** @brief 現在 RPM の判定結果（遅すぎ/適速/速すぎ） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	EHandSpeedState RPMState = EHandSpeedState::Good;

	/** @brief 現在のスコア（リールフェーズ中のフレーム平均） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Scoring")
	float CurrentScore = 0.0f;

	/**
	 * @brief 全回数終了後の総合得点（全フレーム平均 × 100）
	 * @note FishingStateHandUpDown で計算され、本 Widget は表示のみ。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Scoring")
	float FinalScore = 0.0f;

	// ==================== BP イベント ====================

	/** @brief 矢印ガイド更新時に発火（矢印 Image の位置・状態を BP 側で反映する） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Arrow")
	void OnArrowUpdated(float Position, EFishArrowState State);

	/** @brief RPM 更新時に発火（数値表示と速度判定色を BP 側で反映する） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|RPM")
	void OnRPMChanged(float RPM, EHandSpeedState State);

	/** @brief スコア更新時に発火 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Scoring")
	void OnScoreChanged(float Score);

	// ==================== 出力（フェーズ／ステップバー） ====================

	/** @brief 現在の釣りフェーズ（ステップバーの強調位置） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	EFishingPhase CurrentPhase = EFishingPhase::Ready;

	/** @brief 現在フェーズのインデックス（0～4）。ステップバー描画の便宜用 */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	int32 CurrentPhaseIndex = 0;

	/** @brief 現在フェーズの表示名（GetStateDisplayName() の戻り値をそのまま表示） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	FString CurrentPhaseName;

	/** @brief 直前の遷移で中間フェーズを飛ばしたか（例：リール失敗→釣り上げ結果） */
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
	/** @brief ReelState の OnRPMCalculated 受信ハンドラ（RPM 表示の更新） */
	UFUNCTION()
	void OnRPMUpdated(float NewRPM);

	/** @brief HandUpDown 完了受信ハンドラ（bReelUnlocked の更新） */
	UFUNCTION()
	void OnHandUpDownCompleted(bool bIsSuccess);

	/** @brief StateManager の状態変更通知ハンドラ（OnFishingStateChanged 受信） */
	UFUNCTION()
	void HandleFishingStateChanged(UFishingStateComponentBase* NewState);

	/** @brief ステートコンポーネントを対応フェーズへ変換（ポインタ比較） */
	EFishingPhase ResolvePhase(const UFishingStateComponentBase* State) const;

	/** @brief フェーズを適用して BP イベントを発火（イベント駆動と初回同期で共用） */
	void ApplyPhase(EFishingPhase NewPhase, const FString& PhaseName, bool bSkipped);

	/** @brief 常駐センサ（HandHeightPercent 表示用。所有は Pawn、Widget は参照のみ） */
	UPROPERTY()
	TObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;

	/** @brief 上下運動ステート（矢印・スコア表示データの読み取り元） */
	UPROPERTY()
	TObjectPtr<UFishingStateHandUpDown> HandUpDownState;

	/** @brief リールステート（RPM 表示データと閾値の読み取り元） */
	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelSimulator;

	/** @brief ステートマネージャ（購読と現在ステート取得用） */
	UPROPERTY()
	TObjectPtr<UFishingStateManagerComponent> StateManager;

	/** @brief 各フェーズのステートコンポーネント参照（HandUpDown／Reel は既存メンバを流用） */
	UPROPERTY()
	TObjectPtr<UFishingReadyStateComponent> ReadyState;

	/** @brief 釣り上げステート（ResolvePhase のポインタ比較用） */
	UPROPERTY()
	TObjectPtr<UFishingCatchingStateComponent> CatchingState;

	/** @brief 結果ステート（ResolvePhase のポインタ比較用） */
	UPROPERTY()
	TObjectPtr<UFishingResultStateComponent> ResultState;

	/** @brief コンポーネント参照の解決済みフラグ（見つかるまで Tick で再試行する） */
	bool bComponentsInitialized = false;

	/** @brief 一つ前のフェーズ（スキップ判定用） */
	EFishingPhase PreviousPhase = EFishingPhase::Ready;
};
