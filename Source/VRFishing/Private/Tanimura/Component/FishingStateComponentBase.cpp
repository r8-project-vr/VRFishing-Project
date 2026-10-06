// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Component/FishingStateComponentBase.h"

UFishingStateComponentBase::UFishingStateComponentBase()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// ステートコンポーネントは ChangeState() の明示的な Activate() まで非アクティブに保つ。
	bAutoActivate = false;
	// 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
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

bool UFishingStateComponentBase::IsTimeCountingState() const
{
	// 既定では計時対象にしない（計時するステートはオーバーライドして true を返す）
	return false;
}