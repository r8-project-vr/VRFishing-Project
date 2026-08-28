// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/device/FishingSerialWorker.h"
#include "Lee/device/FishingWiredDeviceSettings.h"
#include "VRFishingLog.h"

#if PLATFORM_WINDOWS
#include "ASerialCore/ASerialPacket.h"
#include "ASerialLibControllerWin.h"
#include "WindowsSerial/WindowsSerial.h"
#endif

#include <atomic>

// ============================================================
// 公開 API
// ============================================================

FFishingSerialWorker::FFishingSerialWorker(UASerialLibControllerWin* InController,
	uint8 InTargetDeviceId, uint8 InDeviceVerMin, uint8 InDeviceVerMax,
	EFishingWiredDeviceType InDeviceType,
	float InPollIntervalSec, float InReconnectIntervalSec, int32 InComPortOverride)
	: Controller(InController)
	, TargetDeviceId(InTargetDeviceId)
	, DeviceVerMin(InDeviceVerMin)
	, DeviceVerMax(InDeviceVerMax)
	, DeviceType(InDeviceType)
	, PollIntervalSec(InPollIntervalSec)
	, ReconnectIntervalSec(InReconnectIntervalSec)
	, ComPortOverride(InComPortOverride)
{
}

FFishingSerialWorker::~FFishingSerialWorker()
{
}

bool FFishingSerialWorker::Init()
{
	SetState(EFishingWiredDeviceState::Connecting);

#if PLATFORM_WINDOWS
	if (!Controller)
	{
		UE_LOG(LogFishing, Error, TEXT("[WiredDevice] Controller が null のため接続できません"));
		return false;
	}
	// Win32 シリアル IF を生成してコントローラへ設定（ボーレートは ASerial 規格の 115200）
	SerialIF = new WindowsSerial();
	Controller->SetInterfacePt(SerialIF);
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] ワーカー初期化完了 TargetID=0x%02X Ver=%d〜%d COM=%s"),
		TargetDeviceId, DeviceVerMin, DeviceVerMax,
		ComPortOverride > 0 ? *FString::Printf(TEXT("%d"), ComPortOverride) : TEXT("自動探査"));
#endif

	return true;
}

uint32 FFishingSerialWorker::Run()
{
#if !PLATFORM_WINDOWS
	// ASerialCom は Win64 専用。他プラットフォームでは何もせず停止要求を待つ。
	while (StopTaskCounter.GetValue() == 0)
	{
		FPlatformProcess::Sleep(0.1f);
	}
	return 0;
#else
	while (StopTaskCounter.GetValue() == 0)
	{
		// --- ゲームスレッドからの要求（キャリブレーション等）を処理 ---
		int32 QueuedCmd = 0;
		while (CommandQueue.Dequeue(QueuedCmd))
		{
			if (Controller->GetConnectionState())
			{
				Controller->WriteData(static_cast<uint8>(QueuedCmd));
				UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 要求コマンド送信: 0x%02X"), QueuedCmd);
			}
		}

		// --- 未接続なら（再）接続を試みる ---
		if (!Controller->GetConnectionState())
		{
			SetState(EFishingWiredDeviceState::Connecting);
			ConnectResult Result = Fail;
			if (ComPortOverride > 0)
			{
				Result = Controller->ConnectDevice(ComPortOverride);
			}
			else
			{
				// ASerial 規格 6-5 の自動探査: COM1〜255 を順に試してデバイスID照合
				Result = Controller->AutoConnectDevice();
			}

			if (Result == Succ)
			{
				ConnectedCom = SerialIF->GetConnectCOM();
				ConsecutiveFailures = 0;
				NextPollTimeSec = 0.0;
				SetState(EFishingWiredDeviceState::Connected);
				UE_LOG(LogFishing, Log, TEXT("[WiredDevice] 接続確立: COM%d (ID=0x%02X)"), ConnectedCom, TargetDeviceId);
			}
			else
			{
				SetState(EFishingWiredDeviceState::Disconnected);
				FPlatformProcess::Sleep(ReconnectIntervalSec);
				continue;
			}
		}

		// --- ポーリング周期の判定 ---
		const double NowSec = FPlatformTime::Seconds();
		if (NowSec < NextPollTimeSec)
		{
			FPlatformProcess::Sleep(0.001f);
			continue;
		}
		NextPollTimeSec = NowSec + PollIntervalSec;

		// --- 1 回分のポーリング（コマンド送信→応答受信→サンプル更新） ---
		PollDevice();
	}

	return 0;
#endif
}

