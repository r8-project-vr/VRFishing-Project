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

// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
FString UFishingStateComponentBase::GetStateDisplayName() const
{
    // 既定はクラス名を返す（各ステートで日本語表示名へオーバーライドする）
    return GetClass()->GetName();
}
// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー