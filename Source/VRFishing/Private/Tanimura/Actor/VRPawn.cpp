// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Actor/VRPawn.h"
#include "Lee/component//HandHeightDetectorComponent.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
//#include "Tanimura/Component/CatchingSimulatorComponent.h"

// === 追加：アタッチとスポーンに必要なヘッダーのインクルード ===
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"

AVRPawn::AVRPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    // === 追加：ソケット名の初期値設定 ===
    RodSocketName = FName("Socket_Rod");

    // コンポーネントの生成
    HandHeightDetectorComponent = CreateDefaultSubobject<UHandHeightDetectorComponent>(TEXT("HandHeightDetectorComponent"));
    ReelComponent = CreateDefaultSubobject<UFishingReelStateComponent>(TEXT("ReelComponent"));
    //CatchingComponent = CreateDefaultSubobject<UCatchingSimulatorComponent>(TEXT("CatchingComponent"));

    CurrentMode = EFishingMode::Attract;
}

void AVRPawn::BeginPlay()
{
    Super::BeginPlay();

    // === 追加：HandMeshComponent 未指定の場合、自身のアクターから自動取得 ===
    if (!HandMeshComponent) {
        HandMeshComponent = FindComponentByClass<USkeletalMeshComponent>();
    }

    // === 追加：ゲーム開始時に釣り竿を生成してアタッチ ===
    SpawnAndAttachFishingRod();

    // 1. 誘うモード：魚がヒットしたらリールモードへ
    if (HandHeightDetectorComponent) {
        HandHeightDetectorComponent->OnFishHit.AddDynamic(this, &AVRPawn::OnFishHit);
    }

    // 2. 巻くモード：規定回転数に達したらキャッチモードへ
    if (ReelComponent) {
        ReelComponent->OnTargetRevolutionsReached.AddDynamic(this, &AVRPawn::OnReelTargetReached);
    }

    // 3. 釣り上げるモード：完了/失敗したら誘うモードへ復帰
    //if (CatchingComponent) {
    //    CatchingComponent->OnFishingCompleted.AddDynamic(this, &AVRPawn::OnFishingCompleted);
    //}

    // 初期モード（誘うモード）に設定
    ChangeFishingMode(EFishingMode::Attract);
}

// === 追加：釣り竿の生成・アタッチ処理の実装 ===
void AVRPawn::SpawnAndAttachFishingRod()
{
    UWorld* World = GetWorld();
    if (!World || !FishingRodClass || !HandMeshComponent) {
        return;
    }

    // 二重生成の防止
    if (SpawnedFishingRod) {
        SpawnedFishingRod->Destroy();
    }

    // 釣り竿アクターのスポーン設定
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();

    SpawnedFishingRod = World->SpawnActor<AActor>(FishingRodClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

    if (SpawnedFishingRod) {
        // ソケットにぴったり合わせるトランスフォームルール
        FAttachmentTransformRules AttachmentRules(
            EAttachmentRule::SnapToTarget,
            EAttachmentRule::SnapToTarget,
            EAttachmentRule::KeepWorld,
            false
        );

        // 指定したソケットへアタッチ
        SpawnedFishingRod->AttachToComponent(HandMeshComponent, AttachmentRules, RodSocketName);
    }
}

void AVRPawn::ChangeFishingMode(EFishingMode NewMode)
{
    CurrentMode = NewMode;

    // すべてのコンポーネントを一旦停止（Deactivate）
    if (HandHeightDetectorComponent) {
        HandHeightDetectorComponent->Deactivate();
    }
    if (ReelComponent) {
        ReelComponent->Deactivate();
    }
    //if (CatchingComponent) {
    //    CatchingComponent->Deactivate();
    //}

    // 該当するコンポーネントのみ起動（Activate）
    switch (CurrentMode) {
    case EFishingMode::Attract:
        if (HandHeightDetectorComponent) {
            HandHeightDetectorComponent->Activate();
        }
        break;

    case EFishingMode::Reeling:
        if (ReelComponent) {
            ReelComponent->ResetRevolutionCount();
            ReelComponent->Activate();
        }
        break;

    //case EFishingMode::Catching:
    //    if (CatchingComponent) {
    //        CatchingComponent->Activate();
    //    }
    //    break;

    default:
        break;
    }
}

void AVRPawn::OnFishHit()
{
    ChangeFishingMode(EFishingMode::Reeling);
}

void AVRPawn::OnReelTargetReached()
{
    ChangeFishingMode(EFishingMode::Attract);
}

//void AVRPawn::OnReelTargetReached()
//{
//    ChangeFishingMode(EFishingMode::Catching);
//}
//
//void AVRPawn::OnFishingCompleted()
//{
//    ChangeFishingMode(EFishingMode::Attract);
//}