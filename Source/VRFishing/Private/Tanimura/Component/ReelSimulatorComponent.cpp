// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Component/ReelSimulatorComponent.h"

// Sets default values for this component's properties
UReelSimulatorComponent::UReelSimulatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;  // Tickオフ

    StickThreshold = 0.6f;
    WheelNotchAngleRad = UE_PI / 12.0f; // π/12　≒ 0.2618rad（15度）

    LastAngle = 0.0f;
    AccumulatedAngleRad = 0.0f;
    RevTime = 0.0f;
    bIsTracking = false;
}

void UReelSimulatorComponent::SimulateReelByStick(FVector2D StickInput, float DeltaTime)
{
    // 入力値が閾値未満なら追跡しない
    if (StickInput.SizeSquared() < FMath::Square(StickThreshold)) {
        bIsTracking = false;
        return;
    }

    // 入力ベクトルから現在の角度（-π〜π）を算出
    const float CurrentAngle = FMath::Atan2(StickInput.Y, StickInput.X);

    // 追跡開始時は角度の記録のみ行う
    if (!bIsTracking) {
        LastAngle = CurrentAngle;
        bIsTracking = true;
        return;
    }

    // 前フレームからの角度変化量を計算
    const float DeltaAngle = FMath::FindDeltaAngleRadians(LastAngle, CurrentAngle);

	// 順方向回転のみRPMの算出対象とする
    if (DeltaAngle > 0.0f) {
        CalculateRPM(DeltaAngle, DeltaTime);
    }

    // 次フレーム計算用に現在の角度を保存
    LastAngle = CurrentAngle;
}

void UReelSimulatorComponent::SimulateReelByWheel(float DeltaTime)
{
    // ホイール操作時はスティックの追跡状態をリセット
    bIsTracking = false;

    // ホイール1ノッチ分の固定回転角を流し込む
    CalculateRPM(WheelNotchAngleRad, DeltaTime);
}

void UReelSimulatorComponent::CalculateRPM(float DeltaAngle, float DeltaTime)
{
    // 角度変化量を累積
    AccumulatedAngleRad += DeltaAngle;
    // 経過時間を累積
    RevTime += DeltaTime;

    // 累積角度が1回転（2π）に達したか判定
    const float OneRevolutionRad = UE_TWO_PI;   // 2π ≒ 6.28318530717f
    if (AccumulatedAngleRad >= OneRevolutionRad) {
        // 0で割るのを防止してRPMを算出・通知
        if (RevTime > 0.001f) {
            const float CalculatedRPM = 60.0f / RevTime;
            OnRPMCalculated.Broadcast(CalculatedRPM);
        }

        // 累積角度と時間をリセット
        AccumulatedAngleRad -= OneRevolutionRad;   // 誤差の蓄積を防ぐため端数は残す
        RevTime = 0.0f;
    }
}

//void UReelSimulatorComponent::SimulateReelByKey(float InputRPM)
//{
//    // 引数で受け取ったRPMの値をそのままデリゲートで通知
//    OnRPMCalculated.Broadcast(InputRPM);
//}