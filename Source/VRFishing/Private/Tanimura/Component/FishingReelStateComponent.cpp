// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingReelStateComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tanimura/FishingGameModeBase.h"
// 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "VRFishingLog.h"
// 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

UFishingReelStateComponent::UFishingReelStateComponent()
{
    // ステート単体でのTickは無効化（Manager経由でUpdateStateが呼ばれる）
    PrimaryComponentTick.bCanEverTick = false;

    StickThreshold = 0.6f;
    WheelNotchAngleRad = UE_PI / 12.0f; // π/12 ≒ 0.2618rad（15度）

    bIsCompleted = false;

    LastAngle = 0.0f;
    AccumulatedAngleRad = 0.0f;
    RotationStartTime = 0.0;
    bIsMeasuringRotation = false;
    bIsStickTracking = false;
    TargetRevolutionCount = 10;
    CurrentRevolutionCount = 0;
    LastRevolutionTime = 0.0;

    OverRPMCount = 0;
    UnderRPMCount = 0;
}

void UFishingReelStateComponent::EnterState()
{
    Super::EnterState();

    // ステート開始時にリール状態を初期化
    bIsCompleted = false;
    ResetRevolutionCount();

    // 残り運動時間をGameModeの現在レベルから設定する（時間ベース完了へ移行）
    RemainingExerciseSeconds = 20.0f;
    const UWorld* World = GetWorld();
    AFishingGameModeBase* GameMode = nullptr;
    if (World) {
        GameMode = World->GetAuthGameMode<AFishingGameModeBase>();
    }
    if (GameMode) {
        RemainingExerciseSeconds = GameMode->GetCurrentExerciseSeconds();
    }

    // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 第2セット以降にリールが無反作になる問題の切り分け用ログ。
    // ChangeState() は Activate() → EnterState() の順で呼ぶため、ここでの IsActive() は
    // Activate が実際に効いているかを直接検証する（false のままなら Activate 不生效）。
    UE_LOG(LogFishing, Log, TEXT("[FishingReel] EnterState: IsActive=%d TargetRevolutionCount=%d Min=%.0f WheelMax=%.0f StickMax=%.0f"),
        IsActive() ? 1 : 0, TargetRevolutionCount, MinAllowedRPM, WheelMaxAllowedRPM, StickMaxAllowedRPM);
    // 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
}

void UFishingReelStateComponent::UpdateState(float DeltaTime)
{
    Super::UpdateState(DeltaTime);

    // 完了済み（成功/失敗）なら以降の処理を行わない
    if (bIsCompleted) {
        return;
    }

    // 最初の1回転を検知するまで時間計測と停止検知を開始しない（無回転のまま成功させない）
    if (CurrentRevolutionCount <= 0) {
        return;
    }

    const UWorld* World = GetWorld();
    if (!World) {
        return;
    }
    const double CurrentTime = World->GetTimeSeconds();

    // 停止検知：最後の1回転から一定時間が経過したら失敗として完了する（停止=失敗）
    if (CurrentTime - LastRevolutionTime >= RevolveStopTimeoutSeconds) {
        bIsCompleted = true;
        OnFishingStateCompleted.Broadcast(false);
        return;
    }

    // 残り運動時間を減算し、0以下になったら時間ベースで成功として完了する
    RemainingExerciseSeconds -= DeltaTime;
    if (RemainingExerciseSeconds <= 0.0f) {
        RemainingExerciseSeconds = 0.0f;
        bIsCompleted = true;
        OnFishingStateCompleted.Broadcast(true);
    }
}

void UFishingReelStateComponent::ExitState()
{
    Super::ExitState();

    //// 終了時にリール状態をリセット
    //ResetRevolutionCount();
}

// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
FString UFishingReelStateComponent::GetStateDisplayName() const
{
    return TEXT("ぐるぐるまわして！");
}

bool UFishingReelStateComponent::IsTimeCountingState() const
{
    // 最初の1回転を検知するまでは全体タイマーを進めない（無回転の待機時間を消費しない）
    return CurrentRevolutionCount >= 1;
}
// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

void UFishingReelStateComponent::ResetRevolutionCount()
{
    CurrentRevolutionCount = 0;
    AccumulatedAngleRad = 0.0f;
    bIsMeasuringRotation = false;
    bIsStickTracking = false;

    // ミス回数をリセット
    OverRPMCount = 0;
    UnderRPMCount = 0;

    // 停止検知の基準時刻を無効化（次の最初の1回転を起点に再設定する）
    LastRevolutionTime = 0.0;
}

