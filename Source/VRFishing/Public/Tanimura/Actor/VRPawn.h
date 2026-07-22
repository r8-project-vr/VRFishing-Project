// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VRPawn.generated.h"


// 各モードコンポーネントの前方宣言
class UHandHeightDetectorComponent;
class UReelSimulatorComponent;
class UCatchingSimulatorComponent;

UENUM(BlueprintType)
enum class EFishingMode : uint8
{
    Attract,  // 1. 誘うモード (HandHeightDetector)
    Reeling,  // 2. リールを巻くモード (ReelSimulator)
    Catching  // 3. 魚を釣り上げるモード (CatchingSimulator)
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

    // ポーンにアタッチされている各コンポーネントの参照
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UHandHeightDetectorComponent> HandHeightDetectorComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UReelSimulatorComponent> ReelComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UCatchingSimulatorComponent> CatchingComponent;

private:
    // 現在のモード
    EFishingMode CurrentMode;

    // 各コンポーネントからのイベントを受け取るハンドラー関数
    UFUNCTION()
    void OnFishHit();

    UFUNCTION()
    void OnReelTargetReached();

    UFUNCTION()
    void OnFishingCompleted();
};
