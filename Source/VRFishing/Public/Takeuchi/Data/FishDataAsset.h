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
};
