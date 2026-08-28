// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
// 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Lee/component/HandHeightDetectorComponent.h"
// 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

UFishingReadyStateComponent::UFishingReadyStateComponent()
{
    // ステート単体でのTickは無効化
    PrimaryComponentTick.bCanEverTick = false;

    CurrentWaitTime = 0.0f;
    bIsCompleted = false;
}

void UFishingReadyStateComponent::EnterState()
{
    Super::EnterState();

    // 変数初期化
    CurrentWaitTime = 0.0f;
    bIsCompleted = false;
}

void UFishingReadyStateComponent::UpdateState(float DeltaTime)
{
    Super::UpdateState(DeltaTime);

    // 処理完了済みなら判定を行わない
    if (bIsCompleted) {
        return;
    }

    // 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 手部運動センサから手の下がり量を取得して判定（従来の Actor 位置参照の誤判定を修正）
    if (!HandHeightDetector.IsValid() && GetOwner())
    {
        HandHeightDetector = GetOwner()->FindComponentByClass<UHandHeightDetectorComponent>();
    }
    const bool bIsHandLowered = HandHeightDetector.IsValid()
        && (HandHeightDetector->GetHandHeightBelowHeadCm() >= RequiredDownDistance);
    // 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    //const FVector HUDLocation = GetHUDLocation();
    //const FVector HandLocation = GetRightHandLocation();
    //// HUDより指定距離以上低い位置に手があるか判定
    //const bool bIsHandLowered = (HUDLocation.Z - HandLocation.Z) >= RequiredDownDistance;←もともとのコードも消さない！

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

void UFishingReadyStateComponent::ExitState()
{
    Super::ExitState();

    // 変数リセット
    CurrentWaitTime = 0.0f;
    bIsCompleted = false;
}

// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
FString UFishingReadyStateComponent::GetStateDisplayName() const
{
    return TEXT("うでさげ！");
}
// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

FVector UFishingReadyStateComponent::GetRightHandLocation() const
{
    // 所有アクターから右手の座標を取得
    if (AActor* Owner = GetOwner()) {
        return Owner->GetActorLocation();
    }
    return FVector::ZeroVector;
}

FVector UFishingReadyStateComponent::GetHUDLocation() const
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