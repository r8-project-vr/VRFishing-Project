// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingStateComponentBase.generated.h"

/**
 * ステート完了時に発火するデリゲートの型宣言
 * @param bIsSuccess true=成功 / false=失敗
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFishingStateCompleted, bool, bIsSuccess);

/**
 * 釣りゲームの各モード（準備・腕上下・リール・釣り上げ・結果）が共通で継承するステートの基底クラス
 * UFishingStateManagerComponentのTickで1つだけ駆動するため、各ステートのTickは無効化している
 */
UCLASS(Abstract, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingStateComponentBase : public UActorComponent
{
    GENERATED_BODY()

public:
    UFishingStateComponentBase();

    // ステート開始時に一度だけ呼ばれる初期化処理
    UFUNCTION(BlueprintCallable, Category = "Fishing|State")
    virtual void EnterState();

    /**
     * ステート内で毎フレーム呼ばれる更新処理
     * @param DeltaTime 前フレームからの経過時間（秒）
     */
    UFUNCTION(BlueprintCallable, Category = "Fishing|State")
    virtual void UpdateState(float DeltaTime);

    // ステート終了時に一度だけ呼ばれるリセット処理
    UFUNCTION(BlueprintCallable, Category = "Fishing|State")
    virtual void ExitState();

    // ステートが制限時間を進める対象か
    UFUNCTION(BlueprintPure, Category = "Fishing|State")
    virtual bool IsTimeCountingState() const;

    // 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // ステートの表示名（ログ・UI表示用）を返す
    UFUNCTION(BlueprintPure, Category = "Fishing|State")
    virtual FString GetStateDisplayName() const;
    // 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

public:
    // ステート完了を外部へ通知するデリゲートのインスタンス
    UPROPERTY(BlueprintAssignable, Category = "Fishing|Events")
    FOnFishingStateCompleted OnFishingStateCompleted;
};