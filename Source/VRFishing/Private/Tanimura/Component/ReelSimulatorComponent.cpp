// Fill out your copyright notice in the Description page of Project Settings.


#include "Tanimura/Component/ReelSimulatorComponent.h"

// Sets default values for this component's properties
UReelSimulatorComponent::UReelSimulatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;  // Tickオフ

    // 各メンバ変数の初期値
    StickThreshold = 0.6f;
    AccumulatedAngle = 0.0f;
    RevTime = 0.0f;
    LastAngle = 0.0f;
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

    // 順方向回転のみ累積
    if (DeltaAngle > 0.0f) {
        AccumulatedAngle += DeltaAngle;
    }

    // 回転経過時間を加算
    RevTime += DeltaTime;

    // 累積角度が1回転（2π）に達したか判定
    const float OneRevolutionRad = UE_TWO_PI;   // = 2π（6.28318530717f）
    if (AccumulatedAngle >= OneRevolutionRad) {
        // 0で割るのを防止してRPMを算出・通知
        if (RevTime > 0.001f) {
            const float CalculatedRPM = 60.0f / RevTime;
            OnRPMCalculated.Broadcast(CalculatedRPM);
        }

        // 累積角度と時間をリセット
        AccumulatedAngle -= OneRevolutionRad;   // 誤差の蓄積を防ぐため端数は残す
        RevTime = 0.0f;
    }

    // 次フレーム計算用に現在の角度を保存
    LastAngle = CurrentAngle;
}

void UReelSimulatorComponent::SimulateReelByKey(float InputRPM)
{
    // 引数で受け取ったRPMの値をそのままデリゲートで通知
    OnRPMCalculated.Broadcast(InputRPM);
}