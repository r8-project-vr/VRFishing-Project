// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/device/FishingWiredDeviceSubsystem.h"
#include "Lee/device/FishingSerialWorker.h"
#include "Lee/device/FishingWiredDeviceSettings.h"
#include "VRFishingLog.h"

#if PLATFORM_WINDOWS
#include "ASerialLibControllerWin.h"
#include "WindowsSerial/WindowsSerial.h"
#endif

namespace
{
	/** @brief ASerial デバイスID/Ver を種別から取り出すペア */
	void GetDeviceIdAndVer(EFishingWiredDeviceType Type, uint8& OutId, uint8& OutVer)
	{
		if (Type == EFishingWiredDeviceType::ArmTracker)
		{
			OutId = UFishingWiredDeviceSettings::ArmTrackerDeviceId;
			OutVer = UFishingWiredDeviceSettings::ArmTrackerDeviceVer;
		}
		else
		{
			OutId = UFishingWiredDeviceSettings::BicycleDeviceId;
			OutVer = UFishingWiredDeviceSettings::BicycleDeviceVer;
		}
	}
}

void UFishingWiredDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// ゲームスレッド定期処理を登録（模擬データ生成と状態監視の駆動源）
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFishingWiredDeviceSubsystem::HandleTicker), 0.0f);

	// 設定が有効なら自動接続（Win64 実機 / 模擬モードの両対応）
	const UFishingWiredDeviceSettings* Settings = GetDefault<UFishingWiredDeviceSettings>();
	if (Settings && Settings->bAutoConnectOnStartup)
	{
		StartConnection();
	}
}

void UFishingWiredDeviceSubsystem::Deinitialize()
{
	StopConnection();

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	Super::Deinitialize();
}

void UFishingWiredDeviceSubsystem::StartConnection()
{
	if (bConnectionActive)
	{
		return; // 二重開始防止
	}

	const UFishingWiredDeviceSettings* Settings = GetDefault<UFishingWiredDeviceSettings>();
	if (!Settings)
	{
		return;
	}

	bConnectionActive = true;

	bool bSimulator = Settings->bUseSimulator;
#if !PLATFORM_WINDOWS
	// ASerialCom は Win64 専用のため、他プラットフォームでは自動的に模擬モードへフォールバック
	bSimulator = true;
#endif

	if (bSimulator)
	{
		bSimulatorActive = true;
		SimStartSec = FPlatformTime::Seconds();
		UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 模擬モードで接続開始 (デバイス種別=%d)"),
			static_cast<int32>(Settings->DeviceType));
		BroadcastStateIfChanged();
		return;
	}

#if PLATFORM_WINDOWS
	// --- 実機モード: コントローラ生成 → ワーカースレッド起動 ---
	uint8 DeviceId = 0;
	uint8 DeviceVer = 0;
	GetDeviceIdAndVer(Settings->DeviceType, DeviceId, DeviceVer);

	Controller = NewObject<UASerialLibControllerWin>(this);
	// ID と Ver の照合範囲を仕様書値に固定（単一バージョン運用）
	Controller->Initialize(static_cast<int32>(DeviceId), static_cast<int32>(DeviceVer), static_cast<int32>(DeviceVer));

	Worker = new FFishingSerialWorker(Controller.Get(),
		DeviceId, DeviceVer, DeviceVer,
		Settings->DeviceType,
		Settings->PollIntervalSeconds,
		Settings->ReconnectIntervalSeconds,
		Settings->ComPortOverride);

	WorkerThread = FRunnableThread::Create(Worker, TEXT("FishingSerialWorker"), 0, TPri_Normal);
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 実機接続スレッド起動 (種別=%d, ID=0x%02X)"),
		static_cast<int32>(Settings->DeviceType), DeviceId);
#endif
}

void UFishingWiredDeviceSubsystem::StopConnection()
{
	if (!bConnectionActive)
	{
		return;
	}
	bConnectionActive = false;
	bSimulatorActive = false;

	// スレッド停止（Stop 要求 → 完了待ち → 破棄。Magic_Project UDeviceThreadManager と同手順）
	if (Worker)
	{
		Worker->Stop();
	}
	if (WorkerThread)
	{
		WorkerThread->WaitForCompletion();
		delete WorkerThread;
		WorkerThread = nullptr;
	}
	if (Worker)
	{
		delete Worker;
		Worker = nullptr;
	}
	Controller = nullptr;

	FScopeLock Lock(&SampleMutex);
	LatestSample = FFishingDeviceSampleCore();

	LastBroadcastState = EFishingWiredDeviceState::Idle;
	OnStateChanged.Broadcast(false, 0);
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 接続停止"));
}

bool UFishingWiredDeviceSubsystem::IsConnected() const
{
	return GetDeviceState() == EFishingWiredDeviceState::Connected;
}

bool UFishingWiredDeviceSubsystem::IsSimulatorMode() const
{
	return bSimulatorActive;
}

EFishingWiredDeviceState UFishingWiredDeviceSubsystem::GetDeviceState() const
{
	if (!bConnectionActive)
	{
		return EFishingWiredDeviceState::Idle;
	}
	if (bSimulatorActive)
	{
		return EFishingWiredDeviceState::Connected;
	}
#if PLATFORM_WINDOWS
	if (Worker)
	{
		return Worker->GetState();
	}
#endif
	return EFishingWiredDeviceState::Idle;
}

int32 UFishingWiredDeviceSubsystem::GetConnectedComPort() const
{
#if PLATFORM_WINDOWS
	if (Worker)
	{
		return Worker->GetConnectedComPort();
	}
#endif
	return 0;
}

