// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FishDataAsset.generated.h"

class USkeletalMesh;
class UAnimInstance;

UCLASS(BlueprintType)
class VRFISHING_API UFishDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//エディタ上で表示する魚名
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Basic")
	FText FishName;

	//魚のSkeletalMesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Mesh")
	USkeletalMesh* FishMesh;

	//魚に使用するAnimationBlueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Mesh")
	TSubclassOf<UAnimInstance> AnimationClass;

	//魚モデルの表示倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Mesh")
	FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);

	//魚モデルの向きを調整する回転値
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Mesh")
	FRotator MeshRotationOffset = FRotator::ZeroRotator;

	//魚が通常時に維持しようとする速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Movement", meta = (ClampMin = "0.0"))
	float CruiseSpeed = 150.0f;

	//魚が出せる最大速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Movement", meta = (ClampMin = "0.0"))
	float MaxSpeed = 250.0f;

	//1秒間に変更できる速度の最大量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Movement", meta = (ClampMin = "0.0"))
	float MaxSteeringForce = 100.0f;

	//進行方向へ回転するときの補間速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Movement", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 3.0f;

	//上下方向の移動量。1.0で横方向と同じ強さ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VerticalMovementScale = 0.35f;

	//Boidアルゴリズムのルール
	//仲間として認識する最大距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Boid", meta = (ClampMin = "0.0"))
	float PerceptionRadius = 400.0f;

	//近づきすぎたと判断する距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Boid", meta = (ClampMin = "0.0"))
	float SeparationRadius = 100.0f;

	//近すぎる仲間から離れる力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Boid", meta = (ClampMin = "0.0"))
	float SeparationWeight = 2.0f;

	//仲間と進行方向を合わせる力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Boid", meta = (ClampMin = "0.0"))
	float AlignmentWeight = 1.0f;

	//仲間の中心へ近づく力
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Boid", meta = (ClampMin = "0.0"))
	float CohesionWeight = 1.0f;

	//一度に計算対象とする仲間の最大数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FishData|Boid", meta = (ClampMin = "1"))
	int32 MaxNeighbors = 20;
};
