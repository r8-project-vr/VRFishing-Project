// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VRPawn.generated.h"


// 各モードコンポーネントの前方宣言
class UFishingStateManagerComponent;
class UFishingStateComponentBase;
class UFishingStateWait;            // モード１
// 2026.07.27 Lee start
class UHandHeightDetectorComponent; // モード２
// 2026.07.27 Lee end
// 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
class UFishingStateHandUpDown; // モード２（上下運動プレイステート）
// 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
class UFishingReelStateComponent;            // モード３
class UFishingCatchingStateComponent;        // モード４
class UUserWidget;
class UWidgetComponent;

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

    // 2026.07.27 Lee start
    // 手の上下運動検出ステートコンポーネント（モード２）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UHandHeightDetectorComponent> HandUpDownComponent;
    // 2026.07.27 Lee end

    // 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 手の上下運動プレイステートコンポーネント（モード２）。感知は HandUpDownComponent(センサ)が常駐で行い、
    // カウント等のプレイロジックはこのステートが担う。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingStateHandUpDown> HandUpDownStateComponent;
    // 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    // リール回転状態コンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingReelStateComponent> ReelStateComponent;

    // 釣り上げ状態コンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingCatchingStateComponent> CatchingStateComponent;

    // 釣り上げ完了時に前面表示するリザルトWidgetクラス
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> CatchingResultWidgetClass;

    // [追加] 生成した3D UIを保持・管理するためのウィジェットコンポーネント
    UPROPERTY(Transient)
    TObjectPtr<UWidgetComponent> ResultWidgetComponent;

    // [追加] UIをスポーンさせるプレイヤーからの相対オフセット（前方150cm、高さ10cmなど）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    FVector ResultUIOffset = FVector(150.0f, 0.0f, 10.0f);

    // [追加] 生成された3D UIの描画サイズ
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    FVector2D ResultDrawSize = FVector2D(500.0f, 500.0f);

private:
    // 待機ステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnWaitStateCompleted(bool bIsSuccess);

    // 2026.07.27 Lee start
    // 手の上下運動完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnHandUpDownCompleted(bool bIsSuccess);
    // 2026.07.27 Lee end

    // リール目標回転数達成時の通知を受け取るハンドラー
    UFUNCTION()
    void OnReelTargetReached();

    // 釣り上げステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnCatchingStateCompleted(bool bIsSuccess);

    // ステート変更時の通知を受け取るハンドラー
    UFUNCTION()
    void OnFishingStateChanged(UFishingStateComponentBase* NewState);
};
