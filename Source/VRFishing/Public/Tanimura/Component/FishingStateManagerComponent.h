// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingStateManagerComponent.generated.h"

class UFishingStateComponentBase;

// ステート変更時に発火するデリゲート型宣言
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFishingStateChanged, UFishingStateComponentBase*, NewState);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UFishingStateManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFishingStateManagerComponent();

	// アクティブなステートの更新を実行
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// アクティブなステートを変更
	UFUNCTION(BlueprintCallable, Category = "Fishing|Manager")
	void ChangeState(UFishingStateComponentBase* NewState);

	// ステート変更を通知するデリゲートのインスタンス
	UPROPERTY(BlueprintAssignable, Category = "Fishing|Events")
	FOnFishingStateChanged OnFishingStateChanged;

public:	
	// 現在アクティブなステートの参照（一時的に保持）
	UPROPERTY(Transient)
	UFishingStateComponentBase* CurrentState;
};
