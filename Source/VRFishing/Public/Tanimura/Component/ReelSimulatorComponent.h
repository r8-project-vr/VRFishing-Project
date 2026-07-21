// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReelSimulatorComponent.generated.h"

// RPMが算出されたことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPMCalculated, float, NewRPM);

/**
 * MetaQuestコントローラのスティック回転や、マウスホイールの回転から
 * 擬似的にエルゴメーターのRPMを算出するデバッグ用コンポーネント
 */

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UReelSimulatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UReelSimulatorComponent();

	// スティック入力を基にRPMシミュレーションを実行
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void SimulateReelByStick(FVector2D StickInput, float DeltaTime);

	// マウスホイールが1ノッチ回るたびに呼ばれ、RPMシミュレーションを実行
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void SimulateReelByWheel(float DeltaTime);

	//// キーボードから直接RPMの数値を入力する
	//UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	//void SimulateReelByKey(float InputRPM);

	// RPM算出時に実行されるイベント
	UPROPERTY(BlueprintAssignable, Category = "Reel Simulator")
	FOnRPMCalculated OnRPMCalculated;

protected:
	// スティックのデッドゾーン
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config")
	float StickThreshold;

	// ホイール1ノッチ入力あたりの回転角度（ラジアン）
	// デフォルトは15度 ≒ 0.2618rad
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Debug", meta = (ClampMin = "0.01"))
	float WheelNotchAngleRad;

private:
	// 角度変化量が1回転に達したらRPMを算出し、デリゲートを呼び出す
	void CalculateRPM(float DeltaAngle, float DeltaTime);

	float	LastAngle;				// 前フレームの入力角度（ラジアン）
	float	AccumulatedAngleRad;	// 累積角度（ラジアン）
	float	RevTime;				// 現在の回転の経過時間（秒）
	bool	bIsTracking;			// 入力を追跡中かどうか
};
