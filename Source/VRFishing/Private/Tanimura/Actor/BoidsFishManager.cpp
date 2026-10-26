// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Tanimura/Actor/BoidsFishManager.h"
#include "Tanimura/Actor/BoidsFish.h"
#include "Tanimura/DataAsset/BoidsFishSettings.h"
#include "Tanimura/DataAsset/BoidsFishSettingsTable.h"
#include "Tanimura/Subsystem/FishingCatchSaveSubsystem.h"
#include "Takeuchi/Data/FishDataAsset.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "VRFishingLog.h"

ABoidsFishManager::ABoidsFishManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	BoundsVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsVolume"));
	BoundsVolume->SetupAttachment(RootScene);
	BoundsVolume->SetBoxExtent(FVector(2000.0f, 2000.0f, 1000.0f));
	BoundsVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsVolume->SetGenerateOverlapEvents(false);
	BoundsVolume->SetHiddenInGame(true);
	BoundsVolume->ShapeColor = FColor(0, 180, 255);
}

void ABoidsFishManager::BeginPlay()
{
	Super::BeginPlay();

	// 設定アセット未指定なら既定値で生成
	if (!IsValid(DefaultSettings)) {
		DefaultSettings = NewObject<UBoidsFishSettings>(this, FName(TEXT("RuntimeBoidsFishSettings")), RF_Transient);
	}

	if (bAutoRegisterAgents) {
		CollectAgents();
	}

	// 保存済みの釣果から水族館の魚を生成
	if (bSpawnFromCatchSave) {
		SpawnFishFromCatchSave();
	}
}

void ABoidsFishManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 登録中の魚からマネージャー参照を外す
	TArray<TObjectPtr<ABoidsFish>> RegisteredAgents = Agents;
	Agents.Reset();
	for (const TObjectPtr<ABoidsFish>& Agent : RegisteredAgents) {
		if (IsValid(Agent)) {
			Agent->UnregisterFromManager();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ABoidsFishManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ヒッチ時に魚が大きく飛ばないようDeltaTimeを制限
	const float SafeDeltaTime = FMath::Min(DeltaTime, MaxDeltaTime);
	UpdateFlock(SafeDeltaTime);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug) {
		DrawDebugInfo();
	}
#endif
}

void ABoidsFishManager::RegisterAgent(ABoidsFish* Agent)
{
	if (!IsValid(Agent)) {
		return;
	}

	Agents.AddUnique(Agent);
}

void ABoidsFishManager::UnregisterAgent(ABoidsFish* Agent)
{
	Agents.RemoveAll([Agent](const TObjectPtr<ABoidsFish>& RegisteredAgent) {
		return RegisteredAgent == Agent;
	});
}

int32 ABoidsFishManager::GetAgentCount() const
{
	return Agents.Num();
}

