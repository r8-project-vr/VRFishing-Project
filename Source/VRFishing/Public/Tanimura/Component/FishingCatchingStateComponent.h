// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingCatchingStateComponent.generated.h"

/**
 * 魚を釣り上げる動作を管理するステートコンポーネント
 * アクティブ化から規定時間経過後に完了を通知する（仮処理）
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingCatchingStateComponent : public UFishingStateComponentBase
{
	GENERATED_BODY()

public:
	UFishingCatchingStateComponent();

	// 基底クラスオーバーライド
	virtual void EnterState() override;
	virtual void UpdateState(float DeltaTime) override;
	virtual void ExitState() override;

protected:
	// 完了とみなすまでの経過時間（秒）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fishing|Catching")
	float RequiredHoldTime = 1.0f;

private:
	// ステート開始からの経過時間
	float ElapsedTime;

	// 完了フラグ
	bool bIsCompleted;
};
