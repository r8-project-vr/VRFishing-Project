// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingStateWait.generated.h"

// 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
class UHandHeightDetectorComponent;
// 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

/**
 * ゲーム開始直後の「待機モード」
 * HUDを基準に、指定の高さ(初期値50cm)より低い位置で数秒(初期値2秒)待つと、次のモードへ
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingStateWait : public UFishingStateComponentBase
{
    GENERATED_BODY()

public:
    UFishingStateWait();

	// 基底クラスのメンバ関数をオーバーライド
    virtual void EnterState() override;                 // ステート開始時の処理
    virtual void UpdateState(float DeltaTime) override; // ステート更新時の処理
    virtual void ExitState() override;                  // ステート終了時の処理

protected:
    // 右手の現在位置を取得
    UFUNCTION(BlueprintCallable, Category = "Fishing|Wait")
    FVector GetRightHandLocation() const;

    // HUD/カメラの現在位置を取得
    UFUNCTION(BlueprintCallable, Category = "Fishing|Wait")
    FVector GetHUDLocation() const;

	// 準備完了（手を下ろしている）と判定する、HUDとの下向き距離 (cm)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fishing|Wait")
    float RequiredDownDistance = 50.0f;

	// 次のステートに進むために、準備完了状態でキープしなければいけない時間 (秒)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fishing|Wait")
    float RequiredWaitTime = 2.0f;

    // 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 手部運動センサへの参照（未取得時は所有者から FindComponentByClass で取得）
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Fishing|Wait")
    TWeakObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;
    // 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

private:
    // 条件を満たしている累積時間
    float CurrentWaitTime;

    // 完了フラグ
    bool bIsCompleted;
};