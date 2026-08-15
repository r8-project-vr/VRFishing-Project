// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingCaughtStateComponent.generated.h"

/**
 * 釣り上げ完了後のモードを管理するステートコンポーネント
 * アクティブ化から規定時間経過後に完了を通知する（仮処理）
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingCaughtStateComponent : public UFishingStateComponentBase
{
    GENERATED_BODY()

public:
    UFishingCaughtStateComponent();

    // 基底クラスオーバーライド
    virtual void EnterState() override;
    virtual void UpdateState(float DeltaTime) override;
    virtual void ExitState() override;

protected:
    // 完了とみなすまでの経過時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fishing|Caught")
    float RequiredHoldTime = 0.5f;

private:
    // ステート開始からの経過時間
    float ElapsedTime;

    // 完了フラグ
    bool bIsCompleted;
};
