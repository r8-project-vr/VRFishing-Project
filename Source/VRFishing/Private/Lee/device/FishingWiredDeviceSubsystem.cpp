// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/device/FishingWiredDeviceSubsystem.h"
#include "Lee/device/FishingSerialWorker.h"
#include "Lee/device/FishingWiredDeviceSettings.h"
#include "VRFishingLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/RunnableThread.h"

#if PLATFORM_WINDOWS
#include "ASerialLibControllerWin.h"
#include "WindowsSerial/WindowsSerial.h"
#endif

namespace
{
	/** @brief ASerial デバイスID/Ver を種別から取り出すペア（Project Settings「ASerialデバイス識別」を参照） */
	void GetDeviceIdAndVer(EFishingWiredDeviceType Type, uint8& OutId, uint8& OutVer)
	{
		const UFishingWiredDeviceSettings* Settings = GetDefault<UFishingWiredDeviceSettings>();
		if (Type == EFishingWiredDeviceType::ArmTracker)
		{
			OutId = static_cast<uint8>(Settings->ArmTrackerDeviceId);
			OutVer = static_cast<uint8>(Settings->ArmTrackerDeviceVer);
		}
		else
		{
			OutId = static_cast<uint8>(Settings->BicycleDeviceId);
			OutVer = static_cast<uint8>(Settings->BicycleDeviceVer);
		}
	}

	/** @brief 種別の表示タグ（ログ/画面デバッグ共用） */
	const TCHAR* GetDeviceTypeTag(EFishingWiredDeviceType Type)
	{
		return Type == EFishingWiredDeviceType::ArmTracker ? TEXT("Arm(0x02)") : TEXT("Bike(0x03)");
	}

	/** @brief 接続状態の表示タグ（ログ用） */
	const TCHAR* GetDeviceStateTag(EFishingWiredDeviceState State)
	{
		switch (State)
		{
		case EFishingWiredDeviceState::Connecting:  return TEXT("接続中(探査含む)");
		case EFishingWiredDeviceState::Connected:   return TEXT("接続済み");
		case EFishingWiredDeviceState::Disconnected: return TEXT("切断");
		default:                                    return TEXT("未起動");
		}
	}

	/** @brief 接続状態の画面デバッグ用色（模擬モード時は別扱い） */
	FColor GetDeviceStateDebugColor(EFishingWiredDeviceState State)
	{
		switch (State)
		{
		case EFishingWiredDeviceState::Connected:   return FColor::Green;
		case EFishingWiredDeviceState::Connecting:  return FColor::Yellow;
		case EFishingWiredDeviceState::Disconnected: return FColor::Red;
		default:                                    return FColor(128, 128, 128);
		}
	}

	/** @brief 画面デバッグ表示の ON/OFF（PIE 中に editor コンソール / ~ で切替可） */
	TAutoConsoleVariable<int32> CVarFishingWiredDeviceDebugStatus(
		TEXT("fishing.WiredDevice.DebugStatus"), 1,
		TEXT("有線デバイス(ASerial)の接続状態を画面に常時表示する (1=表示 / 0=非表示)"));

	/** @brief コンソールコマンド Fishing.DeviceStatus: 現在の World の GameInstance 分の状態をログ出力する */
	FAutoConsoleCommand GFishingDeviceStatusCommand(
		TEXT("Fishing.DeviceStatus"),
		TEXT("有線デバイス(ASerial)の接続状態をログへ出力する"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			bool bFoundGameInstance = false;
			if (GWorld)
			{
				// PIE 中は GWorld がカレントのプレイ World を指す。LogStatusReport() は public なので直接呼べる
				if (UGameInstance* GameInstance = GWorld->GetGameInstance())
				{
					if (UFishingWiredDeviceSubsystem* Subsystem = GameInstance->GetSubsystem<UFishingWiredDeviceSubsystem>())
					{
						bFoundGameInstance = true;
						Subsystem->LogStatusReport();
					}
				}
			}
			if (!bFoundGameInstance)
			{
				UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 未実行: 実行中の GameInstance が無い（PIE 開始後に再実行してください）"));
			}
		}));
}

void UFishingWiredDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// ゲームスレッド定期処理を登録（模擬データ生成・状態監視・画面デバッグの駆動源）
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
		UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 模擬モードで接続開始（Arm + Bike 両方の模擬サンプルを生成）"));
		BroadcastStateIfChanged();
		return;
	}