FArmEulerSample UFishingWiredDeviceSubsystem::GetLatestArmEuler()
{
	const FFishingDeviceSampleCore Core = CopySample();

	FArmEulerSample Sample;
	Sample.bValid = Core.bValid;
	Sample.AgeSeconds = Core.GetAgeSeconds(FPlatformTime::Seconds());
	Sample.AxisXDeg = Core.AxisXDeg;
	Sample.AxisYDeg = Core.AxisYDeg;
	Sample.AxisZDeg = Core.AxisZDeg;
	return Sample;
}

FBicycleSample UFishingWiredDeviceSubsystem::GetLatestBicycleSample()
{
	const FFishingDeviceSampleCore Core = CopySample();

	FBicycleSample Sample;
	Sample.bValid = Core.bValid;
	Sample.AgeSeconds = Core.GetAgeSeconds(FPlatformTime::Seconds());
	Sample.Rps = Core.Rps;
	Sample.Rpm = Core.Rpm;
	Sample.Direction = Core.Direction;
	return Sample;
}

void UFishingWiredDeviceSubsystem::RequestDeviceCalibration()
{
#if PLATFORM_WINDOWS
	if (Worker)
	{
		Worker->RequestCalibration();
		return;
	}
#endif
	UE_LOG(LogFishing, Warning, TEXT("[WiredDevice] RequestDeviceCalibration: 実機未接続のため要求を破棄"));
}

bool UFishingWiredDeviceSubsystem::HandleTicker(float DeltaTime)
{
	const UFishingWiredDeviceSettings* Settings = GetDefault<UFishingWiredDeviceSettings>();

	if (bConnectionActive && bSimulatorActive && Settings)
	{
		// --- 模擬データ生成 ---
		// 手: 「下端保持 → 上昇 → 上端保持 → 下降」の三角波を SimSweepCountPerCycle 回繰り返す。
		//   - 下端保持は Ready 状態の「50cm 下げを 2 秒維持」を成立させる（外部モードでは percent == 0.0 のみ成立）
		//   - 上下速度は HandUpDown の RecommendedSpeed(既定 0.2/s) に一致させ、高スコアの検証を可能にする
		// 自転車: RPS が Base±Amplitude の帯で正弦変動し、Reel 状態の RPM 判定帯（既定 30〜70）を通過する
		const double NowSec = FPlatformTime::Seconds();
		const double Elapsed = NowSec - SimStartSec;

		const float HoldSec = FMath::Max(Settings->SimBottomHoldSeconds, 0.5f);
		const float LegSec = 1.0f / FMath::Max(Settings->SimHandSpeedPercentPerSec, 0.05f);
		const float TopDwellSec = FMath::Max(Settings->SimEndpointDwellSeconds, 0.0f);
		const float SweepSec = HoldSec + LegSec + TopDwellSec + LegSec;
		const int32 SweepCount = FMath::Max(Settings->SimSweepCountPerCycle, 1);
		const double CycleSec = static_cast<double>(SweepSec) * SweepCount;

		// 1 サイクル内の位相（サイクルを超えたら最初の下端保持へ戻る）
		const double LocalT = FMath::Fmod(Elapsed, CycleSec);
		const float K = static_cast<float>(FMath::Fmod(LocalT, static_cast<double>(SweepSec)));

		float Percent = 0.0f;
		if (K < HoldSec)
		{
			Percent = 0.0f;                                                    // 下端保持
		}
		else if (K < HoldSec + LegSec)
		{
			Percent = (K - HoldSec) / LegSec;                                  // 上昇
		}
		else if (K < HoldSec + LegSec + TopDwellSec)
		{
			Percent = 1.0f;                                                    // 上端保持
		}
		else
		{
			Percent = 1.0f - (K - HoldSec - LegSec - TopDwellSec) / LegSec;    // 下降
		}

		// 自転車 RPS 模擬: 周期 SimRpsWavePeriodSeconds の正弦で Base±Amplitude を往復
		const float RpsWave = FMath::Sin(2.0f * PI * static_cast<float>(Elapsed / FMath::Max(Settings->SimRpsWavePeriodSeconds, 1.0f)));

		FFishingDeviceSampleCore Sample;
		Sample.bValid = true;
		Sample.TimestampSec = NowSec;
		// 軸角度は ArmTrackerBridge の既定キャリブレーション(-80°〜40°)と対応付ける
		Sample.AxisYDeg = FMath::Lerp(-80.0f, 40.0f, FMath::Clamp(Percent, 0.0f, 1.0f));
		Sample.AxisXDeg = 0.0f;
		Sample.AxisZDeg = 0.0f;
		Sample.Rps = FMath::Max(0.0f, Settings->SimRpsBase + Settings->SimRpsAmplitude * RpsWave);
		Sample.Rpm = Sample.Rps * 60.0f;
		Sample.Direction = 1;

		StoreSample(Sample);
	}
#if PLATFORM_WINDOWS
	else if (bConnectionActive && Worker)
	{
		// --- 実機: ワーカーの最新サンプルをゲームスレッド側へ複写 ---
		StoreSample(Worker->GetLatestSample());
	}
#endif

	BroadcastStateIfChanged();
	return true; // true を返すとティック継続
}

void UFishingWiredDeviceSubsystem::StoreSample(const FFishingDeviceSampleCore& Sample)
{
	FScopeLock Lock(&SampleMutex);
	LatestSample = Sample;
}

FFishingDeviceSampleCore UFishingWiredDeviceSubsystem::CopySample() const
{
	FScopeLock Lock(&SampleMutex);
	return LatestSample;
}

void UFishingWiredDeviceSubsystem::BroadcastStateIfChanged()
{
	const EFishingWiredDeviceState Current = GetDeviceState();
	if (Current != LastBroadcastState)
	{
		LastBroadcastState = Current;
		OnStateChanged.Broadcast(Current == EFishingWiredDeviceState::Connected, GetConnectedComPort());
	}
}