void FFishingSerialWorker::Stop()
{
	// これで Run のループを抜けさせる（Magic_Project FMagicDeviceCmdSender と同方式）
	StopTaskCounter.Increment();
}

void FFishingSerialWorker::Exit()
{
#if PLATFORM_WINDOWS
	if (Controller && Controller->GetConnectionState())
	{
		Controller->DisConnectDevice();
	}
	if (SerialIF)
	{
		delete SerialIF;
		SerialIF = nullptr;
	}
#endif
	SetState(EFishingWiredDeviceState::Idle);
}

void FFishingSerialWorker::RequestCalibration()
{
	CommandQueue.Enqueue(static_cast<int32>(UFishingWiredDeviceSettings::ArmTrackerCmdCalibration));
}

FFishingDeviceSampleCore FFishingSerialWorker::GetLatestSample() const
{
	FScopeLock Lock(&DataMutex);
	return LatestSample;
}

EFishingWiredDeviceState FFishingSerialWorker::GetState() const
{
	FScopeLock Lock(&StateMutex);
	return State;
}

int32 FFishingSerialWorker::GetConnectedComPort() const
{
	FScopeLock Lock(&StateMutex);
	return ConnectedCom;
}

uint16 FFishingSerialWorker::GetLastErrorCode() const
{
#if PLATFORM_WINDOWS
	if (Controller)
	{
		return Controller->GetLastErrorCode();
	}
#endif
	return 0;
}

uint32 FFishingSerialWorker::GetReceiveCount() const
{
	return ReceiveCount.load(std::memory_order_relaxed);
}

// ============================================================
// 内部処理
// ============================================================

#if PLATFORM_WINDOWS

namespace
{
	/**
	 * @brief ビッグエンディアンの 4Byte を符号付き int32 へ変換する。
	 * @param p 先頭バイトへのポインタ
	 * @return 変換値（ASerial デバイスの 1000 倍スケーリング値はこの後に 1000 で除す）
	 * @note Magic_Project FMagicDeviceCmdSender::TransformDataToInt32 と同じビッグエンディアン解釈。
	 */
	int32 ParseI32BE(const uint8* p)
	{
		return static_cast<int32>(
			(static_cast<uint32>(p[0]) << 24) |
			(static_cast<uint32>(p[1]) << 16) |
			(static_cast<uint32>(p[2]) << 8) |
			static_cast<uint32>(p[3]));
	}
}

void FFishingSerialWorker::PollDevice()
{
	if (DeviceType == EFishingWiredDeviceType::ArmTracker)
	{
		PollArmTracker();
	}
	else
	{
		PollBicycle();
	}
}

void FFishingSerialWorker::PollArmTracker()
{
	// オイラー角取得コマンド(0x21)を送り、応答（符号付き4Byte×3軸 X Y Z, 1000倍）を受信する
	if (Controller->WriteData(UFishingWiredDeviceSettings::ArmTrackerCmdGetEuler) != 0)
	{
		ForceReconnect(TEXT("オイラー角コマンド送信失敗"));
		return;
	}

	ASerialDataStruct::ASerialData Buf;
	const int32 ReadResult = Controller->ReadData(&Buf);
	if (ReadResult == -2)
	{
		// タイムアウトはデバイスが応答しなかっただけ。連続したら切断扱いへ。
		if (++ConsecutiveFailures > 20)
		{
			ForceReconnect(TEXT("応答タイムアウト連続"));
		}
		return;
	}
	if (ReadResult != 0 || Buf.data_num < 12)
	{
		ForceReconnect(TEXT("オイラー角応答異常"));
		return;
	}

	ConsecutiveFailures = 0;

	FFishingDeviceSampleCore Sample;
	Sample.bValid = true;
	Sample.TimestampSec = FPlatformTime::Seconds();
	// 1000 倍スケーリング値 → 度へ。軸対応は Magic_Project 実装に準拠（X=Roll/Y=Pitch/Z=Yaw）
	Sample.AxisXDeg = ParseI32BE(&Buf.data[0]) / 1000.0f;
	Sample.AxisYDeg = ParseI32BE(&Buf.data[4]) / 1000.0f;
	Sample.AxisZDeg = ParseI32BE(&Buf.data[8]) / 1000.0f;
	StoreSample(Sample);
	ReceiveCount.fetch_add(1, std::memory_order_relaxed);
}

