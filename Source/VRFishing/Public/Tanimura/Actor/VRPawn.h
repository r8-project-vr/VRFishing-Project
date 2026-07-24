// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VRPawn.generated.h"


// 各モードコンポーネントの前方宣言
class UHandHeightDetectorComponent;
class UFishingReelStateComponent;
// class UCatchingSimulatorComponent;
// === 追加：釣り竿アクター用クラスの前方宣言 ===
class AActor;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EFishingMode : uint8
{
    Attract,  // 1. 誘うモード (HandHeightDetector)
    Reeling,  // 2. リールを巻くモード (ReelSimulator)
    // Catching  // 3. 魚を釣り上げるモード (CatchingSimulator)
};

UCLASS()
class VRFISHING_API AVRPawn : public APawn
{
    GENERATED_BODY()

public:
    AVRPawn();

protected:
    virtual void BeginPlay() override;

    // 手動でもモードを切り替えられる関数（BPからも呼び出し可能）
    UFUNCTION(BlueprintCallable, Category = "Fishing System")
    void ChangeFishingMode(EFishingMode NewMode);

    // === 追加：釣り竿を手のソケットに生成・アタッチする関数 ===
    UFUNCTION(BlueprintCallable, Category = "Fishing System")
    void SpawnAndAttachFishingRod();

    // === 追加：生成する釣り竿のアクタークラス（BP側で設定可能） ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing System|Setup")
    TSubclassOf<AActor> FishingRodClass;

    // === 追加：アタッチ先の手のメッシュコンポーネント参照 ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USkeletalMeshComponent> HandMeshComponent;

    // === 追加：アタッチ先ソケット名 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing System|Setup")
    FName RodSocketName;

    // === 追加：生成された釣り竿のアクター参照 ===
    UPROPERTY(BlueprintReadOnly, Category = "Fishing System")
    TObjectPtr<AActor> SpawnedFishingRod;

    // ポーンにアタッチされている各コンポーネントの参照
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UHandHeightDetectorComponent> HandHeightDetectorComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingReelStateComponent> ReelComponent;

    //UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    //TObjectPtr<UCatchingSimulatorComponent> CatchingComponent;

private:
    // 現在のモード
    EFishingMode CurrentMode;

    // 各コンポーネントからのイベントを受け取るハンドラー関数
    UFUNCTION()
    void OnFishHit();

    UFUNCTION()
    void OnReelTargetReached();

    //UFUNCTION()
    //void OnFishingCompleted();
};
