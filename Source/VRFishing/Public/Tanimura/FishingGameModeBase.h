// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Tanimura/Subsystem/FishingCatchHistorySubsystem.h"
#include "Tanimura/Subsystem/FishingWorkoutStatsSubsystem.h"
#include "FishingGameModeBase.generated.h"

class AFish;
class AVRPawn;
class UFishingStateManagerComponent;
class UTexture2D;

/**
 * 釣りゲーム本編（LV_MainGame）専用のゲームモード
 * 制限時間内でモード1（準備）からモード5（結果）のセットを繰り返し、魚の再スポーンも担当する
 * タイトル・リザルト等の他画面は本クラスを継承せず、それぞれ専用のゲームモードを使う
 */
UCLASS()
class VRFISHING_API AFishingGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AFishingGameModeBase();

    // 制限時間のカウントとセット開始処理
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // セット完了を通知する（VRPawnから呼ばれる）
    void OnSetCompleted(bool bIsSuccess);

    // 次のセットを開始する（リザルトWidgetの「次のセットへ」ボタンから呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Game")
    void StartNextSet();

    // ゲームを終了する（リザルトWidgetの「ゲームを終了」ボタンから呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Game")
    void EndGame();

    // ゲーム開始からの経過時間
    UPROPERTY(BlueprintReadOnly, Category = "Fishing|Game")
    float CurrentGameTime = 0.0f;

    // 残り時間（秒）（BPのUIバインド用に毎Tick更新する）
    UPROPERTY(BlueprintReadOnly, Category = "Fishing|Game")
    float RemainingTime = 0.0f;

    // 残り時間の表示用テキスト（例：1:30）（BPのUIバインド用に毎Tick更新する）
    UPROPERTY(BlueprintReadOnly, Category = "Fishing|Game")
    FText RemainingTimeText;

    // 時間切れフラグ（タイムアップの瞬間にtrueへ変わり、最終セットの結果Widget表示判定に使う）
    UPROPERTY(BlueprintReadOnly, Category = "Fishing|Game")
    bool bIsTimeUp = false;

    // 現在の運動レベル（セット成功ごとに1増え、MaxExerciseLevelで頭打ち）
    UPROPERTY(BlueprintReadOnly, Category = "Fishing|Exercise")
    int32 ExerciseLevel = 1;

    // レベル1のときの1セットあたりの運動時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Exercise")
    float BaseExerciseSeconds = 20.0f;

    // レベルが1上がるごとに増える運動時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Exercise")
    float ExerciseSecondsPerLevel = 5.0f;

    // 運動レベルの上限
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Exercise")
    int32 MaxExerciseLevel = 5;

    // レベル別の魚表示情報（インデックス0がレベル1。MaxExerciseLevelぶん設定する）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Catch")
    TArray<FFishDisplayInfo> FishDisplays;

    // 現在レベルに応じた運動時間（秒）を返す（各運動ステートがEnter時に参照）
    UFUNCTION(BlueprintPure, Category = "Fishing|Exercise")
    float GetCurrentExerciseSeconds() const;

    // 指定レベルに対応する魚の表示名を返す（サブシステムへ委譲する）
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    FText GetFishNameByLevel(int32 Level) const;

    // 指定レベルに対応する魚のイラストを返す（サブシステムへ委譲する）
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    UTexture2D* GetFishTextureByLevel(int32 Level) const;

    // 今回のセットで釣った魚のレベルを返す（0=釣れていない）
    UFUNCTION(BlueprintPure, Category = "Fishing|Catch")
    int32 GetCaughtFishLevel() const;

    // 今回セットの腕上下回数を合計へ加算する（VRPawnの完了通知から呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Workout")
    void AddArmUpDownCount(int32 Count);

    // 今回セットのリール回転回数を合計へ加算する（VRPawnの完了通知から呼ばれる）
    UFUNCTION(BlueprintCallable, Category = "Fishing|Workout")
    void AddReelRevolutionCount(int32 Count);

    // 残り時間（秒）を取得する
    UFUNCTION(BlueprintPure, Category = "Fishing|Game")
    float GetRemainingTime() const;

protected:
    // セット完了時のBPイベント（BPでリザルトWidgetを生成・表示する）
    UFUNCTION(BlueprintImplementableEvent, Category = "Fishing|Game")
    void OnSetCompletedBP(bool bIsSuccess);

    // 制限時間に達したときのBPイベント
    UFUNCTION(BlueprintImplementableEvent, Category = "Fishing|Game")
    void OnTimeUpBP();

    // ゲーム終了時のBPイベント
    UFUNCTION(BlueprintImplementableEvent, Category = "Fishing|Game")
    void OnEndGameBP();

    // 制限時間（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Game")
    float TotalGameTime = 300.0f;

    // スポーンする魚クラス
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Fish")
    TSubclassOf<AFish> FishClass;

    // 魚の初期スポーン位置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Fish")
    FVector FishSpawnLocation = FVector::ZeroVector;

private:
    // 魚を生成する
    AFish* SpawnFish();

    // レベル上の既存の魚をすべて破棄する
    void DestroyAllFish();

    // RemainingTime から表示用テキストを更新する
    void UpdateRemainingTimeText();

    // 現在ステートが制限時間を進める対象か判定
    bool ShouldAdvanceTimer();

    // 釣果サブシステムを取得する
    UFishingCatchHistorySubsystem* GetCatchHistorySubsystem() const;

    // 釣果サブシステムへ魚表示情報を転送し、今回の釣果を破棄する
    void InitializeCatchHistory();

    // 運動成績サブシステムを取得する
    UFishingWorkoutStatsSubsystem* GetWorkoutStatsSubsystem() const;

    // 運動成績サブシステムを取得して今回の成績を破棄する
    void InitializeWorkoutStats();

    // 運動成績サブシステムのキャッシュ（Tickで毎フレーム参照するため保持する）
    TWeakObjectPtr<UFishingWorkoutStatsSubsystem> CachedWorkoutStats;

    // ステート管理コンポーネントのキャッシュ
    TWeakObjectPtr<UFishingStateManagerComponent> CachedStateManagerComponent;

    // ゲーム終了フラグ（trueの間は新しいセットを開始しない）
    bool bIsGameOver = false;
};