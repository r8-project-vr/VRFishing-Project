// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/FishingGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Takeuchi/Actor/Fish.h"
#include "Tanimura/Actor/VRPawn.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Subsystem/FishingCatchHistorySubsystem.h"
#include "Tanimura/Subsystem/FishingWorkoutStatsSubsystem.h"
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

    // 釣果サブシステムへ魚表示情報を転送し、前回プレイの釣果を破棄する
    InitializeCatchHistory();

    // 運動成績サブシステムを取得し、前回プレイの成績を破棄する
    InitializeWorkoutStats();

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

    // 運動ステート中かどうかは1回だけ判定する（残り時間と運動時間で共用する）
    const bool bIsExerciseState = ShouldAdvanceTimer();

    // 運動ステートの間は合計運動時間を加算する（制限時間のタイムアップ後も最終セットぶんを数える）
    if (bIsExerciseState && CachedWorkoutStats.IsValid()) {
        CachedWorkoutStats->AddExerciseSeconds(DeltaSeconds);
    }

    // 時間切れ済みなら計時しない（進行中のセットは完走させる）
    if (!bIsTimeUp && bIsExerciseState) {
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
    // 今回釣った魚はレベル加算前のレベルに対応する（成功するとレベルが上がるため）
    const int32 CaughtFishLevel = ExerciseLevel;

    // 釣果をサブシステムへ記録する（レベル遷移後の最終リザルトでも参照するため）
    UFishingCatchHistorySubsystem* CatchHistory = GetCatchHistorySubsystem();
    if (CatchHistory) {
        if (bIsSuccess) {
            // 成功時のみ釣果として積み、今回のセット結果にも同じレベルを残す
            CatchHistory->AddCaughtFish(CaughtFishLevel);
            CatchHistory->SetCaughtFishLevel(CaughtFishLevel);
        }
        else {
            // 失敗時は釣れていないため履歴に積まず、レベル0（釣れていない）を残す
            CatchHistory->SetCaughtFishLevel(0);
        }
    }

    // セット失敗はそのまま「逃がした魚1匹」として数える
    if (!bIsSuccess) {
        UFishingWorkoutStatsSubsystem* WorkoutStats = GetWorkoutStatsSubsystem();
        if (WorkoutStats) {
            WorkoutStats->AddEscapedFish();
        }
    }

    // セット成功時のみ運動レベルを上げる（上限で頭打ち）
    if (bIsSuccess) {
        ExerciseLevel = FMath::Min(ExerciseLevel + 1, MaxExerciseLevel);
    }

    // セット完了をBPへ通知する（サブシステム更新後に呼ぶことでBPが最新値を読める）
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

FText AFishingGameModeBase::GetFishNameByLevel(int32 Level) const
{
    // 釣果サブシステムへ委譲する（未取得時は空テキストを返す）
    UFishingCatchHistorySubsystem* CatchHistory = GetCatchHistorySubsystem();
    if (!CatchHistory) {
        return FText::GetEmpty();
    }
    return CatchHistory->GetFishNameByLevel(Level);
}

UTexture2D* AFishingGameModeBase::GetFishTextureByLevel(int32 Level) const
{
    // 釣果サブシステムへ委譲する（未取得時はnullptrを返す）
    UFishingCatchHistorySubsystem* CatchHistory = GetCatchHistorySubsystem();
    if (!CatchHistory) {
        return nullptr;
    }
    return CatchHistory->GetFishTextureByLevel(Level);
}

int32 AFishingGameModeBase::GetCaughtFishLevel() const
{
    // 釣果サブシステムへ委譲する（未取得時は釣れていない扱いにする）
    UFishingCatchHistorySubsystem* CatchHistory = GetCatchHistorySubsystem();
    if (!CatchHistory) {
        return 0;
    }
    return CatchHistory->GetCaughtFishLevel();
}

void AFishingGameModeBase::AddArmUpDownCount(int32 Count)
{
    // 運動成績サブシステムへ委譲する（未取得時は何もしない）
    UFishingWorkoutStatsSubsystem* WorkoutStats = GetWorkoutStatsSubsystem();
    if (!WorkoutStats) {
        return;
    }
    WorkoutStats->AddArmUpDownCount(Count);
}

void AFishingGameModeBase::AddReelRevolutionCount(int32 Count)
{
    // 運動成績サブシステムへ委譲する（未取得時は何もしない）
    UFishingWorkoutStatsSubsystem* WorkoutStats = GetWorkoutStatsSubsystem();
    if (!WorkoutStats) {
        return;
    }
    WorkoutStats->AddReelRevolutionCount(Count);
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

UFishingCatchHistorySubsystem* AFishingGameModeBase::GetCatchHistorySubsystem() const
{
    const UWorld* World = GetWorld();
    if (!World) {
        return nullptr;
    }

    // GameInstanceに属するサブシステムを取得する（レベル遷移後も生存する）
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance) {
        return nullptr;
    }

    return GameInstance->GetSubsystem<UFishingCatchHistorySubsystem>();
}

void AFishingGameModeBase::InitializeCatchHistory()
{
    UFishingCatchHistorySubsystem* CatchHistory = GetCatchHistorySubsystem();
    if (!CatchHistory) {
        UE_LOG(LogFishing, Warning, TEXT("AFishingGameModeBase::InitializeCatchHistory: 釣果サブシステムを取得できませんでした。"));
        return;
    }

    // 前回プレイの釣果を破棄してから、今回の魚表示情報を転送する
    CatchHistory->ResetCaughtFish();
    CatchHistory->SetFishDisplays(FishDisplays);

    // Class Defaultsの設定漏れ（レベル数より魚表示情報が少ない）を警告する
    if (FishDisplays.Num() < MaxExerciseLevel) {
        UE_LOG(LogFishing, Warning,
            TEXT("AFishingGameModeBase::InitializeCatchHistory: FishDisplays が %d 件しか設定されていません（MaxExerciseLevel は %d）。BPのClass Defaultsで設定してください。"),
            FishDisplays.Num(), MaxExerciseLevel);
    }
}

UFishingWorkoutStatsSubsystem* AFishingGameModeBase::GetWorkoutStatsSubsystem() const
{
    // 初期化済みならキャッシュを返す（Tickから毎フレーム呼ばれるため）
    if (CachedWorkoutStats.IsValid()) {
        return CachedWorkoutStats.Get();
    }

    const UWorld* World = GetWorld();
    if (!World) {
        return nullptr;
    }

    // GameInstanceに属するサブシステムを取得する（レベル遷移後も生存する）
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance) {
        return nullptr;
    }

    return GameInstance->GetSubsystem<UFishingWorkoutStatsSubsystem>();
}

void AFishingGameModeBase::InitializeWorkoutStats()
{
    UFishingWorkoutStatsSubsystem* WorkoutStats = GetWorkoutStatsSubsystem();
    if (!WorkoutStats) {
        UE_LOG(LogFishing, Warning, TEXT("AFishingGameModeBase::InitializeWorkoutStats: 運動成績サブシステムを取得できませんでした。"));
        return;
    }

    // 前回プレイの成績を破棄する
    WorkoutStats->ResetWorkoutStats();

    // 以降はキャッシュ経由で参照する
    CachedWorkoutStats = WorkoutStats;
}
