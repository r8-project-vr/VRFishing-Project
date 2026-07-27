// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingReelStateComponent.generated.h"

// RPMが算出されたことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPMCalculated, float, NewRPM);

// 目標回転数に達したことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetRevolutionsReached);

/**
 * 掛かった魚を巻き上げるためのリール操作を管理するステートコンポーネント
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingReelStateComponent : public UFishingStateComponentBase
{
	GENERATED_BODY()

public:
	UFishingReelStateComponent();

	// 基底クラスオーバーライド
	virtual void EnterState() override;
	virtual void UpdateState(float DeltaTime) override;
	virtual void ExitState() override;

	// スティック入力を基にRPMシミュレーションを実行
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void SimulateReelByStick(FVector2D StickInput);

	// マウスホイールが1ノッチ回るたびに呼ばれ、RPMシミュレーションを実行
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void SimulateReelByWheel();

	// 回転カウントを初期化する関数
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void ResetRevolutionCount();

	// 目標回転数到達イベント
	UPROPERTY(BlueprintAssignable, Category = "Reel Simulator")
	FOnTargetRevolutionsReached OnTargetRevolutionsReached;

	// RPM算出時に実行されるイベント
	UPROPERTY(BlueprintAssignable, Category = "Reel Simulator")
	FOnRPMCalculated OnRPMCalculated;

protected:
	// スティックのデッドゾーン
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config")
	float StickThreshold;

	// ホイール1ノッチ入力あたりの回転角度（ラジアン）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Debug", meta = (ClampMin = "0.01"))
	float WheelNotchAngleRad;

	// 目標回転数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator", meta = (ClampMin = "1"))
	int32 TargetRevolutionCount;

	// 現在の累積回転数
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reel Simulator")
	int32 CurrentRevolutionCount;

private:
	// 角度変化量が1回転に達したらRPMを算出し、デリゲートを呼び出す
	void CalculateRPM(float DeltaAngle);

	float	LastAngle;				// 前フレームの入力角度（ラジアン）
	float	AccumulatedAngleRad;	// 累積角度（ラジアン）
	double	RotationStartTime;		// 回転の計測を開始した時間（秒）
	bool	bIsMeasuringRotation;	// 計測が開始されているかのフラグ
	bool	bIsStickTracking;		// 入力を追跡中かどうか（スティック操作時用）
};