void ABoidsFishManager::SpawnFishFromCatchSave()
{
	if (!FishClass) {
		UE_LOG(LogFishing, Warning, TEXT("ABoidsFishManager::SpawnFishFromCatchSave: FishClass が未設定のため魚を生成できません。"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) {
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	UFishingCatchSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UFishingCatchSaveSubsystem>() : nullptr;
	if (!SaveSubsystem) {
		UE_LOG(LogFishing, Warning, TEXT("ABoidsFishManager::SpawnFishFromCatchSave: 釣果保存サブシステムを取得できませんでした。"));
		return;
	}

	// 保存済みの釣果を上限まで1匹ずつ生成する
	const TArray<FFishCatchSaveEntry> Entries = SaveSubsystem->GetCaughtFish();
	const int32 SpawnCount = FMath::Min(Entries.Num(), MaxSpawnCount);
	const FBox Bounds = GetBounds();
	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < SpawnCount; ++Index) {
		UFishDataAsset* FishData = Entries[Index].FishData.LoadSynchronous();
		if (!IsValid(FishData)) {
			UE_LOG(LogFishing, Warning, TEXT("ABoidsFishManager::SpawnFishFromCatchSave: 魚種データを読み込めないため %d 匹目を飛ばします。"), Index);
			continue;
		}

		const FVector SpawnLocation = GetRandomSpawnLocation(Bounds, SpawnMargin);
		const FRotator SpawnRotation = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABoidsFish* Fish = World->SpawnActor<ABoidsFish>(FishClass, SpawnLocation, SpawnRotation, SpawnParams);
		if (!IsValid(Fish)) {
			continue;
		}

		Fish->SetFishData(FishData);
		Fish->RegisterWithManager(this);
		++SpawnedCount;
	}

	UE_LOG(LogFishing, Log, TEXT("ABoidsFishManager::SpawnFishFromCatchSave: 保存済みの釣果 %d 件から %d 匹を生成しました。"), Entries.Num(), SpawnedCount);
}

void ABoidsFishManager::CollectAgents()
{
	for (TActorIterator<ABoidsFish> It(GetWorld()); It; ++It) {
		ABoidsFish* Agent = *It;

		// 他のマネージャーに所属している魚は除外
		if (IsValid(Agent) && !Agent->GetBoidsManager()) {
			Agent->RegisterWithManager(this);
		}
	}
}

void ABoidsFishManager::UpdateFlock(float DeltaTime)
{
	// 破棄された魚を登録から除去
	Agents.RemoveAll([](const TObjectPtr<ABoidsFish>& Agent) {
		return !IsValid(Agent);
	});

	const int32 AgentCount = Agents.Num();
	if (AgentCount == 0) {
		return;
	}

	// 設定未生成のままTickした場合の保険
	if (!IsValid(DefaultSettings)) {
		DefaultSettings = NewObject<UBoidsFishSettings>(this, FName(TEXT("RuntimeBoidsFishSettings")), RF_Transient);
	}

	// 全魚の位置と速度を1フレーム分のスナップショットとして取得
	Positions.SetNumUninitialized(AgentCount);
	Velocities.SetNumUninitialized(AgentCount);
	Accelerations.SetNumZeroed(AgentCount);
	AgentSettings.SetNumUninitialized(AgentCount);
	AgentSpecies.SetNumUninitialized(AgentCount);

	for (int32 Index = 0; Index < AgentCount; ++Index) {
		const ABoidsFish* Agent = Agents[Index];
		Positions[Index] = Agent->GetActorLocation();
		Velocities[Index] = Agent->GetBoidsVelocity();
		AgentSettings[Index] = ResolveSettings(Agent);
		AgentSpecies[Index] = Agent->FishData;
	}

	// 位置更新前に全魚の操舵力を計算する
	const FBox Bounds = GetBounds();
	for (int32 Index = 0; Index < AgentCount; ++Index) {
		Accelerations[Index] = ComputeSteering(Index, *AgentSettings[Index], Bounds);
	}

	// 操舵力を積分して位置と向きを更新
	for (int32 Index = 0; Index < AgentCount; ++Index) {
		ABoidsFish* Agent = Agents[Index];
		const UBoidsFishSettings& Settings = *AgentSettings[Index];

		FVector NewVelocity = Velocities[Index] + Accelerations[Index] * DeltaTime;
		const float Speed = NewVelocity.Size();
		const float EffectiveMinSpeed = FMath::Min(Settings.MinSpeed, Settings.MaxSpeed);
		if (Speed > Settings.MaxSpeed && Speed > UE_KINDA_SMALL_NUMBER) {
			NewVelocity *= Settings.MaxSpeed / Speed;
		}
		else if (Speed < EffectiveMinSpeed && Speed > UE_KINDA_SMALL_NUMBER) {
			NewVelocity *= EffectiveMinSpeed / Speed;
		}

		// 境界外へ出ないよう位置を箱内に収める
		FVector NewLocation = Positions[Index] + NewVelocity * DeltaTime;
		for (int32 Axis = 0; Axis < 3; ++Axis) {
			NewLocation[Axis] = FMath::Clamp(NewLocation[Axis], Bounds.Min[Axis], Bounds.Max[Axis]);
		}

		FRotator NewRotation = Agent->GetActorRotation();
		if (Agent->bOrientToVelocity && NewVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER) {
			const FRotator TargetRotation = NewVelocity.Rotation();
			NewRotation = FMath::RInterpTo(NewRotation, TargetRotation, DeltaTime, Settings.TurnInterpSpeed);
		}

		Agent->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::None);
		Agent->SetBoidsVelocity(NewVelocity);
		Agent->OnBoidsUpdated(DeltaTime, NewVelocity.Size());
	}
}

FVector ABoidsFishManager::ComputeSteering(int32 AgentIndex, const UBoidsFishSettings& Settings, const FBox& Bounds) const
{
	const FVector Position = Positions[AgentIndex];
	const FVector Velocity = Velocities[AgentIndex];
	const UFishDataAsset* Species = AgentSpecies[AgentIndex];

	// 進行方向を基準に前方視野を決める
	const FVector Forward = Velocity.GetSafeNormal();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(Settings.PerceptionHalfAngle));

	FVector CohesionSum = FVector::ZeroVector;
	FVector SeparationSum = FVector::ZeroVector;
	FVector AlignmentSum = FVector::ZeroVector;
	int32 CohesionCount = 0;
	int32 SeparationCount = 0;
	int32 AlignmentCount = 0;

	const float MinSeparationDistSq = FMath::Square(FMath::Max(Settings.SeparationMinDistance, 1.0f));

	// 全魚との距離を調べ、前方視野とルールごとの半径で集計する
	for (int32 OtherIndex = 0; OtherIndex < Positions.Num(); ++OtherIndex) {
		if (OtherIndex == AgentIndex) {
			continue;
		}

		const FVector ToOther = Positions[OtherIndex] - Position;
		const float DistSq = ToOther.SizeSquared();
		const float Dist = FMath::Sqrt(DistSq);

		// 群れの対象は同種の魚のみに限定
		const bool bSameSpecies = AgentSpecies[OtherIndex] == Species;

		// 自分から見て相手が前方にいるか判定
		const FVector ToOtherDir = ToOther / FMath::Max(Dist, UE_KINDA_SMALL_NUMBER);
		const bool bInFront = Forward.IsNearlyZero() || FVector::DotProduct(Forward, ToOtherDir) >= CosHalfAngle;

		// 結合: 同種の前方の近傍の位置を合計して重心を求める
		if (bSameSpecies && bInFront && Dist <= Settings.CohesionRadius) {
			CohesionSum += Positions[OtherIndex];
			++CohesionCount;
		}

		// 分離: 近い相手ほど強く反発する。魚種を問わず全方向を見る
		if (Dist <= Settings.SeparationRadius) {
			SeparationSum -= ToOther / FMath::Max(DistSq, MinSeparationDistSq);
			++SeparationCount;
		}

		// 整列: 同種の前方の近傍の速度を合計して平均を求める
		if (bSameSpecies && bInFront && Dist <= Settings.AlignmentRadius) {
			AlignmentSum += Velocities[OtherIndex];
			++AlignmentCount;
		}
	}

	FVector Steering = FVector::ZeroVector;

	// 結合: 群れの中心へ向かう力
	if (CohesionCount > 0) {
		const FVector Center = CohesionSum / static_cast<float>(CohesionCount);
		Steering += (Center - Position) * Settings.CohesionWeight;
	}

	// 分離: 近傍から離れる力
	if (SeparationCount > 0) {
		Steering += (SeparationSum / static_cast<float>(SeparationCount)) * Settings.SeparationWeight;
	}

	// 整列: 近傍の平均速度へ合わせる力
	if (AlignmentCount > 0) {
		const FVector AverageVelocity = AlignmentSum / static_cast<float>(AlignmentCount);
		Steering += (AverageVelocity - Velocity) * Settings.AlignmentWeight;
	}

	// 境界の内側へ戻す力を加える
	Steering += ComputeBoundaryForce(Position, Settings, Bounds) * Settings.BoundaryWeight;

	// 過大な加速度を制限
	return Steering.GetClampedToMaxSize(Settings.MaxAcceleration);
}

