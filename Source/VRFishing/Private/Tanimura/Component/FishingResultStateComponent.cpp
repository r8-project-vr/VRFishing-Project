// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingResultStateComponent.h"

UFishingResultStateComponent::UFishingResultStateComponent()
{
    // ステート単体でのTickは無効化（Manager経由でUpdateStateが呼ばれる）
    PrimaryComponentTick.bCanEverTick = false;

    ElapsedTime = 0.0f;
    bIsCompleted = false;
}

void UFishingResultStateComponent::EnterState()
{
    Super::EnterState();

    // 経過時間と完了フラグを初期化
    ElapsedTime = 0.0f;
    bIsCompleted = false;
}

void UFishingResultStateComponent::UpdateState(float DeltaTime)
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
        OnFishingStateCompleted.Broadcast(bIsSuccess);
    }
}

void UFishingResultStateComponent::ExitState()
{
    Super::ExitState();

    // 変数リセット
    ElapsedTime = 0.0f;
    bIsCompleted = false;
}

void UFishingResultStateComponent::SetResult(bool bSuccess)
{
    // 釣りの成否を設定
    bIsSuccess = bSuccess;
}

bool UFishingResultStateComponent::IsSuccess() const
{
    // 釣りの成否を返す
    return bIsSuccess;
}

bool UFishingResultStateComponent::IsSuccessState() const
{
    // 釣り成功（bIsSuccess=true）なら true
    return bIsSuccess;
}
