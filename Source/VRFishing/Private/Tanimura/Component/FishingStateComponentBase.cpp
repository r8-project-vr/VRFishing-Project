// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Component/FishingStateComponentBase.h"

UFishingStateComponentBase::UFishingStateComponentBase()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UFishingStateComponentBase::EnterState()
{
}

void UFishingStateComponentBase::UpdateState(float DeltaTime)
{
}

void UFishingStateComponentBase::ExitState()
{
}