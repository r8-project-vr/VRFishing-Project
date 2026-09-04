// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FishingGameModeBase.generated.h"

class AFish;
class AVRPawn;

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

    // 残り時間の表示用テキスト（例：残り 1:30）（BPのUIバインド用に毎Tick更新する）
    UPROPERTY(BlueprintReadOnly, Category = "Fishing|Game")
    FText RemainingTimeText;

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
    float TotalGameTime = 90.0f;

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

    // ゲーム終了フラグ（trueの間は新しいセットを開始しない）
    bool bIsGameOver = false;
};