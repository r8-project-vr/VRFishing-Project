// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "VRFishingLog.h"
// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

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

    // 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 遷移ログ用に新旧ステートの表示名を取得（CurrentState は下で置き換わるため先に取得）
    const FString OldStateName = CurrentState ? CurrentState->GetStateDisplayName() : TEXT("None");
    const FString NewStateName = NewState ? NewState->GetStateDisplayName() : TEXT("None");
    // 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

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

    // 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // ステート遷移をログ出力（どの経路から遷移しても必ず記録。購読者の有無に依存しない）
    UE_LOG(LogFishing, Log, TEXT("[FishingState] ChangeState: %s -> %s"), *OldStateName, *NewStateName);
    // 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    // ステート変更を外部通知
    OnFishingStateChanged.Broadcast(CurrentState);
}

// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
UFishingStateComponentBase* UFishingStateManagerComponent::GetCurrentState() const
{
    return CurrentState;
}

FString UFishingStateManagerComponent::GetCurrentStateName() const
{
    return CurrentState ? CurrentState->GetStateDisplayName() : TEXT("None");
}
// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー