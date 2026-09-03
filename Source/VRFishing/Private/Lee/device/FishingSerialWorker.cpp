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

/** @brief デストラクタ。スレッドの停止と SerialIF の解放は Exit() で済んでいる前提 */
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
				// 2026.09.03 Lee: 接続直後に回転方向(0x23)を 1 回取得させる（以降は低頻度更新）
				NextDirectionPollTimeSec = 0.0;
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
	// 2026.09.03 Lee: 送信前にバッファクリア（WriteCmdFlushed。一問一答の徹底）
	if (WriteCmdFlushed(UFishingWiredDeviceSettings::ArmTrackerCmdGetEuler) != 0)
	{
		ForceReconnect(TEXT("オイラー角コマンド送信失敗"));
		return;
	}

	ASerialDataStruct::ASerialData Buf;
	const int32 ReadResult = Controller->ReadData(&Buf);
	if (ReadResult == -2)
	{
		// タイムアウトはデバイスが応答しなかっただけ。連続したら切断扱いへ。
		// 2026.09.03 Lee: 復旧処理は HandlePollFailure へ統一（バッファクリア込み）
		HandlePollFailure(TEXT("応答タイムアウト連続"));
		return;
	}
	if (ReadResult != 0 || Buf.data_num < 12)
	{
		// 2026.09.03 Lee: 応答長異常 1 回での強制再接続は廃止（前作準拠。連続異常時のみ再接続）
		HandlePollFailure(TEXT("オイラー角応答異常"));
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

	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 回転方向(0x23)は接続直後 1 回 + 30 秒毎の低頻度更新へ変更。
	// 旧来の 1 秒毎の割込み送信（元コード）は、応答遅延時にフレーム食い違いを起こしやすい
	// （「RPS応答異常」→ 強制再接続の原因）。前作『自転車でGo!』は方向を輪番ポーリングに
	// 含めておらず、方向は実質固定値（取付方向）であるため頻繁に取得する必要がない。
	// 元コード（消さずにコメントで残す）:
	//     NextDirectionPollTimeSec = NowSec + 1.0;
	//     if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdGetDirection) == 0)
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	if (NowSec >= NextDirectionPollTimeSec)
	{
		NextDirectionPollTimeSec = NowSec + 30.0;
		if (WriteCmdFlushed(UFishingWiredDeviceSettings::BicycleCmdGetDirection) == 0)
		{
			ASerialDataStruct::ASerialData DirBuf;
			if (Controller->ReadData(&DirBuf) == 0 && DirBuf.data_num >= 1)
			{
				const uint8 NewDirection = DirBuf.data[0] ? 1 : 0;
				// 方向応答の変化をログへ残す（実機の取付方向が規格想定と逆なら初回に 0 が来る。
				// 「踏んだのに反応しない」診断のデータ源。値が確定した後は再出力しない）
				if (NewDirection != LastDirection)
				{
					UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike 回転方向(0x23)応答: %d (%s)"),
						NewDirection, NewDirection ? TEXT("正転") : TEXT("逆転"));
				}
				LastDirection = NewDirection;
			}
		}
	}

	// 2026.09.03 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// RPS(0x22) は更新フラグ(0x20)を介さず直接ポーリングする。
	// 実機の 0x20 は「値が変化した時のみ 1 になる」挙動の疑いがあり、均一速度の踏み込み中は
	// フラグが長時間 0 のまま→データが数秒間隔しか届かない（Age が 1 秒超過を繰り返す現象）。
	// RPS は連続物理量であり数百 ms 前の値を直接読んでも問題ないため、フラグ非依存の
	// 直接読みへ変更（前作『自転車でGo!』も Check を介さない循環ポーリングが実績あり）。
	// 元コード（消さずにコメントで残す）:
	//     // 更新フラグ(0x20)を確認してから RPS(0x22)を取得する（更新なし時に読むと古い値になるため）
	//     if (WriteCmdFlushed(UFishingWiredDeviceSettings::BicycleCmdGetUpdateFlag) != 0)
	//     {
	//         ForceReconnect(TEXT("更新フラグコマンド送信失敗"));
	//         return;
	//     }
	//     ASerialDataStruct::ASerialData FlagBuf;
	//     int32 ReadResult = Controller->ReadData(&FlagBuf);
	//     if (ReadResult == -2)
	//     {
	//         HandlePollFailure(TEXT("応答タイムアウト連続"));
	//         return;
	//     }
	//     if (ReadResult != 0 || FlagBuf.data_num < 1)
	//     {
	//         HandlePollFailure(TEXT("更新フラグ応答異常"));
	//         return;
	//     }
	//     ConsecutiveFailures = 0;
	//     if (FlagBuf.data[0] == 0)
	//     {
	//         // 未更新: 前回値を維持して終了
	//         return;
	//     }
	// 2026.09.03 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// RPS(0x22) 取得（符号付き4Byte, 100倍整数。例: 123 → 1.23）
	// 2026.09.03 Lee: 送信前バッファクリア（WriteCmdFlushed。一問一答の徹底）
	if (WriteCmdFlushed(UFishingWiredDeviceSettings::BicycleCmdGetRps) != 0)
	{
		ForceReconnect(TEXT("RPSコマンド送信失敗"));
		return;
	}

	ASerialDataStruct::ASerialData RpsBuf;
	const int32 ReadResult = Controller->ReadData(&RpsBuf);
	if (ReadResult != 0 || RpsBuf.data_num < 4)
	{
		// タイムアウト/応答長異常とも連続 20 回まではスキップ（前作準拠。1 回で切断しない）
		HandlePollFailure(TEXT("RPS応答異常"));
		return;
	}

	// 2026.09.03 Lee(2): 旧フラグ取得成功時のカウンタ復帰を RPS 取得成功時へ移動
	ConsecutiveFailures = 0;

	const float Rps = ParseI32BE(&RpsBuf.data[0]) / 100.0f;

	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 踩踏検知ログ（RPS が初めて 0 以外になった時のみ 1 回）。
	// 実機の踏み込みがデータとして届いているかの最初の確認点。 出力されない場合は
	// 更新フラグ(0x20)が立っていない＝デバイス側で RPS が更新されていない疑いがある。
	if (!bLoggedFirstNonZeroRps && FMath::Abs(Rps) > KINDA_SMALL_NUMBER)
	{
		bLoggedFirstNonZeroRps = true;
		UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike 踩踏検知: RPS=%.2f (RPM=%.0f) Dir=%d"),
			Rps, Rps * 60.0f, LastDirection);
	}
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// 2026.09.03 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 診断のため RPM(0x21) も併せて取得する。実機で RPS(0x22) の値が踏板に追従しないため、
	// 0x21（符号無し 2Byte, ビッグエンディアン）が追従するかを比較する。
	// ・0x21 が追従する → データ経路を RPM/60 へ切替可能（0x22 は参考値扱いへ）
	// ・両方とも追従しない → デバイス側の測定そのものが未動作（固側への確認事項）
	// 応答異常は致命扱いしない（0x21 未実装の固件でも切断しない。滞留バイトは次回送信前 clear で廃棄）
	if (WriteCmdFlushed(UFishingWiredDeviceSettings::BicycleCmdGetRpm) == 0)
	{
		ASerialDataStruct::ASerialData RpmBuf;
		if (Controller->ReadData(&RpmBuf) == 0 && RpmBuf.data_num >= 2)
		{
			RpmFromDevice = static_cast<float>(static_cast<uint16>((RpmBuf.data[0] << 8) | RpmBuf.data[1]));
		}
		else
		{
			UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike RPM(0x21)応答異常 (num=%d)"), RpmBuf.data_num);
		}
	}
	// 2026.09.03 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーー

	FFishingDeviceSampleCore Sample;
	Sample.bValid = true;
	Sample.TimestampSec = FPlatformTime::Seconds();
	Sample.Rps = Rps;
	// 2026.09.03 Lee(2): RPM は 0x21 応答の生値を格納（旧: Rps×60 の算出値）
	Sample.Rpm = RpmFromDevice;
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

// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
int32 FFishingSerialWorker::WriteCmdFlushed(uint8 Cmd)
{
	if (SerialIF)
	{
		// 送信前に滞留バイト（遅延した前回応答等）を捨てる。
		// これで ReadData の応答は必ず直前の送信へのものになり、フレーム食い違いを防ぐ
		SerialIF->clear();
	}
	return Controller ? Controller->WriteData(Cmd) : -1;
}

void FFishingSerialWorker::HandlePollFailure(const TCHAR* Reason)
{
	if (SerialIF)
	{
		// 応答途中の断片を捨てて次の問い合わせからフレーム境界を合わせ直す
		SerialIF->clear();
	}
	if (++ConsecutiveFailures > 20)
	{
		ForceReconnect(Reason);
	}
	else
	{
		UE_LOG(LogFishing, Log, TEXT("[WiredDevice] ポーリング異常をスキップ: %s (連続%d回目)"), Reason, ConsecutiveFailures);
	}
}
// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

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
