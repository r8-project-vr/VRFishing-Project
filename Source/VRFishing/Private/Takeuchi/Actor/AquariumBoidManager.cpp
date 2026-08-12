// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Takeuchi/Actor/AquariumBoidManager.h"
#include "Takeuchi/Actor/AquariumBoidFish.h"
#include "Takeuchi/Data/FishDataAsset.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

// Sets default values
AAquariumBoidManager::AAquariumBoidManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AAquariumBoidManager::BeginPlay()
{
	Super::BeginPlay();

	SpawnFish();
}

void AAquariumBoidManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//全ての魚を移動させる
	UpdateFishMovement(DeltaTime);

	//必要な場合だけ水槽範囲を表示する
	if (bDrawDebugBounds)
	{
		const FVector CenterLocation = GetSwimCenterLocation();

		const FVector CylinderBottom =CenterLocation - FVector::UpVector * SwimHalfHeight;

		const FVector CylinderTop =CenterLocation + FVector::UpVector * SwimHalfHeight;

		DrawDebugCylinder(GetWorld(),CylinderBottom,CylinderTop,SwimRadius,48,FColor::Cyan,false,0.0f,0,3.0f);
	}
}

FVector AAquariumBoidManager::GetSwimCenterLocation() const
{
	//Manager自身の位置を初期値にする
	FVector CenterLocation = GetActorLocation();

	//中心Actorが設定されている場合は、その位置を使用する
	if (SwimCenterActor)
	{
		CenterLocation = SwimCenterActor->GetActorLocation();
	}

	return CenterLocation;
}

FVector AAquariumBoidManager::GetRandomSpawnLocation() const
{
	const FVector CenterLocation = GetSwimCenterLocation();

	//壁際を避けた生成可能範囲を計算する
	const float AvailableRadius = FMath::Max(0.0f, SwimRadius - WallMargin);

	const float AvailableHalfHeight = FMath::Max(0.0f, SwimHalfHeight - WallMargin);

	//円の面積に対して均等になるよう平方根を使用する
	const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);

	const float RandomRadius = FMath::Sqrt(FMath::FRand()) * AvailableRadius;

	const float RandomHeight = FMath::FRandRange(-AvailableHalfHeight, AvailableHalfHeight);

	const float OffsetX = FMath::Cos(RandomAngle) * RandomRadius;

	const float OffsetY = FMath::Sin(RandomAngle) * RandomRadius;

	const FVector RandomOffset(OffsetX, OffsetY, RandomHeight);

	return CenterLocation + RandomOffset;
}

