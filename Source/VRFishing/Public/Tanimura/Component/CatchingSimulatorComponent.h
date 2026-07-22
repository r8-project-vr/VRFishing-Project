// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CatchingSimulatorComponent.generated.h"

// === 追加：釣り上げ完了（または失敗）を通知するデリゲート ===
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFishingCompleted);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UCatchingSimulatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCatchingSimulatorComponent();

	// === 追加：釣り完了イベント ===
	UPROPERTY(BlueprintAssignable, Category = "Fishing Events")
	FOnFishingCompleted OnFishingCompleted;

	// === 追加：右トリガーが押された際に呼び出す関数（BPやEnhanced Inputからバインド可能） ===
	UFUNCTION(BlueprintCallable, Category = "Catching Simulator")
	void OnRightTriggerPressed();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
