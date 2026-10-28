// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoidsFishSettings.generated.h"

/**
 * 魚群の操舵パラメータをまとめたデータアセット
 */
UCLASS(BlueprintType)
class VRFISHING_API UBoidsFishSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 結合の近傍半径（cm）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Neighborhood", meta = (ClampMin = "0.0", Units = "cm"))
	float CohesionRadius = 500.0f;

	// 分離の近傍半径（cm）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Neighborhood", meta = (ClampMin = "0.0", Units = "cm"))
	float SeparationRadius = 200.0f;

	// 整列の近傍半径（cm）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Neighborhood", meta = (ClampMin = "0.0", Units = "cm"))
	float AlignmentRadius = 350.0f;

	// 前方視野の半角（度）。小さいほど細長い群れになる
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Neighborhood", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PerceptionHalfAngle = 60.0f;

	// 分離力が発散しないよう距離の下限として使う値（cm）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Neighborhood", meta = (ClampMin = "1.0", Units = "cm"))
	float SeparationMinDistance = 10.0f;

	// 結合の重み（大きいほど群れの中心へ集まる）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Weights", meta = (ClampMin = "0.0"))
	float CohesionWeight = 1.0f;

	// 分離の重み（大きいほど魚同士が離れる）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Weights", meta = (ClampMin = "0.0"))
	float SeparationWeight = 600.0f;

	// 整列の重み（大きいほど周囲と同じ向きへ進む）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Weights", meta = (ClampMin = "0.0"))
	float AlignmentWeight = 1.0f;

	// 境界から内側へ押し戻す力の重み
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Boundary", meta = (ClampMin = "0.0"))
	float BoundaryWeight = 200.0f;

	// 境界の内側で押し戻しを始める距離（cm）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Boundary", meta = (ClampMin = "0.0", Units = "cm"))
	float BoundaryMargin = 250.0f;

	// 最大速度（cm/s）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxSpeed = 250.0f;

	// 最低速度（cm/s）。止まらないよう常に前進させる
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinSpeed = 40.0f;

	// 1フレームで加えられる加速度の上限（cm/s^2）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Movement", meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float MaxAcceleration = 600.0f;

	// 進行方向へ向き直る速さ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Movement", meta = (ClampMin = "0.0"))
	float TurnInterpSpeed = 3.0f;
};
