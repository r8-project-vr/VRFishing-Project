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
    RotationStartTime = 0.0;
    bIsMeasuringRotation = false;
    // RevTime = 0.0f;
    bIsStickTracking = false;
}

// === 追加：リセット処理の実装 ===
void UReelSimulatorComponent::ResetRevolutionCount()
{
    CurrentRevolutionCount = 0;
    AccumulatedAngleRad = 0.0f;
    bIsMeasuringRotation = false;
}

void UReelSimulatorComponent::SimulateReelByStick(FVector2D StickInput)
{
    //Lee 26.7.24 start
    // 非アクティブ時（非Reelingモード）は入力を無視
    if (!IsActive())
    {
        return;
    }
    //Lee 26.7.24 end


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

void UReelSimulatorComponent::SimulateReelByWheel()
{
    //Lee 26.7.24 start
    // 非アクティブ時（非Reelingモード）は入力を無視
    if (!IsActive())
    {
        return;
    }
    //Lee 26.7.24 end


    //// ホイール操作時はスティックの追跡状態をリセット
    //bIsStickTracking = false;

    // ホイール1ノッチ分の回転角（固定）を流し込む
    CalculateRPM(WheelNotchAngleRad);
}

void UReelSimulatorComponent::CalculateRPM(float DeltaAngle)
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
    //// 経過時間を累積
    //RevTime += DeltaTime;

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

        //// 0で割るのを防止してRPMを算出・通知
        //if (RevTime > 0.001f) {
        //    const float CalculatedRPM = 60.0f / RevTime;
        //    OnRPMCalculated.Broadcast(CalculatedRPM);
        //}

        // 累積角度と時間をリセット
        AccumulatedAngleRad -= OneRevolutionRad;   // 誤差の蓄積を防ぐため端数は残す
        RotationStartTime = CurrentTime;
        // RevTime = 0.0f;

        // === 追加：回転数を加算し、目標に達したら通知 ===
        CurrentRevolutionCount++;
        if (CurrentRevolutionCount >= TargetRevolutionCount) {
            OnTargetRevolutionsReached.Broadcast();
        }
    }
}

//void UReelSimulatorComponent::SimulateReelByKey(float InputRPM)
//{
//    // 引数で受け取ったRPMの値をそのままデリゲートで通知
//    OnRPMCalculated.Broadcast(InputRPM);
//}