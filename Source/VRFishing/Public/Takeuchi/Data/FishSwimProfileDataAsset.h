// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FishSwimProfileDataAsset.generated.h"

/**
 * 
 */
UCLASS()
class VRFISHING_API UFishSwimProfileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	//魚が通常時に維持しようとする速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Movement", meta = (ClampMin = "0.0"))
	float CruiseSpeed = 150.0f;

	//魚が出せる最大速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Movement", meta = (ClampMin = "0.0"))
	float MaxSpeed = 250.0f;

	//1秒間に変更できる速度の最大量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Movement", meta = (ClampMin = "0.0"))
	float MaxSteeringForce = 100.0f;

	//進行方向へ回転するときの補間速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Movement", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 3.0f;

	//上下方向の移動量。1.0で横方向と同じ強さ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VerticalMovementScale = 0.35f;

	//仲間として認識する最大距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Boid", meta = (ClampMin = "0.0"))
	float PerceptionRadius = 400.0f;

	//近づきすぎたと判断する距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Boid", meta = (ClampMin = "0.0"))
	float SeparationRadius = 100.0f;

	//近すぎる仲間から離れる力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Boid", meta = (ClampMin = "0.0"))
	float SeparationWeight = 2.0f;

	//仲間と進行方向を合わせる力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Boid", meta = (ClampMin = "0.0"))
	float AlignmentWeight = 1.0f;

	//仲間の中心へ近づく力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Boid", meta = (ClampMin = "0.0"))
	float CohesionWeight = 1.0f;

	//一度に計算対象とする仲間の最大数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Boid", meta = (ClampMin = "1"))
	int32 MaxNeighbors = 20;

	//時間によって群れへの参加状態を切り替えるか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Schooling")
	bool bEnableSchoolingCycle = false;

	//群れを作っている時間の最小値
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Schooling", meta = (EditCondition = "bEnableSchoolingCycle", ClampMin = "0.1"))
	float SchoolingDurationMin = 8.0f;

	//群れを作っている時間の最大値
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Schooling", meta = (EditCondition = "bEnableSchoolingCycle", ClampMin = "0.1"))
	float SchoolingDurationMax = 15.0f;

	//個別行動をしている時間の最小値
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Schooling", meta = (EditCondition = "bEnableSchoolingCycle", ClampMin = "0.1"))
	float LooseDurationMin = 10.0f;

	//個別行動をしている時間の最大値
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Schooling", meta = (EditCondition = "bEnableSchoolingCycle", ClampMin = "0.1"))
	float LooseDurationMax = 20.0f;

	//個別行動時にAlignmentへ掛ける倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Schooling", meta = (EditCondition = "bEnableSchoolingCycle", ClampMin = "0.0", ClampMax = "1.0"))
	float LooseAlignmentMultiplier = 0.2f;

	//個別行動時にCohesionへ掛ける倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SwimProfile|Schooling", meta = (EditCondition = "bEnableSchoolingCycle", ClampMin = "0.0", ClampMax = "1.0"))
	float LooseCohesionMultiplier = 0.05f;
};