void AAquariumBoidManager::SpawnFish()
{
	if (!FishClass)
	{
		UE_LOG(LogTemp, Warning,TEXT("AquariumBoidManager: FishClassが設定されていません。"));

		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	for (const FAquariumFishSpawnSettings& SpawnSettings : FishSpawnSettings)
	{
		if (!SpawnSettings.FishData)
		{
			continue;
		}

		for (int32 FishIndex = 0; FishIndex < SpawnSettings.SpawnCount; FishIndex++)
		{
			const FVector SpawnLocation = GetRandomSpawnLocation();

			//水平方向を中心にランダムな初期方向を作る
			const float RandomYaw =FMath::FRandRange(0.0f, 360.0f);

			const float RandomPitch =FMath::FRandRange(-10.0f, 10.0f);

			const FRotator SpawnRotation(RandomPitch,RandomYaw,0.0f);

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AAquariumBoidFish* SpawnedFishActor =World->SpawnActor<AAquariumBoidFish>(FishClass,SpawnLocation,SpawnRotation,SpawnParameters);

			if (!SpawnedFishActor)
			{
				continue;
			}

			//魚データを設定して見た目へ反映する
			SpawnedFishActor->FishData = SpawnSettings.FishData;
			SpawnedFishActor->ApplyFishData();

			//向いている方向へ泳ぎ始める
			const FVector InitialDirection =SpawnRotation.Vector();

			SpawnedFishActor->Velocity =InitialDirection * SpawnSettings.FishData->CruiseSpeed;

			SpawnedFish.Add(SpawnedFishActor);
		}
	}
}

void AAquariumBoidManager::UpdateFishMovement(float DeltaTime)
{
	for (AAquariumBoidFish* FishActor : SpawnedFish)
	{
		if (!IsValid(FishActor))
		{
			continue;
		}

		if (!FishActor->FishData)
		{
			continue;
		}

		//水槽境界を避ける力を速度へ加える
		const FVector BoundarySteering =CalculateBoundarySteering(FishActor);

		FishActor->Velocity += BoundarySteering * DeltaTime;

		//魚ごとの最大速度を超えないよう制限する
		FishActor->Velocity = FishActor->Velocity.GetClampedToMaxSize(FishActor->FishData->MaxSpeed);

		//現在の速度を使って次の位置を計算する
		const FVector CurrentLocation =FishActor->GetActorLocation();

		const FVector Movement =FishActor->Velocity * DeltaTime;

		const FVector NewLocation =CurrentLocation + Movement;

		FishActor->SetActorLocation(NewLocation);

		//速度がほぼ0の場合は回転を更新しない
		if (FishActor->Velocity.IsNearlyZero())
		{
			continue;
		}

		//魚の前方向を進行方向へ滑らかに合わせる
		const FRotator CurrentRotation =FishActor->GetActorRotation();

		const FRotator TargetRotation =FishActor->Velocity.Rotation();

		const FRotator NewRotation =FMath::RInterpTo(CurrentRotation,TargetRotation,DeltaTime,FishActor->FishData->RotationInterpSpeed);

		FishActor->SetActorRotation(NewRotation);
	}
}

FVector AAquariumBoidManager::CalculateBoundarySteering(const AAquariumBoidFish* FishActor) const
{
	if (!FishActor)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData)
	{
		return FVector::ZeroVector;
	}

	const FVector CenterLocation = GetSwimCenterLocation();
	const FVector FishLocation = FishActor->GetActorLocation();

	FVector SteeringForce = FVector::ZeroVector;

	//水平方向の中心からの差を計算する
	FVector HorizontalOffset = FishLocation - CenterLocation;
	HorizontalOffset.Z = 0.0f;

	const float HorizontalDistance = HorizontalOffset.Size();

	//壁を避け始める半径
	const float SafeRadius =FMath::Max(0.0f, SwimRadius - WallMargin);

	if (HorizontalDistance > SafeRadius)
	{
		//壁へ近づくほど中心へ戻る力を強くする
		const float DistanceIntoMargin =
			HorizontalDistance - SafeRadius;

		const float HorizontalUrgency =FMath::Clamp(DistanceIntoMargin / FMath::Max(WallMargin, 1.0f),0.0f,1.0f);

		const FVector DirectionToCenter =-HorizontalOffset.GetSafeNormal();

		SteeringForce +=DirectionToCenter *FishActor->FishData->MaxSteeringForce *HorizontalUrgency;
	}

	//水面と底を避け始める高さ
	const float SafeHalfHeight =FMath::Max(0.0f, SwimHalfHeight - WallMargin);

	const float HeightOffset =FishLocation.Z - CenterLocation.Z;

	if (HeightOffset > SafeHalfHeight)
	{
		const float DistanceIntoMargin =HeightOffset - SafeHalfHeight;

		const float VerticalUrgency =FMath::Clamp(DistanceIntoMargin / FMath::Max(WallMargin, 1.0f),0.0f,1.0f);

		//水面に近い場合は下方向へ戻す
		SteeringForce.Z -=FishActor->FishData->MaxSteeringForce *VerticalUrgency;
	}

	else if (HeightOffset < -SafeHalfHeight)
	{
		const float DistanceIntoMargin =-SafeHalfHeight - HeightOffset;

		const float VerticalUrgency =FMath::Clamp(DistanceIntoMargin / FMath::Max(WallMargin, 1.0f),0.0f,1.0f);

		//底に近い場合は上方向へ戻す
		SteeringForce.Z +=FishActor->FishData->MaxSteeringForce *VerticalUrgency;
	}

	//斜め方向で力が強くなりすぎないよう制限する
	SteeringForce = SteeringForce.GetClampedToMaxSize(FishActor->FishData->MaxSteeringForce);

	return SteeringForce;
}

