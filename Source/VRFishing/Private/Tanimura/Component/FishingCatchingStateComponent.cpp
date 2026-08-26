// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingCatchingStateComponent.h"

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

	// 経過時間を加算
	ElapsedTime += DeltaTime;

	// 規定時間に達したら完了イベントを発火
	if (ElapsedTime >= RequiredHoldTime) {
		bIsCompleted = true;
		OnFishingStateCompleted.Broadcast(true);
	}
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
