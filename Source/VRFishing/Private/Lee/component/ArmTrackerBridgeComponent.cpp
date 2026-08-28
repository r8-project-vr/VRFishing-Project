// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/component/ArmTrackerBridgeComponent.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "Lee/device/FishingWiredDeviceSubsystem.h"
#include "VRFishingLog.h"
#include "Engine/Engine.h"

UArmTrackerBridgeComponent::UArmTrackerBridgeComponent()
{
	// センサ（HandHeightDetector）が常時 Tick する常駐サービスであるため、本ブリッジも常駐 Tick で同期する
	PrimaryComponentTick.bCanEverTick = true;
}

void UArmTrackerBridgeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// --- 依存解決（弱参照が切れていたら再取得） ---
	if (!Detector.IsValid() && GetOwner())
	{
		Detector = GetOwner()->FindComponentByClass<UHandHeightDetectorComponent>();
	}
	UHandHeightDetectorComponent* DetectorPtr = Detector.Get();

	UFishingWiredDeviceSubsystem* Subsystem = FindSubsystem();
	if (!Subsystem)
	{
		return;
	}

	// --- 最新サンプル取得と鮮度判定 ---
	const FArmEulerSample Sample = Subsystem->GetLatestArmEuler();
	const bool bFresh = Sample.bValid && Sample.AgeSeconds <= DataStaleTimeoutSeconds;

	if (!bFresh)
	{
		// データが無い/古い → 外部データモードを解除して OpenXR 路線へフォールバック
		if (DetectorPtr && DetectorPtr->IsUsingExternalData())
		{
			DetectorPtr->ClearExternalHandData();
			UE_LOG(LogFishing, Log, TEXT("[ArmBridge] データ無し/古いため外部データモードを解除（OpenXR へフォールバック）"));
		}
		bHasSmoothedPercent = false;
		bHasRawAngle = false;
		return;
	}

	// --- 軸選択 + 反転 ---
	float RawAngleDeg = 0.0f;
	switch (SourceAxis)
	{
	case EArmTrackerAxis::AxisX:	RawAngleDeg = Sample.AxisXDeg;	break;
	case EArmTrackerAxis::AxisY:	RawAngleDeg = Sample.AxisYDeg;	break;
	case EArmTrackerAxis::AxisZ:	RawAngleDeg = Sample.AxisZDeg;	break;
	}
	if (bInvertAxis)
	{
		RawAngleDeg = -RawAngleDeg;
	}
	LastRawAngleDeg = RawAngleDeg;
	bHasRawAngle = true;

	// --- キャリブレーション二点マッピング: [CalLow, CalHigh] → [0,1] ---
	const float CalRange = CalAngleHigh - CalAngleLow;
	float TargetPercent = 0.5f;
	if (!FMath::IsNearlyZero(CalRange))
	{
		TargetPercent = FMath::Clamp((RawAngleDeg - CalAngleLow) / CalRange, 0.0f, 1.0f);
	}

	// --- 端点スナップ（マッピング後） ---
	// Ready の下げ判定は percent == 0.0 のみで成立するため、端点近傍を正確な 0/1 に固定する
	const float Snap = FMath::Clamp(EndpointSnapPercent, 0.0f, 0.1f);
	if (TargetPercent <= Snap)
	{
		TargetPercent = 0.0f;
	}
	else if (TargetPercent >= 1.0f - Snap)
	{
		TargetPercent = 1.0f;
	}

	// --- 平滑化（低域フィルタ）。速度は平滑化前の値との差分から先に算出しておく ---
	const float PrevPercent = bHasSmoothedPercent ? SmoothedPercent : TargetPercent;
	if (!bHasSmoothedPercent)
	{
		SmoothedPercent = TargetPercent;
		bHasSmoothedPercent = true;
	}
	else if (SmoothingAlpha < 1.0f)
	{
		SmoothedPercent = FMath::Lerp(SmoothedPercent, TargetPercent, FMath::Clamp(SmoothingAlpha, 0.0f, 1.0f));
	}
	else
	{
		SmoothedPercent = TargetPercent;
	}

	// --- 端点スナップ（平滑化後） ---
	// 低域フィルタは指数的にしか収束しないため、平滑値が微小正値に留まる端点（特に 0）を
	// 正確な 0/1 に固定する。これで Ready 状態の 2 秒保持判定が確実に成立する。
	if (SmoothedPercent <= Snap)
	{
		SmoothedPercent = 0.0f;
	}
	else if (SmoothedPercent >= 1.0f - Snap)
	{
		SmoothedPercent = 1.0f;
	}

	// --- 速度換算: 正規化空間の差分 → cm/s（仮想 Z 移動量 = percent 差 × (Bottom+Top)） ---
	float BottomOffset = 50.0f;
	float TopOffset = 30.0f;
	if (DetectorPtr)
	{
		BottomOffset = DetectorPtr->BottomOffset;
		TopOffset = DetectorPtr->TopOffset;
	}
	const float SpeedCmPerSec = DeltaTime > KINDA_SMALL_NUMBER
		? (SmoothedPercent - PrevPercent) * (BottomOffset + TopOffset) / DeltaTime
		: 0.0f;

	// --- 注入（外部データモードへ切替。下流の状態機・Widget は無変更で動作する） ---
	if (DetectorPtr)
	{
		DetectorPtr->SetExternalHandData(SmoothedPercent, SpeedCmPerSec);
	}

	if (bShowDebug && GEngine)
	{
		const FString SourceText = Subsystem->IsSimulatorMode()
			? TEXT("SIM")
			: FString::Printf(TEXT("COM%d"), Subsystem->GetConnectedComPort());
		FString DebugMsg = FString::Printf(
			TEXT("[ArmBridge] Raw: %.1f° | Percent: %.2f | Speed: %.1f cm/s | Age: %.2fs | %s"),
			RawAngleDeg, SmoothedPercent, SpeedCmPerSec, Sample.AgeSeconds, *SourceText);
		GEngine->AddOnScreenDebugMessage(30, 0.0f, FColor::Cyan, DebugMsg);
	}
}

UFishingWiredDeviceSubsystem* UArmTrackerBridgeComponent::FindSubsystem() const
{
	if (!GetOwner())
	{
		return nullptr;
	}
	UGameInstance* GameInstance = GetOwner()->GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UFishingWiredDeviceSubsystem>() : nullptr;
}

void UArmTrackerBridgeComponent::CaptureCalibrationLow()
{
	if (bHasRawAngle)
	{
		CalAngleLow = LastRawAngleDeg;
		UE_LOG(LogFishing, Log, TEXT("[ArmBridge] キャリブレーション下限を記録: %.1f°"), CalAngleLow);
	}
	else
	{
		UE_LOG(LogFishing, Warning, TEXT("[ArmBridge] CaptureCalibrationLow: 角度データがまだ無いため記録できません"));
	}
}

void UArmTrackerBridgeComponent::CaptureCalibrationHigh()
{
	if (bHasRawAngle)
	{
		CalAngleHigh = LastRawAngleDeg;
		UE_LOG(LogFishing, Log, TEXT("[ArmBridge] キャリブレーション上限を記録: %.1f°"), CalAngleHigh);
	}
	else
	{
		UE_LOG(LogFishing, Warning, TEXT("[ArmBridge] CaptureCalibrationHigh: 角度データがまだ無いため記録できません"));
	}
}

void UArmTrackerBridgeComponent::SendDeviceCalibrationRequest()
{
	if (UFishingWiredDeviceSubsystem* Subsystem = FindSubsystem())
	{
		Subsystem->RequestDeviceCalibration();
	}
}
