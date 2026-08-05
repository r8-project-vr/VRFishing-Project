// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Actor/VRPawn.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Component/FishingStateWait.h"
#include "Lee/component//HandHeightDetectorComponent.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
// 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Lee/component/FishingStateHandUpDown.h"
// 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Tanimura/Component/FishingCatchingStateComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"

AVRPawn::AVRPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    // 各ステートコンポーネントおよびマネージャーの生成
    StateManagerComponent = CreateDefaultSubobject<UFishingStateManagerComponent>(TEXT("StateManagerComponent"));
    WaitStateComponent = CreateDefaultSubobject<UFishingStateWait>(TEXT("WaitStateComponent"));
    // 2026.07.27 Lee start
    HandUpDownComponent = CreateDefaultSubobject<UHandHeightDetectorComponent>(TEXT("HandUpDownComponent"));
    // 2026.07.27 Lee end
    // 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    HandUpDownStateComponent = CreateDefaultSubobject<UFishingStateHandUpDown>(TEXT("HandUpDownStateComponent"));
    // 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    ReelStateComponent = CreateDefaultSubobject<UFishingReelStateComponent>(TEXT("ReelStateComponent"));
    CatchingStateComponent = CreateDefaultSubobject<UFishingCatchingStateComponent>(TEXT("CatchingStateComponent"));
}

void AVRPawn::BeginPlay()
{
    Super::BeginPlay();

    // ステート変更イベントをバインド（初期ステート設定より前に登録する必要があります）
    if (StateManagerComponent) {
        StateManagerComponent->OnFishingStateChanged.AddUniqueDynamic(this, &AVRPawn::OnFishingStateChanged);
    }

    // 待機ステートの完了イベントをバインド
    if (WaitStateComponent) {
        WaitStateComponent->OnFishingStateCompleted.AddDynamic(this, &AVRPawn::OnWaitStateCompleted);
    }

    // リールステートの完了イベントをバインド
    if (ReelStateComponent) {
        ReelStateComponent->OnFishingStateCompleted.AddDynamic(this, &AVRPawn::OnReelStateCompleted);
    }

    // 2026.07.27 Lee start
    // 手の上下運動ステートの完了イベントをバインド
    //if (HandUpDownComponent) {
    //    HandUpDownComponent->OnFishingStateCompleted.AddDynamic(this, &AVRPawn::OnHandUpDownCompleted);
    //}
    // 2026.07.27 Lee end
    // 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // センサ(HandUpDownComponent)は常駐化して OnFishingStateCompleted を持たないため、
    // 完了イベントは上下運動プレイステート(HandUpDownStateComponent)へバインドする
    if (HandUpDownStateComponent) {
        HandUpDownStateComponent->OnFishingStateCompleted.AddDynamic(this, &AVRPawn::OnHandUpDownCompleted);
    }
    // 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    // 釣り上げステートの完了イベントをバインド
    if (CatchingStateComponent) {
        CatchingStateComponent->OnFishingStateCompleted.AddDynamic(this, &AVRPawn::OnCatchingStateCompleted);
    }

    // 初期状態として待機ステートを設定
    if (StateManagerComponent && WaitStateComponent) {
        StateManagerComponent->ChangeState(WaitStateComponent);
    }
}

void AVRPawn::InjectReelStickInput(FVector2D StickInput)
{
    // 現在のステートがリールステートである場合のみ入力を処理
    if (ReelStateComponent && ReelStateComponent->IsActive()) {
        ReelStateComponent->SimulateReelByStick(StickInput);
    }
}

void AVRPawn::InjectReelWheelInput()
{
    // 現在のステートがリールステートである場合のみ入力を処理
    if (ReelStateComponent && ReelStateComponent->IsActive()) {
        ReelStateComponent->SimulateReelByWheel();
    }
}

void AVRPawn::OnWaitStateCompleted(bool bIsSuccess)
{
    // 2026.07.27 Lee start
    // 旧: StateManagerComponent->ChangeState(ReelStateComponent);
    //if (bIsSuccess && StateManagerComponent && HandUpDownComponent) {
    //    StateManagerComponent->ChangeState(HandUpDownComponent);
    //}
    // 2026.07.27 Lee end
    // 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 待機完了後は上下運動プレイステートへ遷移（センサではなくステート側へ）
    if (bIsSuccess && StateManagerComponent && HandUpDownStateComponent) {
        StateManagerComponent->ChangeState(HandUpDownStateComponent);
    }
    // 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
}

void AVRPawn::OnReelStateCompleted(bool bIsSuccess)
{
    // リール回転完了時に釣り上げステートへ遷移
    if (bIsSuccess && StateManagerComponent && CatchingStateComponent) {
        StateManagerComponent->ChangeState(CatchingStateComponent);
    }
}

void AVRPawn::OnCatchingStateCompleted(bool bIsSuccess)
{
    // [変更] 2D Viewportへの追加から、ワールド空間(WidgetComponent)での配置に変更
    if (!CatchingResultWidgetClass) {
        return;
    }

    // すでに生成されている場合は重複生成を避ける
    if (ResultWidgetComponent) {
        ResultWidgetComponent->SetVisibility(true);
        return;
    }

    // 動的に WidgetComponent を生成
    ResultWidgetComponent = NewObject<UWidgetComponent>(this, UWidgetComponent::StaticClass());
    if (ResultWidgetComponent) {
        ResultWidgetComponent->RegisterComponent();

        // 空間上の描画設定
        ResultWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
        ResultWidgetComponent->SetWidgetClass(CatchingResultWidgetClass);
        ResultWidgetComponent->SetDrawSize(ResultDrawSize);

        // プレイヤーの位置・回転に基づき、目の前のワールド座標へ配置
        const FVector SpawnLocation = GetActorLocation() + GetActorRotation().RotateVector(ResultUIOffset);

        // プレイヤーの方向（Yaw）を向くように回転を設定
        const FRotator SpawnRotation = FRotator(0.0f, GetActorRotation().Yaw + 180.0f, 0.0f);

        ResultWidgetComponent->SetWorldLocationAndRotation(SpawnLocation, SpawnRotation);
    }
}

// 2026.07.27 Lee start
void AVRPawn::OnHandUpDownCompleted(bool bIsSuccess)
{
    // 手の上下運動完了時にリールステートへ遷移
    if (bIsSuccess && StateManagerComponent && ReelStateComponent) {
        StateManagerComponent->ChangeState(ReelStateComponent);
    }
}
// 2026.07.27 Lee end

// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// ステート遷移ログは StateManagerComponent::ChangeState() へ一元化したため、
// 本ハンドラーでは画面表示のみ行う（表示名は GetStateDisplayName() を使用）
void AVRPawn::OnFishingStateChanged(UFishingStateComponentBase* NewState)
{
    if (!NewState) {
        return;
    }

    // 画面上に常時表示。
    // 0.0f は毎フレーム更新する用途のため、単発呼び出しだと次フレームで消える）
    if (GEngine) {
        const FString DisplayMessage = FString::Printf(TEXT("[Fishing Mode] Current State: %s"), *NewState->GetStateDisplayName());
        GEngine->AddOnScreenDebugMessage(10, 3600.0f, FColor::Cyan, DisplayMessage);
    }
}
// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 旧: クラス名を使用して5秒間表示
//const FString StateName = NewState->GetClass()->GetName();
//const FString DisplayMessage = FString::Printf(TEXT("[Fishing Mode] Current State: %s"), *StateName);
//UE_LOG(LogTemp, Log, TEXT("%s"), *DisplayMessage);
//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DisplayMessage);←もともとのコードも消さない！