#if PLATFORM_WINDOWS
	// --- 実機モード: 有効な種別ごとにワーカースレッドを起動（2 台同時接続） ---
	if (Settings->bConnectArmTracker)
	{
		StartDeviceWorker(EFishingWiredDeviceType::ArmTracker, Settings->ArmTrackerComPortOverride, DeviceSlots[0]);
	}
	if (Settings->bConnectBicycle)
	{
		StartDeviceWorker(EFishingWiredDeviceType::Bicycle, Settings->BicycleComPortOverride, DeviceSlots[1]);
	}
#endif

	BroadcastStateIfChanged();
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
	for (FDeviceWorkerSlot& Slot : DeviceSlots)
	{
		StopDeviceWorker(Slot);
	}

	{
		FScopeLock Lock(&ArmSampleMutex);
		LatestArmSample = FFishingDeviceSampleCore();
	}
	{
		FScopeLock Lock(&BicycleSampleMutex);
		LatestBicycleSample = FFishingDeviceSampleCore();
	}

	LastBroadcastStates[0] = EFishingWiredDeviceState::Idle;
	LastBroadcastStates[1] = EFishingWiredDeviceState::Idle;
	OnStateChanged.Broadcast(EFishingWiredDeviceType::ArmTracker, false, 0);
	OnStateChanged.Broadcast(EFishingWiredDeviceType::Bicycle, false, 0);
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 接続停止"));
}

bool UFishingWiredDeviceSubsystem::IsConnected() const
{
	return IsDeviceConnected(EFishingWiredDeviceType::ArmTracker)
		|| IsDeviceConnected(EFishingWiredDeviceType::Bicycle);
}

bool UFishingWiredDeviceSubsystem::IsSimulatorMode() const
{
	return bSimulatorActive;
}

EFishingWiredDeviceState UFishingWiredDeviceSubsystem::GetDeviceState(EFishingWiredDeviceType DeviceType) const
{
	if (!bConnectionActive)
	{
		return EFishingWiredDeviceState::Idle;
	}
	if (bSimulatorActive)
	{
		// 模擬モードは両デバイス分のサンプルを常に生成する
		return EFishingWiredDeviceState::Connected;
	}
	const FDeviceWorkerSlot* Slot = FindSlot(DeviceType);
	if (Slot && Slot->Worker)
	{
		return Slot->Worker->GetState();
	}
	return EFishingWiredDeviceState::Idle;
}

bool UFishingWiredDeviceSubsystem::IsDeviceConnected(EFishingWiredDeviceType DeviceType) const
{
	return GetDeviceState(DeviceType) == EFishingWiredDeviceState::Connected;
}

int32 UFishingWiredDeviceSubsystem::GetDeviceComPort(EFishingWiredDeviceType DeviceType) const
{
	const FDeviceWorkerSlot* Slot = FindSlot(DeviceType);
#if PLATFORM_WINDOWS
	if (Slot && Slot->Worker)
	{
		return Slot->Worker->GetConnectedComPort();
	}
#endif
	return 0;
}

FArmEulerSample UFishingWiredDeviceSubsystem::GetLatestArmEuler()
{
	const FFishingDeviceSampleCore Core = CopyArmSample();

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
	const FFishingDeviceSampleCore Core = CopyBicycleSample();

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
	const FDeviceWorkerSlot* Slot = FindSlot(EFishingWiredDeviceType::ArmTracker);
	if (Slot && Slot->Worker)
	{
		Slot->Worker->RequestCalibration();
		return;
	}
#endif
	UE_LOG(LogFishing, Warning, TEXT("[WiredDevice] RequestDeviceCalibration: 実機未接続のため要求を破棄"));
}

void UFishingWiredDeviceSubsystem::LogStatusReport() const
{
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] ---- 状態レポート ----"));
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 接続処理: %s / モード: %s"),
		bConnectionActive ? TEXT("開始済み") : TEXT("停止中"),
		bSimulatorActive ? TEXT("模擬") : TEXT("実機"));
	UE_LOG(LogFishing, Log, TEXT("%s"), *BuildDeviceStatusLine(EFishingWiredDeviceType::ArmTracker));
	UE_LOG(LogFishing, Log, TEXT("%s"), *BuildDeviceStatusLine(EFishingWiredDeviceType::Bicycle));
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] ------------------"));
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

		StoreArmSample(Sample);
		StoreBicycleSample(Sample);
	}
	else if (bConnectionActive)
	{
		// --- 実機: 各ワーカーの最新サンプルをゲームスレッド側へ複写 ---
		const FDeviceWorkerSlot* ArmSlot = FindSlot(EFishingWiredDeviceType::ArmTracker);
		if (ArmSlot && ArmSlot->Worker)
		{
			StoreArmSample(ArmSlot->Worker->GetLatestSample());
		}
		const FDeviceWorkerSlot* BikeSlot = FindSlot(EFishingWiredDeviceType::Bicycle);
		if (BikeSlot && BikeSlot->Worker)
		{
			StoreBicycleSample(BikeSlot->Worker->GetLatestSample());
		}
	}

	DrawDebugStatus();
	BroadcastStateIfChanged();
	return true; // true を返すとティック継続
}

