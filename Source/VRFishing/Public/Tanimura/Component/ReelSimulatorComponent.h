// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReelSimulatorComponent.generated.h"

// RPMが算出されたことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPMCalculated, float, NewRPM);

/**
 * コントローラーのスティック回転から擬似的にエルゴメーターのRPMを算出するデバッグ用コンポーネント
 */

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UReelSimulatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UReelSimulatorComponent();

	// スティック入力と経過時間からRPM計算を更新
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void UpdateReelSimulation(FVector2D StickInput, float DeltaTime);

	// RPM算出時に実行されるイベント
	UPROPERTY(BlueprintAssignable, Category = "Reel Simulator")
	FOnRPMCalculated OnRPMCalculated;

protected:
	// スティックのデッドゾーン
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config")
	float StickThreshold;

private:
	// 前フレームの入力角度（ラジアン）
	float LastAngle;

	// 累積角度（ラジアン）
	float AccumulatedAngle;

	// 1回転にかかった計測時間（秒）
	float ElapsedTimeSinceLastRevolution;

	// 現在スティック入力を追跡中かどうかのフラグ
	bool bIsTracking;
};