FVector ABoidsFishManager::ComputeBoundaryForce(const FVector& Position, const UBoidsFishSettings& Settings, const FBox& Bounds) const
{
	const FVector Extent = Bounds.Max - Bounds.Min;
	const float Margin = FMath::Max(Settings.BoundaryMargin, 1.0f);
	FVector Force = FVector::ZeroVector;

	for (int32 Axis = 0; Axis < 3; ++Axis) {
		// 箱が薄い軸ではマージンを内側に収める
		const float AxisMargin = FMath::Min(Margin, static_cast<float>(Extent[Axis]) * 0.5f);
		if (AxisMargin <= UE_KINDA_SMALL_NUMBER) {
			continue;
		}

		const float InnerMin = static_cast<float>(Bounds.Min[Axis]) + AxisMargin;
		const float InnerMax = static_cast<float>(Bounds.Max[Axis]) - AxisMargin;
		if (Position[Axis] < InnerMin) {
			Force[Axis] += (InnerMin - Position[Axis]) / AxisMargin;
		}
		else if (Position[Axis] > InnerMax) {
			Force[Axis] -= (Position[Axis] - InnerMax) / AxisMargin;
		}
	}

	return Force;
}

const UBoidsFishSettings* ABoidsFishManager::ResolveSettings(const ABoidsFish* Agent) const
{
	if (IsValid(Agent)) {
		// 個別指定を最優先
		if (IsValid(Agent->SettingsOverride)) {
			return Agent->SettingsOverride;
		}

		// 魚種ごとの設定を次に優先
		if (IsValid(SettingsTable)) {
			if (const UBoidsFishSettings* SpeciesSettings = SettingsTable->FindSettings(Agent->FishData)) {
				return SpeciesSettings;
			}
		}
	}

	return DefaultSettings;
}

