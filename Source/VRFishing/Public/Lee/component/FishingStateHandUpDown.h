// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingStateHandUpDown.generated.h"

class UHandHeightDetectorComponent;

/**
 * @brief 手の上下運動ステート（モード２）。
 * @note 感知(HandHeightDetectorComponent)とプレイロジック(カウント)を分離した薄状態。
 *       毎フレームセンサの HandHeightPercent を読み、閾値で上下往復をカウントして目標回数到達で完了。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingStateHandUpDown : public UFishingStateComponentBase
{
	GENERATED_BODY()

public:
	UFishingStateHandUpDown();

	// 基底クラスのメンバ関数をオーバーライド
	virtual void EnterState() override;
	virtual void UpdateState(float DeltaTime) override;
	virtual void ExitState() override;

	// ==================== 設定パラメータ ====================

	/// @brief ヒットまでに必要な上げ下げの目標回数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Count")
	int32 TargetUpAndDownCount = 5;

	/// @brief 手を「上がった」と判定するパーセンテージ閾値 (0.0～1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Count", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float UpperThresholdPercent = 0.8f;

	/// @brief 手を「下がった」と判定するパーセンテージ閾値 (0.0～1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|HandUpDown|Count", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float LowerThresholdPercent = 0.2f;

	// ==================== 出力 ====================

	/// @brief 現在の上げ下げ達成回数
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|HandUpDown|Count")
	int32 CurrentUpAndDownCount = 0;

private:
	/// @brief 手部運動センサへの参照（EnterState で所有者から取得）
	TWeakObjectPtr<UHandHeightDetectorComponent> Detector;

	/** 手が現在「上位置」に達しているかのフラグ */
	bool bIsHandAtTop = false;

	/** 完了フラグ（OnFishingStateCompleted の二重発火防止） */
	bool bIsCompleted = false;
};
