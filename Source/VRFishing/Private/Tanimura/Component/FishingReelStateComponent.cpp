// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Component/FishingReelStateComponent.h"

UFishingReelStateComponent::UFishingReelStateComponent()
{
    // ステート単体でのTickは無効化（Manager経由でUpdateStateが呼ばれる）
    PrimaryComponentTick.bCanEverTick = false;

    StickThreshold = 0.6f;
    WheelNotchAngleRad = UE_PI / 12.0f; // π/12 ≒ 0.2618rad（15度）

    LastAngle = 0.0f;
    AccumulatedAngleRad = 0.0f;
    RotationStartTime = 0.0;
    bIsMeasuringRotation = false;
    bIsStickTracking = false;
    TargetRevolutionCount = 10;
    CurrentRevolutionCount = 0;
}

void UFishingReelStateComponent::EnterState()
{
    Super::EnterState();

    // ステート開始時にリール状態を初期化
    ResetRevolutionCount();
}

void UFishingReelStateComponent::UpdateState(float DeltaTime)
{
    Super::UpdateState(DeltaTime);
}

void UFishingReelStateComponent::ExitState()
{
    Super::ExitState();

    //// 終了時にリール状態をリセット
    //ResetRevolutionCount();
}

void UFishingReelStateComponent::ResetRevolutionCount()
{
    CurrentRevolutionCount = 0;
    AccumulatedAngleRad = 0.0f;
    bIsMeasuringRotation = false;
    bIsStickTracking = false;
}

void UFishingReelStateComponent::SimulateReelByStick(FVector2D StickInput)
{
    //// 非アクティブ時は入力を無視
    //if (!IsActive()) {
    //    return;
    //}

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
        CalculateRPM(DeltaAngle);
    }

    // 次フレーム計算用に現在の角度を保存
    LastAngle = CurrentAngle;
}

void UFishingReelStateComponent::SimulateReelByWheel()
{
    //// 非アクティブ時は入力を無視
    //if (!IsActive()) {
    //    return;
    //}

    // ホイール1ノッチ分の回転角（固定）を流し込む
    CalculateRPM(WheelNotchAngleRad);
}

void UFishingReelStateComponent::CalculateRPM(float DeltaAngle)
{
    const UWorld* World = GetWorld();
    if (!World) {
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
        // 経過時間を算出
        const double DeltaTime = CurrentTime - RotationStartTime;

        // 0で割るのを防止
        if (DeltaTime > 0.001) {
            // 1Min = 60秒で何回転できるか（RPM）を計算
            const float CalculatedRPM = static_cast<float>(60.0 / DeltaTime);
            // 算出したRPMをバインド先へ通知
            OnRPMCalculated.Broadcast(CalculatedRPM);
        }

        // 累積角度と時間をリセット
        AccumulatedAngleRad -= OneRevolutionRad;   // 誤差の蓄積を防ぐため端数は残す
        RotationStartTime = CurrentTime;

        // 回転数を加算し、目標に達したら通知
        CurrentRevolutionCount++;
        if (CurrentRevolutionCount >= TargetRevolutionCount) {
            OnFishingStateCompleted.Broadcast(true);
        }
    }
}