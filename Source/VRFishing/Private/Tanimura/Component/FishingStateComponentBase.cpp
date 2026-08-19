// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Component/FishingStateComponentBase.h"

UFishingStateComponentBase::UFishingStateComponentBase()
{
    PrimaryComponentTick.bCanEverTick = false;

    // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // ステートコンポーネントは ChangeState() の明示的な Activate() まで非アクティブに保つ。
    // UActorComponent 既定（bAutoActivate=true）のままだと BeginPlay 時に全ステートが自動アクティブ化され、
    // タイトルメニュー等で IsActive() ガードが効かずリール回転が誤検知されるため無効化する。
    // ※ Activate()/Deactivate() の可否は bAutoActivate に依存しない
    //   （UActorComponent::ShouldActivate() は !IsActive() 判定のみ。UE5.8 エンジンソース確認済み）
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

bool UFishingStateComponentBase::IsSuccessState() const
{
    // 通常ステートは成功を表さない
    return false;
}