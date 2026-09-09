// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/FishingGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Takeuchi/Actor/Fish.h"
#include "Tanimura/Actor/VRPawn.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
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

    // 時間切れ済みなら計時しない（進行中のセットは完走させる）
    if (!bIsTimeUp && ShouldAdvanceTimer()) {
        CurrentGameTime += DeltaSeconds;

        // 制限時間に達した瞬間に時間切れを確定し、BPへ通知する
        if (CurrentGameTime >= TotalGameTime) {
            CurrentGameTime = TotalGameTime;
            bIsTimeUp = true;
            OnTimeUpBP();
        }
    }

    // 残り時間を更新する（BPのUIバインド用）
    RemainingTime = GetRemainingTime();

    // 残り時間の表示用テキストを更新する（BPのUIバインド用）
    UpdateRemainingTimeText();
}

void AFishingGameModeBase::OnSetCompleted(bool bIsSuccess)
{
    // セット成功時のみ運動レベルを上げる（上限で頭打ち）
    if (bIsSuccess) {
        ExerciseLevel = FMath::Min(ExerciseLevel + 1, MaxExerciseLevel);
    }

    // セット完了をBPへ通知する（BPでリザルトWidgetを生成・表示する）
    OnSetCompletedBP(bIsSuccess);
}

void AFishingGameModeBase::StartNextSet()
{
    // ゲーム終了後や時間切れ後は新しいセットを開始しない
    if (bIsGameOver || bIsTimeUp) {
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

float AFishingGameModeBase::GetCurrentExerciseSeconds() const
{
    // 現在レベルに応じた運動秒数を計算する（レベル1で基本秒数、以降レベルごとに加算）
    return BaseExerciseSeconds + (ExerciseLevel - 1) * ExerciseSecondsPerLevel;
}

bool AFishingGameModeBase::ShouldAdvanceTimer()
{
    UWorld* World = GetWorld();
    if (!World) {
        return false;
    }

    // プレイヤーPawnを取得する
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) {
        CachedStateManagerComponent.Reset();
        return false;
    }
    APawn* Pawn = PC->GetPawn();
    if (!Pawn) {
        CachedStateManagerComponent.Reset();
        return false;
    }

    // キャッシュが無効（初回・Pawn差し替え・破棄）のときは状態管理コンポーネントを取り直す
    UFishingStateManagerComponent* StateManager = CachedStateManagerComponent.Get();
    if (!StateManager || StateManager->GetOwner() != Pawn) {
        StateManager = Pawn->FindComponentByClass<UFishingStateManagerComponent>();
        CachedStateManagerComponent = StateManager;
    }
    if (!StateManager) {
        return false;
    }

    // 現在アクティブなステートが計時対象かを問い合わせる
    UFishingStateComponentBase* CurrentState = StateManager->GetCurrentState();
    if (!CurrentState) {
        return false;
    }
    return CurrentState->IsTimeCountingState();
}
