// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Component/FishingStateComponentBase.h"

UFishingStateComponentBase::UFishingStateComponentBase()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = false;
}


void UFishingStateComponentBase::BeginPlay()
{
	Super::BeginPlay();
}


void UFishingStateComponentBase::EnterState_Implementation()
{
	// コンポーネントを有効化
	Activate(true);
}

void UFishingStateComponentBase::UpdateState(float DeltaTime)
{

}

void UFishingStateComponentBase::ExitState_Implementation()
{
	// コンポーネントを無効化
	Deactivate();
}