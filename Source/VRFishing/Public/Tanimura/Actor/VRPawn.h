// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VRPawn.generated.h"


// 各モードコンポーネントの前方宣言
class UFishingStateManagerComponent;
class UFishingStateComponentBase;
class UFishingReadyStateComponent;  // モード１
// 2026.07.27 Lee start
class UHandHeightDetectorComponent;
// 2026.07.27 Lee end
// 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
class UFishingStateHandUpDown; // モード２（上下運動プレイステート）
// 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
class UFishingReelStateComponent;            // モード３
class UFishingCatchingStateComponent;        // モード４
class UFishingResultStateComponent;          // モード５

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

    // 次のセットを準備状態（モード1）から開始する（GameModeから呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Game")
    void StartNewSet();

protected:
    virtual void BeginPlay() override;

protected:
    // ステートマシンマネージャーコンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingStateManagerComponent> StateManagerComponent;

    // 準備状態コンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingReadyStateComponent> ReadyStateComponent;

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

    // 釣り上げ結果状態コンポーネント
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UFishingResultStateComponent> ResultStateComponent;

    // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    /**
     * @brief 釣りステートマシンを起動する本編マップ名。
     * @note BeginPlay で PIE プレフィックス除去後のマップ名と比較し、一致しない場合は
     *       ステートマシンを起動しない（タイトルメニュー等でのリール誤検知を防止）。
     *       マップ名を変更した場合は本プロパティ（または BP_XRPawn 側の上書き）を更新すること。
     */
    UPROPERTY(EditDefaultsOnly, Category = "Fishing|StateMachine")
    FName FishingMapName = TEXT("LV_MainGame");
    // 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

private:
    // 待機ステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnReadyStateCompleted(bool bIsSuccess);

    // 2026.07.27 Lee start
    // 手の上下運動完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnHandUpDownCompleted(bool bIsSuccess);
    // 2026.07.27 Lee end

    // リールステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnReelStateCompleted(bool bIsSuccess);

    // 釣り上げステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnCatchingStateCompleted(bool bIsSuccess);

    // 釣り上げ完了ステート完了時の通知を受け取るハンドラー
    UFUNCTION()
    void OnResultStateCompleted(bool bIsSuccess);

    // ステート変更時の通知を受け取るハンドラー
    UFUNCTION()
    void OnFishingStateChanged(UFishingStateComponentBase* NewState);
};
