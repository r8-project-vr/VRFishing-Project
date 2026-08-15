// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Takeuchi/Actor/AquariumBoidManager.h"
#include "Takeuchi/Actor/AquariumBoidFish.h"
#include "Takeuchi/Data/FishDataAsset.h"
#include "Takeuchi/Data/FishSwimProfileDataAsset.h"
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
			UE_LOG(LogTemp,Warning,TEXT("AquariumBoidManager: SwimProfileが設定されていない魚データがあります。"));
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

			//生成順に群れを振り分ける
			const int32 ValidSchoolCount = FMath::Max(1, SpawnSettings.SchoolCount);
			SpawnedFishActor->SchoolId = FishIndex % ValidSchoolCount;

			//群れ状態と切り替え時間を初期化する
			InitializeSchoolingState(SpawnedFishActor);

			//向いている方向へ泳ぎ始める
			const FVector InitialDirection =SpawnRotation.Vector();

			SpawnedFishActor->Velocity = InitialDirection * SpawnSettings.FishData->SwimProfile->CruiseSpeed;

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

		if (!FishActor->FishData->SwimProfile)
		{
			continue;
		}

		UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;

		//時間経過によって群れ状態を更新する
		UpdateSchoolingState(FishActor, DeltaTime);

		//水槽境界から離れる力を計算する
		const FVector BoundarySteering = CalculateBoundarySteering(FishActor);

		//近づきすぎた同種の魚から離れる力を計算する
		FVector SeparationSteering = CalculateSeparationSteering(FishActor);

		//周囲にいる同種の魚と進行方向を揃える力を計算する
		FVector AlignmentSteering = CalculateAlignmentSteering(FishActor);

		//周囲にいる同種の魚の中心へ向かう力を計算する
		FVector CohesionSteering = CalculateCohesionSteering(FishActor);

		//魚の速度を巡航速度へ近づける力を計算する
		FVector CruiseSteering = CalculateCruiseSteering(FishActor);

		//魚ごとの上下移動の強さを反映する
		SeparationSteering.Z *= SwimProfile->VerticalMovementScale;
		AlignmentSteering.Z *= SwimProfile->VerticalMovementScale;
		CohesionSteering.Z *= SwimProfile->VerticalMovementScale;
		CruiseSteering.Z *= SwimProfile->VerticalMovementScale;

		//今回のフレームで加える力を合成する
		FVector TotalSteering = BoundarySteering + SeparationSteering + AlignmentSteering + CohesionSteering + CruiseSteering;

		//合計した力が強くなりすぎないよう制限する
		TotalSteering = TotalSteering.GetClampedToMaxSize( SwimProfile->MaxSteeringForce );

		//合成した力を速度へ反映する
		FishActor->Velocity += TotalSteering * DeltaTime;

		//魚ごとの最大速度を超えないよう制限する
		FishActor->Velocity = FishActor->Velocity.GetClampedToMaxSize( SwimProfile->MaxSpeed );

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

		const FRotator NewRotation =FMath::RInterpTo(CurrentRotation,TargetRotation,DeltaTime,SwimProfile->RotationInterpSpeed);

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

	if (!FishActor->FishData->SwimProfile)
	{
		return FVector::ZeroVector;
	}

	const UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;
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

		SteeringForce +=DirectionToCenter *SwimProfile->MaxSteeringForce *HorizontalUrgency;
	}

	//水面と底を避け始める高さ
	const float SafeHalfHeight =FMath::Max(0.0f, SwimHalfHeight - WallMargin);

	const float HeightOffset =FishLocation.Z - CenterLocation.Z;

	if (HeightOffset > SafeHalfHeight)
	{
		const float DistanceIntoMargin =HeightOffset - SafeHalfHeight;

		const float VerticalUrgency =FMath::Clamp(DistanceIntoMargin / FMath::Max(WallMargin, 1.0f),0.0f,1.0f);

		//水面に近い場合は下方向へ戻す
		SteeringForce.Z -=SwimProfile->MaxSteeringForce *VerticalUrgency;
	}

	else if (HeightOffset < -SafeHalfHeight)
	{
		const float DistanceIntoMargin =-SafeHalfHeight - HeightOffset;

		const float VerticalUrgency =FMath::Clamp(DistanceIntoMargin / FMath::Max(WallMargin, 1.0f),0.0f,1.0f);

		//底に近い場合は上方向へ戻す
		SteeringForce.Z +=SwimProfile->MaxSteeringForce *VerticalUrgency;
	}

	//斜め方向で力が強くなりすぎないよう制限する
	SteeringForce = SteeringForce.GetClampedToMaxSize(SwimProfile->MaxSteeringForce);

	return SteeringForce;
}