void UFishingReelStateComponent::ApplyRotationLoadLevel(int32 LoadLevel)
{
    // 負荷が高いほど速すぎ閾値と遅すぎ閾値を上げて厳しくする
    if (LoadLevel == 0) {
        WheelMaxAllowedRPM = 30.0f;
        StickMaxAllowedRPM = 80.0f;
        MinAllowedRPM = 20.0f;
    } else if (LoadLevel == 1) {
        WheelMaxAllowedRPM = 40.0f;
        StickMaxAllowedRPM = 100.0f;
        MinAllowedRPM = 30.0f;
    } else {
        WheelMaxAllowedRPM = 50.0f;
        StickMaxAllowedRPM = 120.0f;
        MinAllowedRPM = 40.0f;
    }
}

void UFishingReelStateComponent::SimulateReelByStick(FVector2D StickInput)
{
    // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 入力到達診断ログ（ガードの前に置く。1 秒に 1 回のみ出力）。
    // このログが出ない＝BP の入力イベントから本関数が呼ばれていない（入力バインド断絶）。
    // ログは出るが IsActive=0 ＝ガードで弾かれている。
    static double LastStickLogTime = 0.0;
    const double NowSec = FPlatformTime::Seconds();
    if (NowSec - LastStickLogTime >= 1.0)
    {
        LastStickLogTime = NowSec;
        UE_LOG(LogFishing, Log, TEXT("[FishingReel] Stick入力到達: X=%.2f Y=%.2f IsActive=%d"),
            StickInput.X, StickInput.Y, IsActive() ? 1 : 0);
    }
    // 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    //// 非アクティブ時は入力を無視
    //if (!IsActive()) {
    //    return;
    //}
    // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // ↑ コメントアウトされていた IsActive ガードを復活する。
    // BP_XRPawn の入力イベントが本関数を直接呼ぶため、ここが唯一の入口ガードとなる。
    // 非アクティブ（リールステート以外）時は入力を無視して RPM 誤検知を防ぐ。
    if (!IsActive()) {
        return;
    }
    // 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    // 入力値が閾値未満なら追跡しない
    if (StickInput.SizeSquared() < FMath::Square(StickThreshold)) {
        bIsStickTracking = false;
        return;
    }

    // 入力ベクトルから現在の角度（-π〜π）を算出
    const float CurrentAngle = FMath::Atan2(StickInput.Y, StickInput.X);

    // 追跡開始時は角度の記録のみ行う
    if (!bIsStickTracking) {
        LastAngle = CurrentAngle;
        bIsStickTracking = true;
        return;
    }

    // 前フレームからの角度変化量を計算
    const float DeltaAngle = FMath::FindDeltaAngleRadians(LastAngle, CurrentAngle);

	// 順方向回転のみRPMの算出対象とする
    if (DeltaAngle > 0.0f) {
        CalculateRPM(DeltaAngle, StickMaxAllowedRPM);
    }

    // 次フレーム計算用に現在の角度を保存
    LastAngle = CurrentAngle;
}

void UFishingReelStateComponent::SimulateReelByWheel()
{
    // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // 入力到達診断ログ（ガードの前に置く。Stick 版と同様に 1 秒に 1 回のみ出力）。
    static double LastWheelLogTime = 0.0;
    const double NowSec = FPlatformTime::Seconds();
    if (NowSec - LastWheelLogTime >= 1.0)
    {
        LastWheelLogTime = NowSec;
        UE_LOG(LogFishing, Log, TEXT("[FishingReel] Wheel入力到達: IsActive=%d"), IsActive() ? 1 : 0);
    }
    // 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    //// 非アクティブ時は入力を無視
    //if (!IsActive()) {
    //    return;
    //}
    // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // ↑ コメントアウトされていた IsActive ガードを復活する（Stick 版と同様の理由）。
    if (!IsActive()) {
        return;
    }
    // 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    // ホイール1ノッチ分の回転角（固定）を流し込む
    CalculateRPM(WheelNotchAngleRad, WheelMaxAllowedRPM);
}

