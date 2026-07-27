// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"

UFishingStateManagerComponent::UFishingStateManagerComponent()
{
	// 現在のステートにTickを供給するため有効化
	PrimaryComponentTick.bCanEverTick = true;
	CurrentState = nullptr;
}

void UFishingStateManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // アクティブなステートが存在する場合のみTick処理を委譲
    if (CurrentState) {
        CurrentState->UpdateState(DeltaTime);
    }
}

void UFishingStateManagerComponent::ChangeState(UFishingStateComponentBase* NewState)
{
    // 同一ステートへの重複遷移を防止
    if (CurrentState == NewState) {
        return;
    }

    // 既存ステートの終了処理と無効化
    if (CurrentState) {
        CurrentState->ExitState();
        CurrentState->Deactivate();
    }

    // 新規ステートの更新
    CurrentState = NewState;

    // 新規ステートの有効化と開始処理
    if (CurrentState) {
        CurrentState->Activate();
        CurrentState->EnterState();
    }

    // ステート変更を外部通知
    OnFishingStateChanged.Broadcast(CurrentState);
}