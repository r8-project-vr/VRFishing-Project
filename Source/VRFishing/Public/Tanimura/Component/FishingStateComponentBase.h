// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingStateComponentBase.generated.h"

// ステート完了時に発火するデリゲートの型宣言
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFishingStateCompleted, bool, bIsSuccess);

UCLASS(Abstract, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingStateComponentBase : public UActorComponent
{
    GENERATED_BODY()

public:
    UFishingStateComponentBase();

    // ステート開始時の共通処理
    UFUNCTION(BlueprintCallable, Category = "Fishing|State")
    virtual void EnterState();

    // ステート更新時の共通処理
    UFUNCTION(BlueprintCallable, Category = "Fishing|State")
    virtual void UpdateState(float DeltaTime);

    // ステート終了時の共通処理
    UFUNCTION(BlueprintCallable, Category = "Fishing|State")
    virtual void ExitState();

public:
    // ステート完了を外部へ通知するデリゲートインスタンス
    UPROPERTY(BlueprintAssignable, Category = "Fishing|Events")
    FOnFishingStateCompleted OnFishingStateCompleted;
};