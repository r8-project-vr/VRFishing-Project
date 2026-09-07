// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/FishingGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Takeuchi/Actor/Fish.h"
#include "Tanimura/Actor/VRPawn.h"
#include "VRFishingLog.h"

AFishingGameModeBase::AFishingGameModeBase()
{
    // 制限時間のカウントに使用するためTickを有効化
    PrimaryActorTick.bCanEverTick = true;
}

void AFishingGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // 残り時間を初期表示する（BPのUIバインド用）
    RemainingTime = TotalGameTime;
    UpdateRemainingTimeText();

    // ゲーム開始時に最初の魚をスポーンする（セット開始はVRPawnのReady遷移が担当）
    SpawnFish();
}

void AFishingGameModeBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // ゲーム終了後は時間を進めない
    if (bIsGameOver) {
        return;
    }

    // 経過時間を進める
    CurrentGameTime += DeltaSeconds;

    // 残り時間を更新する（BPのUIバインド用）
    RemainingTime = GetRemainingTime();

    // 残り時間の表示用テキストを更新する（BPのUIバインド用）
    UpdateRemainingTimeText();

    // 制限時間を超えたらゲームを終了する
    if (CurrentGameTime >= TotalGameTime) {
        bIsGameOver = true;
        OnTimeUpBP();
    }
}

void AFishingGameModeBase::OnSetCompleted(bool bIsSuccess)
{
    // セット完了をBPへ通知する（BPでリザルトWidgetを生成・表示する）
    OnSetCompletedBP(bIsSuccess);
}

void AFishingGameModeBase::StartNextSet()
{
    // ゲーム終了後は新しいセットを開始しない
    if (bIsGameOver) {
        return;
    }

    // 前セットの魚を破棄して、新しい魚をスポーンする
    DestroyAllFish();
    SpawnFish();

    // プレイヤーのステートを準備状態（モード1）へ戻す
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) {
        if (AVRPawn* Pawn = Cast<AVRPawn>(PC->GetPawn())) {
            Pawn->StartNewSet();
        }
    }
}

void AFishingGameModeBase::EndGame()
{
    // 以後のセット開始を止める
    bIsGameOver = true;

    // ゲーム終了処理をBPへ委譲する
    OnEndGameBP();
}

AFish* AFishingGameModeBase::SpawnFish()
{
	// 魚クラス未設定なら生成せずに警告ログを出す
    if (!FishClass) {
        UE_LOG(LogFishing, Warning, TEXT("AFishingGameModeBase::SpawnFish: FishClass が未設定のため魚をスポーンできません。BPのClass Defaultsで設定してください。"));
        return nullptr;
    }

    // 指定位置に魚を生成する
    FActorSpawnParameters SpawnParams;
    return GetWorld()->SpawnActor<AFish>(FishClass, FishSpawnLocation, FRotator::ZeroRotator, SpawnParams);
}

void AFishingGameModeBase::DestroyAllFish()
{
    // レベル上の既存の魚をすべて収集して破棄する
    TArray<AActor*> FishActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFish::StaticClass(), FishActors);
    for (AActor* FishActor : FishActors) {
        FishActor->Destroy();
    }
}

float AFishingGameModeBase::GetRemainingTime() const
{
    // 制限時間から経過時間を引いた残り時間を返す（マイナスにはしない）
    return FMath::Max(0.0f, TotalGameTime - CurrentGameTime);
}

void AFishingGameModeBase::UpdateRemainingTimeText()
{
    // 残り時間を分:秒形式にして表示用テキストを更新する
    RemainingTimeText = FText::FromString(FString::Printf(
        TEXT("%d:%02d"),
        FMath::FloorToInt(RemainingTime / 60.0f),
        FMath::FloorToInt(RemainingTime) % 60));
}