void UFishingReelStateComponent::CalculateRPM(float DeltaAngle, float MaxAllowedRPM)
{
    const UWorld* World = GetWorld();
    if (!World) {
        return;
    }

    // 完了済み（成功/失敗）なら以降の入力を無視
    if (bIsCompleted) {
        return;
    }

    const double CurrentTime = World->GetTimeSeconds();

    // 最初の回転入力時に計測開始時間を記録
    if (!bIsMeasuringRotation) {
        RotationStartTime = CurrentTime;
        bIsMeasuringRotation = true;
        AccumulatedAngleRad = 0.0f;
    }

    // 角度変化量を累積
    AccumulatedAngleRad += DeltaAngle;

    // 累積角度が1回転（2π）に達したか判定
    const float OneRevolutionRad = UE_TWO_PI;   // 2π ≒ 6.28318530717f
    if (AccumulatedAngleRad >= OneRevolutionRad) {
        // 最初の1回転かどうかを判定（回転数加算前の値で判定する）
        const bool bIsFirstRevolution = (CurrentRevolutionCount == 0);

        // 最初の1回転の速度は失敗判定にもUI表示にも使わない（計時・停止検知の起点のみ）
        if (!bIsFirstRevolution) {
            // 経過時間を算出
            const double DeltaTime = CurrentTime - RotationStartTime;

            // 0で割るのを防止
            if (DeltaTime > 0.001) {
                // 1Min = 60秒で何回転できるか（RPM）を計算
                const float CalculatedRPM = static_cast<float>(60.0 / DeltaTime);
                // 算出したRPMをバインド先へ通知
                OnRPMCalculated.Broadcast(CalculatedRPM);

                // 引数で渡された上限RPMで速すぎ・遅すぎを判定
                JudgeRPM(CalculatedRPM, MaxAllowedRPM);

                // ミス累積で失敗が確定した場合は、回転数加算・計時開始へ進まない
                if (bIsCompleted) {
                    return;
                }
            }
        }

        // 累積角度と時間をリセット
        AccumulatedAngleRad -= OneRevolutionRad;   // 誤差の蓄積を防ぐため端数は残す
        RotationStartTime = CurrentTime;

        // 回転数を加算する（完了判定はUpdateStateの時間ベース処理へ移行）
        CurrentRevolutionCount++;

        // 停止検知の基準時刻を更新（最初の1回転完了時から停止検知・計時を開始する）
        LastRevolutionTime = CurrentTime;

        // 2026.08.20 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
        // 1回転ごとの進捗ログ（入力がステートまで届いているかの確認用）
        UE_LOG(LogFishing, Log, TEXT("[FishingReel] Revolution: %d/%d"), CurrentRevolutionCount, TargetRevolutionCount);
        // 2026.08.20 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
    }
}

void UFishingReelStateComponent::JudgeRPM(float CalculatedRPM, float MaxAllowedRPM)
{
    // 速すぎミス（上限超過）をカウント
    if (CalculatedRPM > MaxAllowedRPM) {
        OverRPMCount++;
        ShowErrorLog(true, CalculatedRPM);

        // 許容回数に達したら釣り失敗
        if (OverRPMCount >= MaxMistakeCount) {
            bIsCompleted = true;
            OnFishingStateCompleted.Broadcast(false);
        }
        return;
    }

    // 遅すぎミス（下限未満）をカウント
    if (CalculatedRPM < MinAllowedRPM) {
        UnderRPMCount++;
        ShowErrorLog(false, CalculatedRPM);

        // 許容回数に達したら釣り失敗
        if (UnderRPMCount >= MaxMistakeCount) {
            bIsCompleted = true;
            OnFishingStateCompleted.Broadcast(false);
        }
    }
}

void UFishingReelStateComponent::ShowErrorLog(bool bIsTooFast, float CurrentRPM)
{
    // ミス種別の表示名を生成
    FString ErrorName = TEXT("遅すぎ");
    if (bIsTooFast) {
        ErrorName = TEXT("速すぎ");
    }

    // ミスの累積回数を取得
    int32 MistakeCount = UnderRPMCount;
    if (bIsTooFast) {
        MistakeCount = OverRPMCount;
    }

    // ミスログを画面に表示
    if (GEngine) {
        GEngine->AddOnScreenDebugMessage(
            -1, 3.0f, FColor::Red,
            FString::Printf(TEXT("[FishingReel] %sミス RPM=%.1f (%d/%d回)"),
                *ErrorName, CurrentRPM, MistakeCount, MaxMistakeCount));
    }

    // ミスログを出力ログにも表示
    UE_LOG(LogTemp, Error, TEXT("[FishingReel] %sミス RPM=%.1f (%d/%d回)"),
        *ErrorName, CurrentRPM, MistakeCount, MaxMistakeCount);
}