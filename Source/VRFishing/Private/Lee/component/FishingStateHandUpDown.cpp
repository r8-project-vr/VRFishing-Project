// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Lee/component/FishingStateHandUpDown.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "GameFramework/Actor.h"

UFishingStateHandUpDown::UFishingStateHandUpDown()
{
	// ステート単体でのTickは無効化（Manager経由でUpdateStateが呼ばれる）
	PrimaryComponentTick.bCanEverTick = false;
}

void UFishingStateHandUpDown::EnterState()
{
	Super::EnterState();

	// カウンターと状態フラグを初期化
	CurrentUpAndDownCount = 0;
	bIsHandAtTop = false;
	bIsCompleted = false;

	// センサを所有者から取得（未取得の場合のみ）
	if (!Detector.IsValid() && GetOwner())
	{
		Detector = GetOwner()->FindComponentByClass<UHandHeightDetectorComponent>();
	}
}

void UFishingStateHandUpDown::UpdateState(float DeltaTime)
{
	Super::UpdateState(DeltaTime);

	// 処理完了済みなら判定を行わない（二重発火防止）
	if (bIsCompleted)
	{
		return;
	}

	// センサが取得できていない場合は判定不可
	if (!Detector.IsValid())
	{
		return;
	}

	const float HandHeightPercent = Detector->HandHeightPercent;

	// ==================== 腕上下回数の判定 ====================

	if (!bIsHandAtTop && HandHeightPercent >= UpperThresholdPercent)
	{
		// 手が上端領域に到達
		bIsHandAtTop = true;
	}
	else if (bIsHandAtTop && HandHeightPercent <= LowerThresholdPercent)
	{
		// 上端に到達した状態から下端領域まで下がったため 1 回とカウント
		bIsHandAtTop = false;
		CurrentUpAndDownCount++;

		// 目標回数に達したらステート完了を通知
		if (CurrentUpAndDownCount >= TargetUpAndDownCount)
		{
			bIsCompleted = true;
			OnFishingStateCompleted.Broadcast(true);
		}
	}
}

void UFishingStateHandUpDown::ExitState()
{
	Super::ExitState();

	// 変数リセット
	CurrentUpAndDownCount = 0;
	bIsHandAtTop = false;
	bIsCompleted = false;
}
