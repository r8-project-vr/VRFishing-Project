// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingStateWait.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UFishingStateWait::UFishingStateWait()
{
    // ステート単体でのTickは無効化
    PrimaryComponentTick.bCanEverTick = false;

    CurrentWaitTime = 0.0f;
    bIsCompleted = false;
}

void UFishingStateWait::EnterState()
{
    Super::EnterState();

    // 変数初期化
    CurrentWaitTime = 0.0f;
    bIsCompleted = false;
}

void UFishingStateWait::UpdateState(float DeltaTime)
{
    Super::UpdateState(DeltaTime);

    // 処理完了済みなら判定を行わない
    if (bIsCompleted) {
        return;
    }

    const FVector HUDLocation = GetHUDLocation();
    const FVector HandLocation = GetRightHandLocation();

    // HUDより指定距離以上低い位置に手があるか判定
    const bool bIsHandLowered = (HUDLocation.Z - HandLocation.Z) >= RequiredDownDistance;

    if (bIsHandLowered) {
        // 条件を満たしている時間を累積
        CurrentWaitTime += DeltaTime;

        // 規定時間に達したら完了イベントを発火
        if (CurrentWaitTime >= RequiredWaitTime) {
            bIsCompleted = true;
            OnFishingStateCompleted.Broadcast(true);
        }
    }
    else {
        // 条件から外れたため累積時間をリセット
        CurrentWaitTime = 0.0f;
    }
}

void UFishingStateWait::ExitState()
{
    Super::ExitState();

    // 変数リセット
    CurrentWaitTime = 0.0f;
    bIsCompleted = false;
}

FVector UFishingStateWait::GetRightHandLocation() const
{
    // 所有アクターから右手の座標を取得
    if (AActor* Owner = GetOwner()) {
        return Owner->GetActorLocation();
    }
    return FVector::ZeroVector;
}

FVector UFishingStateWait::GetHUDLocation() const
{
    APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);

    // プレイヤーカメラの座標を取得
    if (CameraManager) {
        return CameraManager->GetCameraLocation();
    }

    // カメラ取得失敗時のフォールバック処理
    if (AActor* Owner = GetOwner()) {
        return Owner->GetActorLocation();
    }

    return FVector::ZeroVector;
}