void UFishingWiredDeviceSubsystem::StartDeviceWorker(EFishingWiredDeviceType DeviceType, int32 ComPortOverride, FDeviceWorkerSlot& Slot)
{
#if PLATFORM_WINDOWS
	uint8 DeviceId = 0;
	uint8 DeviceVer = 0;
	GetDeviceIdAndVer(DeviceType, DeviceId, DeviceVer);

	Slot.Controller = NewObject<UASerialLibControllerWin>(this);
	// ID と Ver の照合範囲を仕様書値に固定（単一バージョン運用）
	Slot.Controller->Initialize(static_cast<int32>(DeviceId), static_cast<int32>(DeviceVer), static_cast<int32>(DeviceVer));

	const UFishingWiredDeviceSettings* Settings = GetDefault<UFishingWiredDeviceSettings>();
	Slot.Worker = new FFishingSerialWorker(Slot.Controller.Get(),
		DeviceId, DeviceVer, DeviceVer,
		DeviceType,
		Settings->PollIntervalSeconds,
		Settings->ReconnectIntervalSeconds,
		ComPortOverride);

	// スレッド名は種別ごとに一意にする（プロファイラ/ログ識別用）
	const TCHAR* ThreadName = DeviceType == EFishingWiredDeviceType::ArmTracker
		? TEXT("FishingSerialWorker_Arm")
		: TEXT("FishingSerialWorker_Bike");
	Slot.Thread = FRunnableThread::Create(Slot.Worker, ThreadName, 0, TPri_Normal);

	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 実機接続スレッド起動: %s (COM=%s, ID=0x%02X)"),
		GetDeviceTypeTag(DeviceType),
		ComPortOverride > 0 ? *FString::Printf(TEXT("%d"), ComPortOverride) : TEXT("自動探査"),
		DeviceId);
#else
	// ASerialCom は Win64 専用のため何もしない（StartConnection 側で模擬モードへフォールバック済み）
	(void)DeviceType;
	(void)ComPortOverride;
	(void)Slot;
#endif
}

void UFishingWiredDeviceSubsystem::StopDeviceWorker(FDeviceWorkerSlot& Slot)
{
	if (Slot.Worker)
	{
		Slot.Worker->Stop();
	}
	if (Slot.Thread)
	{
		Slot.Thread->WaitForCompletion();
		delete Slot.Thread;
		Slot.Thread = nullptr;
	}
	if (Slot.Worker)
	{
		delete Slot.Worker;
		Slot.Worker = nullptr;
	}
	Slot.Controller = nullptr;
}

FDeviceWorkerSlot* UFishingWiredDeviceSubsystem::FindSlot(EFishingWiredDeviceType DeviceType)
{
	return &DeviceSlots[GetTypeIndex(DeviceType)];
}

const FDeviceWorkerSlot* UFishingWiredDeviceSubsystem::FindSlot(EFishingWiredDeviceType DeviceType) const
{
	return &DeviceSlots[GetTypeIndex(DeviceType)];
}

void UFishingWiredDeviceSubsystem::StoreArmSample(const FFishingDeviceSampleCore& Sample)
{
	FScopeLock Lock(&ArmSampleMutex);
	LatestArmSample = Sample;
}

void UFishingWiredDeviceSubsystem::StoreBicycleSample(const FFishingDeviceSampleCore& Sample)
{
	FScopeLock Lock(&BicycleSampleMutex);
	LatestBicycleSample = Sample;
}

FFishingDeviceSampleCore UFishingWiredDeviceSubsystem::CopyArmSample() const
{
	FScopeLock Lock(&ArmSampleMutex);
	return LatestArmSample;
}

FFishingDeviceSampleCore UFishingWiredDeviceSubsystem::CopyBicycleSample() const
{
	FScopeLock Lock(&BicycleSampleMutex);
	return LatestBicycleSample;
}

