// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/component/BicycleBridgeComponent.h"
#include "Lee/device/FishingWiredDeviceSubsystem.h"
#include "Tanimura/Actor/VRPawn.h"
#include "VRFishingLog.h"
#include "Engine/Engine.h"

UBicycleBridgeComponent::UBicycleBridgeComponent()
{
	// Reel 状態がアクティブな間のみ注入が実処理になるため、常駐 Tick で同期を取る
	PrimaryComponentTick.bCanEverTick = true;
}

void UBicycleBridgeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UFishingWiredDeviceSubsystem* Subsystem = FindSubsystem();
	AVRPawn* Pawn = Cast<AVRPawn>(GetOwner());
	if (!Subsystem || !Pawn)
	{
		return;
	}

	// --- 最新サンプル取得と鮮度判定 ---
	const FBicycleSample Sample = Subsystem->GetLatestBicycleSample();
	const bool bFresh = Sample.bValid && Sample.AgeSeconds <= DataStaleTimeoutSeconds;
	if (!bFresh)
	{
		// データ無し/古い → 注入を止める（Reel 状態は入力途絶により自然に遅すぎ判定へ向かう）
		return;
	}

	// --- RPS 決定（回転方向と反転設定を反映） ---
	float Rps = Sample.Rps * RpsScale;
	if (bRespectDeviceDirection && Sample.Direction == 0)
	{
		Rps = -Rps;
	}

	// --- 仮想スティック角度を進めて注入（ベクトル角差がそのまま Δangle → RPM 判定へ流れる） ---
	VirtualAngleRad += Rps * 2.0f * PI * DeltaTime;
	if (bInvertVirtualRotation)
	{
		VirtualAngleRad = -VirtualAngleRad;
	}
	const FVector2D VirtualStick(FMath::Cos(VirtualAngleRad), FMath::Sin(VirtualAngleRad));
	Pawn->InjectReelStickInput(VirtualStick);

	if (bShowDebug && GEngine)
	{
		const FString SourceText = Subsystem->IsSimulatorMode()
			? TEXT("SIM")
			: FString::Printf(TEXT("COM%d"), Subsystem->GetConnectedComPort());
		FString DebugMsg = FString::Printf(
			TEXT("[BicycleBridge] RPS: %.2f | RPM: %.0f | θ: %.1f° | Age: %.2fs | %s"),
			Rps, Rps * 60.0f, FMath::RadiansToDegrees(VirtualAngleRad), Sample.AgeSeconds, *SourceText);
		GEngine->AddOnScreenDebugMessage(31, 0.0f, FColor::Orange, DebugMsg);
	}
}

UFishingWiredDeviceSubsystem* UBicycleBridgeComponent::FindSubsystem() const
{
	if (!GetOwner())
	{
		return nullptr;
	}
	UGameInstance* GameInstance = GetOwner()->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UFishingWiredDeviceSubsystem>() : nullptr;
}
