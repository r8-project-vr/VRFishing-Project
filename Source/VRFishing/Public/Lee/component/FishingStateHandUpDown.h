// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingStateHandUpDown.generated.h"

class UHandHeightDetectorComponent;

/**
 * @brief 矢印ガイドの状態を定義する列挙型。
 */
UENUM(BlueprintType)
enum class EFishArrowState : uint8
{
	MovingUp        UMETA(DisplayName = "上昇中"),
	WaitingAtTop    UMETA(DisplayName = "上部で待機"),
	MovingDown      UMETA(DisplayName = "下降中"),
	WaitingAtBottom UMETA(DisplayName = "下部で待機")
};

/**
 * @brief 手の上下運動ステート（モード２）。
 * @note 感知(HandHeightDetectorComponent)とプレイロジック(カウント)を分離した薄状態。
 *       毎フレームセンサの HandHeightPercent を読み、閾値で上下往復をカウントして目標回数到達で完了。
 * @note 矢印ガイド・スコアリングのプレイロジックもここで管理し、Widgetは表示のみ行う。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingStateHandUpDown : public UFishingStateComponentBase
{
	GENERATED_BODY()

public:
	/** @brief コンストラクタ。Tick は無効（StateManager 経由で UpdateState が呼ばれる） */
	UFishingStateHandUpDown();

	// 基底クラスのメンバ関数をオーバーライド
	virtual void EnterState() override;
	virtual void UpdateState(float DeltaTime) override;
	virtual void ExitState() override;

	/** @brief ステートの表示名（ログ・UI表示用） */
	virtual FString GetStateDisplayName() const override;

	// ==================== 設定パラメータ ====================

	// ---- 上下カウント ----

	/** @brief ヒットまでに必要な上げ下げの目標回数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Count", meta = (ClampMin = "1"))
	int32 TargetUpAndDownCount = 5;

	/** @brief 手を「上がった」と判定するパーセンテージ閾値 (0.0～1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Count", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float UpperThresholdPercent = 0.8f;

	/** @brief 手を「下がった」と判定するパーセンテージ閾値 (0.0～1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Count", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float LowerThresholdPercent = 0.2f;

	// ---- 矢印ガイド ----

	/** @brief 推奨手の速度（正規化空間 0.0〜1.0 を 1 秒あたりに移動する割合）。矢印ガイドの移動速度と速度スコアリングの推奨値で共用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Arrow")
	float RecommendedSpeed = 0.2f;

	/** @brief 矢印が上端/下端で手を待つ閾値（0.0～1.0、小さいほど敏感） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Arrow")
	float ArrowWaitThreshold = 0.20f;

	// ---- スコアリング ----

	/** @brief 手と矢印の位置誤差がこれ以下なら満点扱い (0.0～1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScoringPerfectThreshold = 0.05f;

	/** @brief 手と矢印の位置誤差がこれ以上なら0点扱い (0.0～1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScoringFailThreshold = 0.3f;

	/** @brief スコアの平滑化係数（大きいほど変動が速い） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Scoring", meta = (ClampMin = "0.1"))
	float ScoreSmoothingFactor = 5.0f;

	// ---- 失敗検知（過速・過遅） ----

	/** @brief 品質がこの値未満のフレームを失敗タイマーに加算する閾値 (0.0～1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Fail", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FailQualityThreshold = 0.2f;

	/** @brief 低品質がこの秒数連続したら失敗（回復でタイマーはリセット） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Fail", meta = (ClampMin = "0.1"))
	float FailTimeSeconds = 2.0f;

	// ==================== 出力 ====================

	/** @brief 現在の上げ下げ達成回数 */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Count")
	int32 CurrentUpAndDownCount = 0;

	/** @brief 矢印の現在位置（0.0～1.0） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Arrow")
	float ArrowPosition = 0.0f;

	/** @brief 矢印の現在状態 */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Arrow")
	EFishArrowState ArrowState = EFishArrowState::MovingUp;

	/** @brief 現在のライブスコア（平滑化後、0～100） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Scoring")
	float CurrentScore = 0.0f;

	/** @brief 最終総合スコア（全フレーム平均×100、完了時に確定） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Scoring")
	float FinalScore = 0.0f;

	/** @brief ステート完了フラグ（Widget表示用／二重発火防止） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Count")
	bool bIsCompleted = false;

	/** @brief 現在の低品質連続時間（UI警告用に公開） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Fail")
	float FailTimeAccumulated = 0.0f;

	/** @brief 失敗フラグ（bIsCompleted とは別に公開。過速・過遅による失敗で true） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Fail")
	bool bIsFailed = false;

private:
	/** @brief 手部運動センサへの参照（EnterState で所有者から取得） */
	TWeakObjectPtr<UHandHeightDetectorComponent> Detector;

	/** 手が現在「上位置」に達しているかのフラグ */
	bool bIsHandAtTop = false;

	/** スコアの累積値（全フレーム平均算出用） */
	float TotalQualitySum = 0.0f;

	/** 累積フレーム数 */
	int32 TotalFrameCount = 0;

	/** @brief 矢印状態を更新する */
	void TickArrow(float DeltaTime, float HandPercent);

	/**
	 * @brief 誤差（位置・速度）から品質スコアを算出する。
	 * @param Error 誤差（0.0〜1.0 尺度）
	 * @return 0.0〜1.0 の品質（perfect 以下で 1.0、fail 以上で 0.0、間は線形）
	 */
	float CalcMatchQuality(float Error) const;

	/**
	 * @brief 失敗処理：フラグ設定 → 最終スコア → ログ → 基底クラスの通知デリゲートで失敗(false)を通知。
	 * @note 遷移先はリスナー（VRPawn）側が決定する（Reelステートと同じ方式）。本コンポーネント自身は遷移を行わない。
	 */
	void HandleFailure();
};
