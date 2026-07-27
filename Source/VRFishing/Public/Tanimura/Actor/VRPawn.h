// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VRPawn.generated.h"


// 各モードコンポーネントの前方宣言
class UFishingStateManagerComponent;
class UFishingStateWait;            // モード１
//class UHandHeightDetectorComponent; // モード２
class UFishingReelStateComponent;            // モード３
class UFishingCatchingStateComponent;        // モード４
class UUserWidget;

UCLASS()
class VRFISHING_API AVRPawn : public APawn
{
    GENERATED_BODY()

public:
    AVRPawn();

    // スティック入力発生時に呼び出すハンドラー
    UFUNCTION(BlueprintCallable, Category = "Fishing|Input")
    void InjectReelStickInput(FVector2D StickInput);

    // マウスホイール入力発生時に呼び出すハンドラー
    UFUNCTION(BlueprintCallable, Category = "Fishing|Input")
    void InjectReelWheelInput();

protected:
    virtual void BeginPlay() override;

protected:
    // ステートマシンマネージャーコンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingStateManagerComponent> StateManagerComponent;

    // 待機状態コンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingStateWait> WaitStateComponent;

    // リール回転状態コンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingReelStateComponent> ReelStateComponent;

    // 釣り上げ状態コンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingCatchingStateComponent> CatchingStateComponent;

    // 釣り上げ完了時に前面表示するリザルトWidgetクラス
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> CatchingResultWidgetClass;

private:
    // 待機ステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnWaitStateCompleted(bool bIsSuccess);

    // リール目標回転数達成時の通知を受け取るハンドラー
    UFUNCTION()
    void OnReelTargetReached();

    // 釣り上げステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnCatchingStateCompleted(bool bIsSuccess);
};
