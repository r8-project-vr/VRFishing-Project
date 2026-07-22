// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Component/CatchingSimulatorComponent.h"

// Sets default values for this component's properties
UCatchingSimulatorComponent::UCatchingSimulatorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCatchingSimulatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UCatchingSimulatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

// === 追加：右トリガー入力時の実行処理 ===
void UCatchingSimulatorComponent::OnRightTriggerPressed()
{
	// コンポーネントが有効（Catchingモード実行中）である場合のみ通知を発火
	if (IsActive()) {
		OnFishingCompleted.Broadcast();
	}
}