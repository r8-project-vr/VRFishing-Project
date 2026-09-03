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

	// --- RPS 決定 ---
	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 有効 RPS は絶対値へ正規化して常に正方向へ進める（詳細はヘッダの撤去コメント参照）。
	// 以前のコード（消さずにコメントで残す）:
	// float Rps = Sample.Rps * RpsScale;
	// if (bRespectDeviceDirection && Sample.Direction == 0)
	// {
	//     Rps = -Rps;
	// }
	const float Rps = FMath::Abs(Sample.Rps) * RpsScale;
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// --- Debug 表示（毎フレーム必ず出す） ---
	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// データ停止中に行ごと消えると「未踩踏/断線/無効」の区別が付かないため、
	// 鮮度判定の前へ移動して常時表示に変更（停止中は灰色で状態を明示）。
	if (bShowDebug && GEngine)
	{
		const FString SourceText = Subsystem->IsSimulatorMode()
			? TEXT("SIM")
			: FString::Printf(TEXT("COM%d"), Subsystem->GetDeviceComPort(EFishingWiredDeviceType::Bicycle));
		if (bFresh)
		{
			// 2026.09.03 Lee(2): RPM 表示を RPS×60 の算出値からデバイス 0x21 応答の生値へ変更（診断比較用）
			FString DebugMsg = FString::Printf(
				TEXT("[BicycleBridge] RPS(0x22): %.2f | RPM(0x21): %.0f | θ: %.1f° | Age: %.2fs | Dir:%d | %s"),
				Rps, Sample.Rpm, FMath::RadiansToDegrees(VirtualAngleRad), Sample.AgeSeconds, Sample.Direction, *SourceText);
			GEngine->AddOnScreenDebugMessage(31, 0.0f, FColor::Orange, DebugMsg);
		}
		else
		{
			const FString DebugMsg = Sample.bValid
				? FString::Printf(TEXT("[BicycleBridge] データ停止中 (Age: %.2fs 超過) | %s"), Sample.AgeSeconds, *SourceText)
				: FString::Printf(TEXT("[BicycleBridge] データ無し (未受信) | %s"), *SourceText);
			GEngine->AddOnScreenDebugMessage(31, 0.0f, FColor::Silver, DebugMsg);
		}
	}
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	if (!bFresh)
	{
		// データ無し/古い → 注入を止める（Reel 状態は入力途絶により自然に遅すぎ判定へ向かう）
		return;
	}

	// --- 仮想スティック角度を進めて注入（ベクトル角差がそのまま Δangle → RPM 判定へ流れる） ---
	// 2026.09.03 Lee: 旧 bInvertVirtualRotation の θ 符号反転は毎フレーム ±振動を生むバグ実装のため削除。
	VirtualAngleRad += Rps * 2.0f * PI * DeltaTime;
	const FVector2D VirtualStick(FMath::Cos(VirtualAngleRad), FMath::Sin(VirtualAngleRad));
	Pawn->InjectReelStickInput(VirtualStick);
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