FVector AAquariumBoidManager::CalculateSeparationSteering( const AAquariumBoidFish* FishActor) const
{
	if (!FishActor)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData->SwimProfile)
	{
		return FVector::ZeroVector;
	}

	const UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;

	if (SwimProfile->SeparationRadius <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	const FVector FishLocation = FishActor->GetActorLocation();

	FVector SeparationDirection = FVector::ZeroVector;

	int32 NeighborCount = 0;

	for (AAquariumBoidFish* OtherFish : SpawnedFish)
	{
		if (!IsValid(OtherFish))
		{
			continue;
		}

		//自分自身は対象にしない
		if (OtherFish == FishActor)
		{
			continue;
		}

		if (!OtherFish->FishData)
		{
			continue;
		}

		//同じFishDataを持つ魚だけを同種として扱う
		if (OtherFish->FishData != FishActor->FishData)
		{
			continue;
		}

		const FVector Difference = FishLocation - OtherFish->GetActorLocation();

		const float Distance = Difference.Size();

		//完全に同じ位置の場合は通常の方向を計算できない
		if (Distance <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		//十分離れている魚は対象にしない
		if (Distance >= SwimProfile->SeparationRadius)
		{
			continue;
		}

		//近い魚ほど強く離れる
		const float DistanceWeight =1.0f -Distance / SwimProfile->SeparationRadius;

		SeparationDirection +=Difference.GetSafeNormal() *DistanceWeight;

		NeighborCount++;

		if (NeighborCount >= SwimProfile->MaxNeighbors)
		{
			break;
		}
	}

	if (NeighborCount == 0)
	{
		return FVector::ZeroVector;
	}

	//周囲の魚から受けた方向を平均化する
	SeparationDirection /=static_cast<float>(NeighborCount);

	FVector SeparationForce =SeparationDirection.GetSafeNormal() *SwimProfile->MaxSteeringForce *SwimProfile->SeparationWeight;

	SeparationForce =SeparationForce.GetClampedToMaxSize(SwimProfile->MaxSteeringForce);

	return SeparationForce;
}

FVector AAquariumBoidManager::CalculateAlignmentSteering(const AAquariumBoidFish* FishActor) const
{
	if (!FishActor)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData->SwimProfile)
	{
		return FVector::ZeroVector;
	}

	const UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;

	if (SwimProfile->PerceptionRadius <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	const FVector FishLocation = FishActor->GetActorLocation();
	FVector AverageVelocity = FVector::ZeroVector;
	int32 NeighborCount = 0;

	for (AAquariumBoidFish* OtherFish : SpawnedFish)
	{
		if (!IsValid(OtherFish))
		{
			continue;
		}

		//自分自身は対象にしない
		if (OtherFish == FishActor)
		{
			continue;
		}

		if (!OtherFish->FishData)
		{
			continue;
		}

		//同じFishDataを使用する魚だけを同種として扱う
		if (OtherFish->FishData != FishActor->FishData)
		{
			continue;
		}

		//同じ群れに所属している魚だけを対象にする
		if (OtherFish->SchoolId != FishActor->SchoolId)
		{
			continue;
		}

		const float Distance = FVector::Distance(FishLocation, OtherFish->GetActorLocation());

		if (Distance > SwimProfile->PerceptionRadius)
		{
			continue;
		}

		AverageVelocity += OtherFish->Velocity;
		NeighborCount++;

		if (NeighborCount >= SwimProfile->MaxNeighbors)
		{
			break;
		}
	}

	if (NeighborCount == 0)
	{
		return FVector::ZeroVector;
	}

	//周囲の魚の平均速度を求める
	AverageVelocity /= static_cast<float>(NeighborCount);

	if (AverageVelocity.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	//周囲の魚と同じ方向へ進む目標速度を作る
	const FVector DesiredVelocity = AverageVelocity.GetSafeNormal() * SwimProfile->CruiseSpeed;

	//現在速度と目標速度の差を操舵力にする
	FVector AlignmentForce = DesiredVelocity - FishActor->Velocity;

	float SchoolingMultiplier = 1.0f;

	//個別行動中はAlignmentを弱める
	if (SwimProfile->bEnableSchoolingCycle)
	{
		if (!FishActor->bIsSchooling)
		{
			SchoolingMultiplier = SwimProfile->LooseAlignmentMultiplier;
		}
	}

	AlignmentForce *= SwimProfile->AlignmentWeight * SchoolingMultiplier;
	AlignmentForce = AlignmentForce.GetClampedToMaxSize(SwimProfile->MaxSteeringForce);

	return AlignmentForce;
}

FVector AAquariumBoidManager::CalculateCohesionSteering(const AAquariumBoidFish* FishActor) const
{
	if (!FishActor)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData->SwimProfile)
	{
		return FVector::ZeroVector;
	}

	const UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;

	if (SwimProfile->PerceptionRadius <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	const FVector FishLocation = FishActor->GetActorLocation();
	FVector NeighborCenter = FVector::ZeroVector;
	int32 NeighborCount = 0;

	for (AAquariumBoidFish* OtherFish : SpawnedFish)
	{
		if (!IsValid(OtherFish))
		{
			continue;
		}

		//自分自身は対象にしない
		if (OtherFish == FishActor)
		{
			continue;
		}

		if (!OtherFish->FishData)
		{
			continue;
		}

		//同じFishDataを使用する魚だけを同種として扱う
		if (OtherFish->FishData != FishActor->FishData)
		{
			continue;
		}

		//同じ群れに所属している魚だけを対象にする
		if (OtherFish->SchoolId != FishActor->SchoolId)
		{
			continue;
		}

		const FVector OtherFishLocation = OtherFish->GetActorLocation();
		const float Distance = FVector::Distance(FishLocation, OtherFishLocation);

		if (Distance > SwimProfile->PerceptionRadius)
		{
			continue;
		}

		NeighborCenter += OtherFishLocation;
		NeighborCount++;

		if (NeighborCount >= SwimProfile->MaxNeighbors)
		{
			break;
		}
	}

	if (NeighborCount == 0)
	{
		return FVector::ZeroVector;
	}

	//周囲にいる同種の魚の中心位置を求める
	NeighborCenter /= static_cast<float>(NeighborCount);

	const FVector DirectionToCenter = NeighborCenter - FishLocation;

	if (DirectionToCenter.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	//群れの中心へ向かう目標速度を作る
	const FVector DesiredVelocity = DirectionToCenter.GetSafeNormal() * SwimProfile->CruiseSpeed;

	//現在速度と目標速度の差を操舵力にする
	FVector CohesionForce = DesiredVelocity - FishActor->Velocity;

	float SchoolingMultiplier = 1.0f;

	//個別行動中はCohesionを弱める
	if (SwimProfile->bEnableSchoolingCycle)
	{
		if (!FishActor->bIsSchooling)
		{
			SchoolingMultiplier = SwimProfile->LooseCohesionMultiplier;
		}
	}

	CohesionForce *= SwimProfile->CohesionWeight * SchoolingMultiplier;
	CohesionForce = CohesionForce.GetClampedToMaxSize(SwimProfile->MaxSteeringForce);

	return CohesionForce;
}

FVector AAquariumBoidManager::CalculateCruiseSteering(const AAquariumBoidFish* FishActor) const
{
	if (!FishActor)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData)
	{
		return FVector::ZeroVector;
	}

	if (!FishActor->FishData->SwimProfile)
	{
		return FVector::ZeroVector;
	}

	const UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;

	if (SwimProfile->CruiseSpeed <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	FVector CurrentDirection = FishActor->Velocity.GetSafeNormal();

	//停止している場合はActorの前方向を使用する
	if (CurrentDirection.IsNearlyZero())
	{
		CurrentDirection = FishActor->GetActorForwardVector();
	}

	//現在の進行方向を維持した巡航速度を作る
	const FVector DesiredVelocity = CurrentDirection * SwimProfile->CruiseSpeed;

	//現在速度と巡航速度との差を操舵力にする
	FVector CruiseForce = DesiredVelocity - FishActor->Velocity;
	CruiseForce = CruiseForce.GetClampedToMaxSize(SwimProfile->MaxSteeringForce);

	return CruiseForce;
}

void AAquariumBoidManager::InitializeSchoolingState(AAquariumBoidFish* FishActor) const
{
	if (!FishActor)
	{
		return;
	}

	if (!FishActor->FishData)
	{
		return;
	}

	if (!FishActor->FishData->SwimProfile)
	{
		return;
	}

	const UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;

	//周期切り替えを使用しない魚は常に群れ状態にする
	if (!SwimProfile->bEnableSchoolingCycle)
	{
		FishActor->bIsSchooling = true;
		FishActor->SchoolingStateRemainingTime = 0.0f;
		return;
	}

	//開始時は群れ状態と個別状態をランダムに決める
	FishActor->bIsSchooling = FMath::RandBool();

	if (FishActor->bIsSchooling)
	{
		const float MinimumDuration = FMath::Min(SwimProfile->SchoolingDurationMin, SwimProfile->SchoolingDurationMax);
		const float MaximumDuration = FMath::Max(SwimProfile->SchoolingDurationMin, SwimProfile->SchoolingDurationMax);
		FishActor->SchoolingStateRemainingTime = FMath::FRandRange(MinimumDuration, MaximumDuration);
	}
	else
	{
		const float MinimumDuration = FMath::Min(SwimProfile->LooseDurationMin, SwimProfile->LooseDurationMax);
		const float MaximumDuration = FMath::Max(SwimProfile->LooseDurationMin, SwimProfile->LooseDurationMax);
		FishActor->SchoolingStateRemainingTime = FMath::FRandRange(MinimumDuration, MaximumDuration);
	}
}

void AAquariumBoidManager::UpdateSchoolingState(AAquariumBoidFish* FishActor, float DeltaTime) const
{
	if (!FishActor)
	{
		return;
	}

	if (!FishActor->FishData)
	{
		return;
	}

	if (!FishActor->FishData->SwimProfile)
	{
		return;
	}

	const UFishSwimProfileDataAsset* SwimProfile = FishActor->FishData->SwimProfile;

	if (!SwimProfile->bEnableSchoolingCycle)
	{
		FishActor->bIsSchooling = true;
		return;
	}

	FishActor->SchoolingStateRemainingTime -= DeltaTime;

	if (FishActor->SchoolingStateRemainingTime > 0.0f)
	{
		return;
	}

	//時間が来たら群れ状態と個別状態を切り替える
	FishActor->bIsSchooling = !FishActor->bIsSchooling;

	if (FishActor->bIsSchooling)
	{
		const float MinimumDuration = FMath::Min(SwimProfile->SchoolingDurationMin, SwimProfile->SchoolingDurationMax);
		const float MaximumDuration = FMath::Max(SwimProfile->SchoolingDurationMin, SwimProfile->SchoolingDurationMax);
		FishActor->SchoolingStateRemainingTime = FMath::FRandRange(MinimumDuration, MaximumDuration);
	}
	else
	{
		const float MinimumDuration = FMath::Min(SwimProfile->LooseDurationMin, SwimProfile->LooseDurationMax);
		const float MaximumDuration = FMath::Max(SwimProfile->LooseDurationMin, SwimProfile->LooseDurationMax);
		FishActor->SchoolingStateRemainingTime = FMath::FRandRange(MinimumDuration, MaximumDuration);
	}
}
