// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Actor/VRPawn.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateWait.h"
#include "Lee/component//HandHeightDetectorComponent.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingCatchingStateComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

AVRPawn::AVRPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    // 各ステートコンポーネントおよびマネージャーの生成
    StateManagerComponent = CreateDefaultSubobject<UFishingStateManagerComponent>(TEXT("StateManagerComponent"));
    WaitStateComponent = CreateDefaultSubobject<UFishingStateWait>(TEXT("WaitStateComponent"));
    // 2026.07.27 Lee start
    HandUpDownComponent = CreateDefaultSubobject<UHandHeightDetectorComponent>(TEXT("HandUpDownComponent"));
    // 2026.07.27 Lee end
    ReelStateComponent = CreateDefaultSubobject<UFishingReelStateComponent>(TEXT("ReelStateComponent"));
    CatchingStateComponent = CreateDefaultSubobject<UFishingCatchingStateComponent>(TEXT("CatchingStateComponent"));
}

void AVRPawn::BeginPlay()
{
    Super::BeginPlay();

    // 待機ステートの完了イベントをバインド
    if (WaitStateComponent) {
        WaitStateComponent->OnFishingStateCompleted.AddDynamic(this, &AVRPawn::OnWaitStateCompleted);
    }

    // リールステートの目標回転数達成イベントをバインド
    if (ReelStateComponent) {
        ReelStateComponent->OnTargetRevolutionsReached.AddDynamic(this, &AVRPawn::OnReelTargetReached);
    }

    // 2026.07.27 Lee start
    // 手の上下運動ステートの完了イベントをバインド
    if (HandUpDownComponent) {
        HandUpDownComponent->OnFishingStateCompleted.AddDynamic(this, &AVRPawn::OnHandUpDownCompleted);
    }
    // 2026.07.27 Lee end

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
    if (bIsSuccess && StateManagerComponent && HandUpDownComponent) {
        StateManagerComponent->ChangeState(HandUpDownComponent);
    }
    // 2026.07.27 Lee end
}

void AVRPawn::OnReelTargetReached()
{
    // リール回転完了時に釣り上げステートへ遷移
    if (StateManagerComponent && CatchingStateComponent) {
        StateManagerComponent->ChangeState(CatchingStateComponent);
    }
}

void AVRPawn::OnCatchingStateCompleted(bool bIsSuccess)
{
    // 釣り上げ完了時にリザルトWidgetを前面表示
    if (CatchingResultWidgetClass) {
        UUserWidget* ResultWidget = CreateWidget<UUserWidget>(GetWorld(), CatchingResultWidgetClass);
        if (ResultWidget) {
            ResultWidget->AddToViewport(100);
        }
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