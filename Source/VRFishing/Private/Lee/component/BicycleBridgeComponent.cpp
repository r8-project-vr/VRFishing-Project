// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/component/BicycleBridgeComponent.h"
#include "Lee/device/FishingWiredDeviceSubsystem.h"
// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Lee/device/FishingWiredDeviceSettings.h"
// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Tanimura/Actor/VRPawn.h"
// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Tanimura/Component/FishingReelStateComponent.h"
// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
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

	// 2026.10.29 Lee: モードで経路を分岐する
	// 模擬 = 従来の仮想スティック経路（挙動は一切変更しない）/ 実機 = 復号スナップショット経路
	if (Subsystem->IsSimulatorMode())
	{
		TickSimulator(Subsystem, Pawn, DeltaTime);
	}
	else
	{
		TickDevice(Subsystem, Pawn, DeltaTime);
	}
}

// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
void UBicycleBridgeComponent::TickSimulator(UFishingWiredDeviceSubsystem* Subsystem, AVRPawn* Pawn, float DeltaTime)
{
	// --- 模擬モード: 従来経路をそのまま維持する（RPS → 仮想角度 θ → InjectReelStickInput） ---
	const FBicycleSample Sample = Subsystem->GetLatestBicycleSample();
	const bool bFresh = Sample.bValid && Sample.AgeSeconds <= DataStaleTimeoutSeconds;

	// 有効 RPS。模擬データは元から実回転数で生成されるためパルス換算（BicyclePulsesPerRevolution）
	// は適用されない（旧コードの !IsSimulatorMode 分岐は本関数では恒真のため省略。
	// 旧実機向けのパルス換算は新経路（TickDevice）でも使用しない）
	const float Rps = FMath::Abs(Sample.Rps) * RpsScale;

	// --- Debug 表示（毎フレーム必ず出す。停止中は灰色で状態を明示） ---
	if (bShowDebug && GEngine)
	{
		const FString SourceText = TEXT("SIM");
		if (bFresh)
		{
			const FString DebugMsg = FString::Printf(
				TEXT("[BicycleBridge] RPS: %.2f | RPM: %.0f | θ: %.1f° | Age: %.2fs | Dir:%d | %s"),
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

	if (!bFresh)
	{
		// データ無し/古い → 注入を止める（Reel 状態は入力途絶により自然に遅すぎ判定へ向かう）
		return;
	}

	// 仮想スティック角度を進めて注入（ベクトル角差がそのまま Δangle → RPM 判定へ流れる）
	VirtualAngleRad += Rps * 2.0f * PI * DeltaTime;
	const FVector2D VirtualStick(FMath::Cos(VirtualAngleRad), FMath::Sin(VirtualAngleRad));
	Pawn->InjectReelStickInput(VirtualStick);
}

void UBicycleBridgeComponent::TickDevice(UFishingWiredDeviceSubsystem* Subsystem, AVRPawn* Pawn, float DeltaTime)
{
	(void)DeltaTime; // イベント駆動のためフレーム間隔を直接使わない（実時刻はイベント側で持つ）
	UFishingReelStateComponent* ReelState = Pawn->FindComponentByClass<UFishingReelStateComponent>();
	const bool bReelActive = ReelState && ReelState->IsActive();

	// 2026.10.29 Lee(8): 接続代次・復号快照・完走周期イベント批次を同一ロック取得の
	// アトミックビュー（DrainBicycleState）で受け取る。旧実装は ticker 缓存の快照と
	// ワーカー直 drain が非同期のため「旧代次を読んだ直後の再接続で新代次イベントを
	// 取得して代次不符で全破棄する」競合が残っていた。本ビューでは消費基準（代次）と
	// 対象（イベント）が常に一致する。方向反転設定はゲームスレッド側の設定値を直接参照する
	const FBicycleDrainResult Drain = Subsystem->DrainBicycleState();
	const FBicycleMotionSnapshotCore& Motion = Drain.Snapshot;
	const uint32 CurrentGeneration = Drain.Generation;
	const UFishingWiredDeviceSettings* Settings = GetDefault<UFishingWiredDeviceSettings>();
	const bool bInverted = Settings ? Settings->bInvertBicycleDirection : false;

	// --- Debug 表示（スナップショットは状態/診断表示用。圈の駆動はイベント経路のみ） ---
	if (bShowDebug && GEngine)
	{
		FString DebugMsg;
		if (!Motion.bValid)
		{
			DebugMsg = FString::Printf(TEXT("[BicycleBridge] 復号快照無し (状態フレーム未受信) | COM%d"),
				Subsystem->GetDeviceComPort(EFishingWiredDeviceType::Bicycle));
		}
		else
		{
			const TCHAR* PhaseTag = TEXT("停止");
			switch (Motion.Phase)
			{
			case EBicycleMotionPhase::Measuring: PhaseTag = TEXT("測定中"); break;
			case EBicycleMotionPhase::Forward:   PhaseTag = TEXT("正方向"); break;
			case EBicycleMotionPhase::Reverse:   PhaseTag = TEXT("逆方向"); break;
			default: break;
			}
			const double AgeSec = FPlatformTime::Seconds() - Motion.TimestampSec;
			DebugMsg = FString::Printf(TEXT("[BicycleBridge] 復号RPM: %.1f | %s | 圈 +%lld -%lld | Gen:%u | Age: %.2fs"),
				Motion.Rpm, PhaseTag, Motion.ForwardCycles, Motion.ReverseCycles,
				CurrentGeneration, AgeSec);
		}
		GEngine->AddOnScreenDebugMessage(31, 0.0f, Motion.bValid ? FColor::Orange : FColor::Silver, DebugMsg);
	}

	// --- 履歴の再構築判定（純関数。テスト対象） ---
	// 再構築時の批次の扱いは ShouldConsumeEventsOnRebuild（安全側）に従う:
	// - 純粋な再接続（活性継続・方向不変・代次変化のみ）→ 批次を消費へ（握手直後の首圈を守る）。
	//   ワーカーが代次を先に发布しているため、同ロック取得分の批次は境界後と証明できる
	// - フェーズの立ち上がり / 方向設定変更 / 非アクティブ → 履歴として全破棄
	//  （境界後と証明できない同帧混合は安全側の破棄を優先する＝既知の制限）
	const bool bGenerationChanged = (CurrentGeneration != BaselineGeneration);
	const bool bDirectionChanged = (bInverted != bBaselineDirectionInverted);
	if (UBicycleBridgeComponent::ShouldRebuildBaseline(
			bReelActive, bWasReelActive,
			CurrentGeneration, BaselineGeneration,
			bInverted, bBaselineDirectionInverted))
	{
		const bool bConsumeNewBatch = UBicycleBridgeComponent::ShouldConsumeEventsOnRebuild(
			bReelActive, bWasReelActive, bGenerationChanged, bDirectionChanged);
		BaselineGeneration = CurrentGeneration;
		bBaselineDirectionInverted = bInverted;
		bWasReelActive = bReelActive;
		if (!bConsumeNewBatch)
		{
			return; // drain した批次は履歴として破棄済み（DrainBicycleState でキューは空）
		}
		// 消費ループへフォールスルー（旧代次の混入は下の代次フィルタで除外）
	}
	else
	{
		bWasReelActive = true;
	}

	if (!ReelState || Drain.Events.Num() == 0)
	{
		return;
	}

	// イベントの実時刻（FPlatformTime::Seconds() 軸）をゲーム時刻（GetTimeSeconds 軸）へ写像する。
	// 両者は同一プロセスの単調時計なのでオフセット 1 回の減算で対応する。
	// これにより停止検知の基準が「周期の実際の完了時刻」で更新され、消費遅延や
	// 最新快照の残置値で旧周期を現在時刻へ紐付ける誤りを防ぐ
	const UWorld* World = Pawn->GetWorld();
	const double GameNowSec = World ? World->GetTimeSeconds() : 0.0;
	const double DeviceNowSec = FPlatformTime::Seconds();
	const double GameToDeviceOffset = GameNowSec - DeviceNowSec;

	for (const FBicycleCycleEventCore& Event : Drain.Events)
	{
		// 旧接続のイベントは破棄（再構築で drain 済みのはずだが、間隔での取りこぼしに備える）。
		// 基準は本フレームのアトミック取得分（CurrentGeneration と同一値に更新済み）
		if (Event.Generation != BaselineGeneration)
		{
			continue;
		}

		// ゲーム内前進方向のみ推進する（逆方向は ProcessDeviceCycle 内でも破棄される）
		const bool bForwardCycle = UBicycleBridgeComponent::IsGameForwardEvent(bInverted, Event.Dir);

		// 周期ごとの実効 RPM（旧プロトコルのパルス換算は適用しない）。
		// 測定中の初回周期は RPM 未確定のため 0 を渡す（推進のみで判定・通知を行わない）
		float CycleRPM = 0.0f;
		if (!Event.IsMeasuring() && bForwardCycle)
		{
			CycleRPM = static_cast<float>(Event.GetRpm() * RpsScale);
		}

		const double EventGameTimeSec = Event.DeviceTimeSec + GameToDeviceOffset;

		if (!ReelState->ProcessDeviceCycle(bForwardCycle, CycleRPM, Event.IsMeasuring(), EventGameTimeSec))
		{
			// 完了（成功/失敗）が確定したので残イベントは処理しない
			// （キューは drain 済み。次フェーズの立ち上がりで履歴ごと再構築される）
			break;
		}
	}
}
// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

UFishingWiredDeviceSubsystem* UBicycleBridgeComponent::FindSubsystem() const
{
	if (!GetOwner())
	{
		return nullptr;
	}
	UGameInstance* GameInstance = GetOwner()->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UFishingWiredDeviceSubsystem>() : nullptr;
}
