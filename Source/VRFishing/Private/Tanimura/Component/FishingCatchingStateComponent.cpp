// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingCatchingStateComponent.h"

// 2026.08.31 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Lee/component/HandHeightDetectorComponent.h"
// 2026.08.31 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

UFishingCatchingStateComponent::UFishingCatchingStateComponent()
{
	// ステート単体でのTickは無効化（Manager経由でUpdateStateが呼ばれる）
	PrimaryComponentTick.bCanEverTick = false;

	ElapsedTime = 0.0f;
	bIsCompleted = false;
}

void UFishingCatchingStateComponent::EnterState()
{
	Super::EnterState();

	// 経過時間と完了フラグを初期化
	ElapsedTime = 0.0f;
	bIsCompleted = false;
}

void UFishingCatchingStateComponent::UpdateState(float DeltaTime)
{
	Super::UpdateState(DeltaTime);

	// 処理完了済みなら判定を行わない
	if (bIsCompleted) {
		return;
	}

	// 2026.08.31 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 手部運動センサを遅延解決（Ready と同じパターン）
	if (!HandHeightDetector.IsValid() && GetOwner())
	{
		HandHeightDetector = GetOwner()->FindComponentByClass<UHandHeightDetectorComponent>();
	}

	// 手が頭上 RequiredUpDistance cm を超えた瞬間に収竿完了（維持時間は要求しない）
	const bool bIsHandRaised = HandHeightDetector.IsValid()
		&& (HandHeightDetector->GetHandHeightBelowHeadCm() <= -RequiredUpDistance);

	if (bIsHandRaised) {
		bIsCompleted = true;
		OnFishingStateCompleted.Broadcast(true);
	}

	//// 旧・仮処理: 経過時間で自動成功（2026.08.31 Lee により収竿動作判定へ置換）
	//// 経過時間を加算
	//ElapsedTime += DeltaTime;

	//// 規定時間に達したら完了イベントを発火
	//if (ElapsedTime >= RequiredHoldTime) {
	//	bIsCompleted = true;
	//	OnFishingStateCompleted.Broadcast(true);
	//}
	// 2026.08.31 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
}

void UFishingCatchingStateComponent::ExitState()
{
	Super::ExitState();

	// 変数リセット
	ElapsedTime = 0.0f;
	bIsCompleted = false;
}

// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
FString UFishingCatchingStateComponent::GetStateDisplayName() const
{
	return TEXT("うんとひっぱって！");
}
// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
