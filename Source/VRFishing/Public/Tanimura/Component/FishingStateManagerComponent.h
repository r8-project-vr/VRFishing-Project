// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingStateManagerComponent.generated.h"

class UFishingStateComponentBase;

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

protected:
	virtual void BeginPlay() override;

public:	
	// 現在アクティブなステートの参照（一時的に保持）
	UPROPERTY(Transient)
	UFishingStateComponentBase* CurrentState;
};
