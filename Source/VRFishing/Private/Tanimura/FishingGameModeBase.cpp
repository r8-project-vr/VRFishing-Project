// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/FishingGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Takeuchi/Actor/Fish.h"
#include "Tanimura/Actor/VRPawn.h"

AFishingGameModeBase::AFishingGameModeBase()
{
    // 制限時間のカウントに使用するためTickを有効化
    PrimaryActorTick.bCanEverTick = true;
}

void AFishingGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // 残り時間を表示するマップでは総時間を初期表示する（タイトルではここで固定表示される）
    if (IsTimeDisplayMap()) {
        RemainingTime = TotalGameTime;
        UpdateRemainingTimeText();
    }

    // 本編マップ以外ではゲームを開始しない（タイトル・リザルトレベルでの魚スポーンを防ぐ）
    if (!IsCurrentMapFishing()) {
        // タイトルでは表示の同期にTickが必要なため、表示対象外のマップ（リザルト等）でのみTickを無効化する
        if (!IsTimeDisplayMap()) {
            SetActorTickEnabled(false);
        }
        return;
    }

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

    // 本編マップでのみ経過時間を進める（タイトルでは表示の同期のみ行う）
    if (IsCurrentMapFishing()) {
        CurrentGameTime += DeltaSeconds;
    }

    // 残り時間を更新する（BPのUIバインド用）
    RemainingTime = GetRemainingTime();

    // 残り時間の表示用テキストを更新する（BPのUIバインド用）
    UpdateRemainingTimeText();

    // 残り時間を画面表示する（テスト用）
    if (GEngine) {
        GEngine->AddOnScreenDebugMessage(
            6, 0.0f, FColor::Green,
            FString::Printf(TEXT("[Time] 残り %.0f 秒 / 合計 %.0f 秒"), GetRemainingTime(), TotalGameTime));
    }

    // 制限時間を超えたらゲームを終了する（本編マップのみ）
    if (IsCurrentMapFishing() && CurrentGameTime >= TotalGameTime) {
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
    // 本編マップ以外ではセットを開始しない
    if (!IsCurrentMapFishing()) {
        return;
    }

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
    // 本編マップ以外ではゲームを終了しない
    if (!IsCurrentMapFishing()) {
        return;
    }

    // 以後のセット開始を止める
    bIsGameOver = true;

    // ゲーム終了処理をBPへ委譲する
    OnEndGameBP();
}

AFish* AFishingGameModeBase::SpawnFish()
{
    // 魚クラス未設定なら生成しない
    if (!FishClass) {
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

bool AFishingGameModeBase::IsCurrentMapFishing() const
{
    const UWorld* World = GetWorld();
    if (!World) {
        return false;
    }

    // PIEではマップ名に "UEDPIE_0_" が付くため除去してから本編マップと比較する（FName比較は大文字小文字を区別しない）
    return FName(*UWorld::RemovePIEPrefix(World->GetMapName())) == FishingMapName;
}

bool AFishingGameModeBase::IsCurrentMapTitle() const
{
    const UWorld* World = GetWorld();
    if (!World) {
        return false;
    }

    // PIEではマップ名に "UEDPIE_0_" が付くため除去してからタイトルマップと比較する（FName比較は大文字小文字を区別しない）
    return FName(*UWorld::RemovePIEPrefix(World->GetMapName())) == TitleMapName;
}

bool AFishingGameModeBase::IsTimeDisplayMap() const
{
    // 残り時間を表示するのは本編マップかタイトルマップのみとする
    return IsCurrentMapFishing() || IsCurrentMapTitle();
}

void AFishingGameModeBase::UpdateRemainingTimeText()
{
    // 残り時間を「残り 分:秒」形式へ整形して表示用テキストを更新する
    RemainingTimeText = FText::FromString(FString::Printf(
        TEXT("残り %d:%02d"),
        FMath::FloorToInt(RemainingTime / 60.0f),
        FMath::FloorToInt(RemainingTime) % 60));
}