FBox ABoidsFishManager::GetBounds() const
{
	if (!IsValid(BoundsVolume)) {
		return FBox(FVector(-1000.0f), FVector(1000.0f));
	}

	const FVector Center = BoundsVolume->GetComponentLocation();
	const FVector Extent = BoundsVolume->GetScaledBoxExtent();
	return FBox(Center - Extent, Center + Extent);
}

FVector ABoidsFishManager::GetRandomSpawnLocation(const FBox& Bounds, float Margin) const
{
	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();
	const float SafeMargin = FMath::Max(Margin, 0.0f);
	FVector Location = Center;

	for (int32 Axis = 0; Axis < 3; ++Axis) {
		// 箱が薄い軸でも内側に収まるようマージンを制限
		const float AxisMargin = FMath::Min(SafeMargin, Extent[Axis] * 0.9f);
		const float Range = FMath::Max(Extent[Axis] - AxisMargin, 0.0f);
		Location[Axis] = Center[Axis] + FMath::FRandRange(-Range, Range);
	}

	return Location;
}

#if ENABLE_DRAW_DEBUG
void ABoidsFishManager::DrawDebugInfo() const
{
	const FBox Bounds = GetBounds();
	DrawDebugBox(GetWorld(), Bounds.GetCenter(), Bounds.GetExtent(), FColor::Cyan, false, -1.0f, 0, 2.0f);

	const int32 DrawCount = FMath::Min(Agents.Num(), DebugDrawAgentCount);
	for (int32 Index = 0; Index < DrawCount; ++Index) {
		const ABoidsFish* Agent = Agents[Index];
		if (!IsValid(Agent)) {
			continue;
		}

		const FVector Position = Agent->GetActorLocation();
		const FVector Velocity = Agent->GetBoidsVelocity();
		DrawDebugDirectionalArrow(GetWorld(), Position, Position + Velocity * 0.5f, 20.0f, FColor::Green, false, -1.0f, 0, 1.5f);

		if (const UBoidsFishSettings* Settings = ResolveSettings(Agent)) {
			DrawDebugSphere(GetWorld(), Position, Settings->SeparationRadius, 8, FColor(120, 120, 0), false, -1.0f, 0, 0.5f);
		}
	}
}
#endif
