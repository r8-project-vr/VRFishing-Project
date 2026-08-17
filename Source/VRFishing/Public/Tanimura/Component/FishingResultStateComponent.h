// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingResultStateComponent.generated.h"

/**
 * 釣りの結果（成功/失敗）を表示するモードを管理するステートコンポーネント
 * アクティブ化から規定時間経過後に、成否を付けて完了を通知する（仮処理）
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingResultStateComponent : public UFishingStateComponentBase
{
    GENERATED_BODY()

public:
    UFishingResultStateComponent();

    // 基底クラスオーバーライド
    virtual void EnterState() override;
    virtual void UpdateState(float DeltaTime) override;
    virtual void ExitState() override;

    // 基底クラスの「釣り成功」判定をオーバーライド
    virtual bool IsSuccessState() const override;

    // 釣りの成否（成功=true / 失敗=false）を設定する
    UFUNCTION(BlueprintCallable, Category = "Fishing|Result")
    void SetResult(bool bSuccess);

    // 釣りの成否を取得する
    UFUNCTION(BlueprintPure, Category = "Fishing|Result")
    bool IsSuccess() const;

protected:
    // 完了とみなすまでの経過時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fishing|Result")
    float RequiredHoldTime = 0.5f;

    // 釣りの成否（成功=true / 失敗=false）
    UPROPERTY(BlueprintReadOnly, Category = "Fishing|Result")
    bool bIsSuccess = true;

private:
    // ステート開始からの経過時間
    float ElapsedTime;

    // 完了フラグ
    bool bIsCompleted;
};