FString UFishingWiredDeviceSubsystem::BuildDeviceStatusLine(EFishingWiredDeviceType DeviceType) const
{
	// --- 接続状態部分（模擬モードは模擬接続として表示） ---
	FString StateText;
	if (bSimulatorActive)
	{
		StateText = TEXT("模擬接続");
	}
	else
	{
		switch (GetDeviceState(DeviceType))
		{
		case EFishingWiredDeviceState::Connected:
			StateText = FString::Printf(TEXT("接続済み COM%d"), GetDeviceComPort(DeviceType));
			break;
		case EFishingWiredDeviceState::Connecting:
			StateText = TEXT("探査中(自動)…");
			break;
		case EFishingWiredDeviceState::Disconnected:
			StateText = TEXT("切断(再試行中)");
			break;
		default:
			StateText = TEXT("未起動(設定で無効)");
			break;
		}
	}

	// --- 受信実績部分（実機のみ。RX カウントが増え続ければ通信は健全） ---
	FString ExtraText;
	if (!bSimulatorActive)
	{
		const FDeviceWorkerSlot* Slot = FindSlot(DeviceType);
		if (Slot && Slot->Worker)
		{
			ExtraText = FString::Printf(TEXT(" | RX=%u"), Slot->Worker->GetReceiveCount());
			const uint16 LastError = Slot->Worker->GetLastErrorCode();
			if (LastError != 0)
			{
				ExtraText += FString::Printf(TEXT(" Err=0x%04X"), LastError);
			}
		}
	}

	// --- 最新データ部分（種別ごとに表示項目が異なる） ---
	const FFishingDeviceSampleCore Sample = DeviceType == EFishingWiredDeviceType::ArmTracker
		? CopyArmSample()
		: CopyBicycleSample();
	FString ValueText;
	if (DeviceType == EFishingWiredDeviceType::ArmTracker)
	{
		ValueText = FString::Printf(TEXT(" | Euler=(%.1f, %.1f, %.1f)deg Age=%.2fs"),
			Sample.AxisXDeg, Sample.AxisYDeg, Sample.AxisZDeg,
			Sample.GetAgeSeconds(FPlatformTime::Seconds()));
	}
	else
	{
		ValueText = FString::Printf(TEXT(" | RPS=%.2f RPM=%.0f Dir=%d Age=%.2fs"),
			Sample.Rps, Sample.Rpm, static_cast<int32>(Sample.Direction),
			Sample.GetAgeSeconds(FPlatformTime::Seconds()));
	}

	return FString::Printf(TEXT("[WiredDevice] %s: %s%s%s"),
		GetDeviceTypeTag(DeviceType), *StateText, *ExtraText, *ValueText);
}

void UFishingWiredDeviceSubsystem::DrawDebugStatus()
{
	if (!GEngine)
	{
		return;
	}
	if (CVarFishingWiredDeviceDebugStatus.GetValueOnGameThread() <= 0)
	{
		return;
	}

	// 種別ごとに 1 行ずつ固定キーで上書き描画（キー 40=Arm / 41=Bike。ブリッジのデバッグ行 30/31 とは別枠）
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const EFishingWiredDeviceType Type = Index == 0
			? EFishingWiredDeviceType::ArmTracker
			: EFishingWiredDeviceType::Bicycle;
		const FColor Color = bSimulatorActive
			? FColor::Cyan
			: GetDeviceStateDebugColor(GetDeviceState(Type));
		GEngine->AddOnScreenDebugMessage(40 + Index, 0.0f, Color, BuildDeviceStatusLine(Type));
	}
}

void UFishingWiredDeviceSubsystem::BroadcastStateIfChanged()
{
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const EFishingWiredDeviceType Type = Index == 0
			? EFishingWiredDeviceType::ArmTracker
			: EFishingWiredDeviceType::Bicycle;

		const EFishingWiredDeviceState Current = GetDeviceState(Type);
		if (Current == LastBroadcastStates[Index])
		{
			continue;
		}

		UE_LOG(LogFishing, Log, TEXT("[WiredDevice] %s 状態変化: %s → %s (COM%d)"),
			GetDeviceTypeTag(Type),
			GetDeviceStateTag(LastBroadcastStates[Index]),
			GetDeviceStateTag(Current),
			GetDeviceComPort(Type));

		LastBroadcastStates[Index] = Current;
		OnStateChanged.Broadcast(Type, Current == EFishingWiredDeviceState::Connected, GetDeviceComPort(Type));
	}
}
