// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingStateManagerComponent.generated.h"

class UFishingStateComponentBase;

/**
 * ステートが変わったことを通知するデリゲート（型宣言）
 * @param NewState 新しくアクティブになったステート
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFishingStateChanged, UFishingStateComponentBase*, NewState);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UFishingStateManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFishingStateManagerComponent();

	// アクティブなステートの更新を実行
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * アクティブなステートを変更する
	 * @param NewState 次にアクティブにするステート
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|Manager")
	void ChangeState(UFishingStateComponentBase* NewState);

	// ステート変更を通知するデリゲートのインスタンス
	UPROPERTY(BlueprintAssignable, Category = "Fishing|Events")
	FOnFishingStateChanged OnFishingStateChanged;

	// 現在アクティブなステートを取得
	UFUNCTION(BlueprintPure, Category = "Fishing|Manager")
	UFishingStateComponentBase* GetCurrentState() const;

	// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 現在アクティブなステートの表示名を取得（UI表示用）
	UFUNCTION(BlueprintPure, Category = "Fishing|Manager")
	FString GetCurrentStateName() const;
	// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

public:
	// 現在アクティブなステートの参照（一時的に保持）
	UPROPERTY(BlueprintReadOnly, Transient)
	UFishingStateComponentBase* CurrentState;
};