void FFishingSerialWorker::PollBicycle()
{
	const double NowSec = FPlatformTime::Seconds();

	// 1 秒毎に回転方向(0x23)を更新（取付方向に依存するため頻繁に変えない）
	if (NowSec >= NextDirectionPollTimeSec)
	{
		NextDirectionPollTimeSec = NowSec + 1.0;
		if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdGetDirection) == 0)
		{
			ASerialDataStruct::ASerialData DirBuf;
			if (Controller->ReadData(&DirBuf) == 0 && DirBuf.data_num >= 1)
			{
				LastDirection = DirBuf.data[0] ? 1 : 0;
			}
		}
	}

	// 更新フラグ(0x20)を確認してから RPS(0x22)を取得する（更新なし時に読むと古い値になるため）
	if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdGetUpdateFlag) != 0)
	{
		ForceReconnect(TEXT("更新フラグコマンド送信失敗"));
		return;
	}

	ASerialDataStruct::ASerialData FlagBuf;
	int32 ReadResult = Controller->ReadData(&FlagBuf);
	if (ReadResult == -2)
	{
		if (++ConsecutiveFailures > 20)
		{
			ForceReconnect(TEXT("応答タイムアウト連続"));
		}
		return;
	}
	if (ReadResult != 0 || FlagBuf.data_num < 1)
	{
		ForceReconnect(TEXT("更新フラグ応答異常"));
		return;
	}

	ConsecutiveFailures = 0;
	if (FlagBuf.data[0] == 0)
	{
		// 未更新: 前回値を維持して終了
		return;
	}

	// RPS(0x22) 取得（符号付き4Byte, 100倍整数。例: 123 → 1.23）
	if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdGetRps) != 0)
	{
		ForceReconnect(TEXT("RPSコマンド送信失敗"));
		return;
	}

	ASerialDataStruct::ASerialData RpsBuf;
	ReadResult = Controller->ReadData(&RpsBuf);
	if (ReadResult != 0 || RpsBuf.data_num < 4)
	{
		ForceReconnect(TEXT("RPS応答異常"));
		return;
	}

	const float Rps = ParseI32BE(&RpsBuf.data[0]) / 100.0f;

	FFishingDeviceSampleCore Sample;
	Sample.bValid = true;
	Sample.TimestampSec = FPlatformTime::Seconds();
	Sample.Rps = Rps;
	Sample.Rpm = Rps * 60.0f;
	Sample.Direction = LastDirection;
	StoreSample(Sample);
	ReceiveCount.fetch_add(1, std::memory_order_relaxed);
}

void FFishingSerialWorker::ForceReconnect(const TCHAR* Reason)
{
	UE_LOG(LogFishing, Warning, TEXT("[WiredDevice] 再接続へ: %s (最終エラーコード=0x%04X)"), Reason, GetLastErrorCode());
	if (Controller->GetConnectionState())
	{
		Controller->DisConnectDevice();
	}
	ConnectedCom = 0;
	SetState(EFishingWiredDeviceState::Disconnected);
	// 次のループで再接続試行の前に待機させる
	FPlatformProcess::Sleep(ReconnectIntervalSec);
}

#endif // PLATFORM_WINDOWS

void FFishingSerialWorker::SetState(EFishingWiredDeviceState NewState)
{
	FScopeLock Lock(&StateMutex);
	State = NewState;
}

void FFishingSerialWorker::StoreSample(const FFishingDeviceSampleCore& Sample)
{
	FScopeLock Lock(&DataMutex);
	LatestSample = Sample;
}
