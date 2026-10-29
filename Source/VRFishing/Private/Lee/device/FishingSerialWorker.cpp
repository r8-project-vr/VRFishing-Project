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
			// 2026.10.29 Lee(9): 前回の停止要求フラグを解除してから接続試行する。
			// （Stop() が途中で要求しても、探査ループと応答待ちが即時 Fail へ抜ける）
			Controller->ClearAbort();
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
				// 2026.10.28 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
				// 自転車: プラグインの ConnectDevice は応答の ID/Ver のみ照合し「長さ 4」を
				// 検証しないため、この時点では Connected にしない。0x01（情報リクエスト）を送り、
				// PollBicycleHandshake が実受信フレームの「長さ 4 + ID/Ver 一致 + 校験合法」を
				// 確認した時点で接続代次を +1・ストリーム系を初期化して 0x22 を送り、
				// Connected へ昇格する。期限超過時は ForceReconnect がポートを解放する
				if (DeviceType == EFishingWiredDeviceType::Bicycle)
				{
					bAwaitingBicycleHandshake = true;
					BicycleHandshakeDeadlineSec = FPlatformTime::Seconds() + HandshakeConfirmTimeoutSec;
					if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdHeartbeat) != 0)
					{
						ForceReconnect(TEXT("握手確認用(0x01)送信失敗"));
						continue;
					}
					UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike 情報応答の検証を開始(0x01 送信済み): COM%d"),
						ConnectedCom);
				}
				// 2026.10.28 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
				// 自転車は握手検証後に PollBicycleHandshake が Connected へ昇格させる
				if (!bAwaitingBicycleHandshake)
				{
					SetState(EFishingWiredDeviceState::Connected);
				}
				UE_LOG(LogFishing, Log, TEXT("[WiredDevice] ポート接続: COM%d (ID=0x%02X)"), ConnectedCom, TargetDeviceId);
			}
			else
			{
				SetState(EFishingWiredDeviceState::Disconnected);
				// 2026.10.29 Lee(9): まとめ sleep から中断可能な待機へ（停止要求に即応する）
				SleepInterruptible(ReconnectIntervalSec);
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
	// 2026.10.29 Lee(9) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// コントローラ側の探査(COM1〜256)・接続応答待ち・受信待ちループにも中断を要求する。
	// 以前は Run ループ先頭のフラグ検査だけだったため、抜去後の探査や応答待ちの途中では
	// ループ一周分（数十秒〜）戻ってこられず、WaitForCompletion が固まる原因になっていた
#if PLATFORM_WINDOWS
	if (Controller)
	{
		Controller->RequestAbort();
	}
#endif
	// 2026.10.29 Lee(9) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
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

// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
FBicycleMotionSnapshotCore FFishingSerialWorker::GetLatestBicycleMotionSnapshot() const
{
	FScopeLock Lock(&DataMutex);
	return LatestBicycleMotion;
}

// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
TArray<FBicycleCycleEventCore> FFishingSerialWorker::DrainBicycleCycleEvents()
{
	FScopeLock Lock(&DataMutex);
	// 移動で取り出す（呼び出し後にキューは空になる＝履歴破棄もこの 1 回で済む）
	return MoveTemp(BicycleCycleEventQueue);
}

// 2026.10.29 Lee(8) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
FBicycleDrainResult FFishingSerialWorker::DrainBicycleState()
{
	FScopeLock Lock(&DataMutex);
	// 同一ロック内で代次・快照・イベント批次を揃えて返す（3 者の一致ビュー）。
	// 移動で取り出すため呼び出し後にキューは空になる
	FBicycleDrainResult Result;
	Result.Generation = ConnectionGeneration.load(std::memory_order_relaxed);
	Result.Snapshot = LatestBicycleMotion;
	Result.Events = MoveTemp(BicycleCycleEventQueue);
	return Result;
}
// 2026.10.29 Lee(8) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

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
		// 2026.10.28 Lee: 旧 PollBicycle（一問一答ポーリング）から PollBicycleStream へ置換。
		// 実機の 0x22 は 1 バイト状態フレームの連続送信へ切り替わっており、
		// 送信前 clear + 阻塞読み取りの一問一答では受信フレームを廃棄し続ける
		// （docs/自行车设备排查与RPM标定资料_2026-10-28 の実測参照）。
		// 2026.10.28 Lee(2): 接続直後は有効情報応答の検証（握手）を先に完了させる
		if (bAwaitingBicycleHandshake)
		{
			PollBicycleHandshake();
		}
		else
		{
			PollBicycleStream();
		}
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

// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 【旧実装・非呼び出し】旧 PollBicycle（0x20 更新フラグ → 0x22 RPS、30 秒毎に 0x23 方向の
// 一問一答）は PollBicycleStream へ置換済みのため、現在は誰からも呼ばれない。
// 実機の 0x22 が 1 バイト状態フレームの連続送信へ切り替わったため、WriteCmdFlushed
// （送信前 clear）+ 阻塞 ReadData の一問一答では受信フレームを廃棄し続けるのが理由。
// 関数本体は規約に従い削除せず残置している（呼び出し点無し。整理は別タスクで行う）。
// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 2026.10.28 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 【実装削除】直下の注釈ブロックが述べる旧 PollBicycle（0x20 更新フラグ → 0x22 RPS の
// 一問一答）は、クラス宣言からの削除済み（ストリーム化への置換）に伴い、実装本体も
// 2026-10-28 の全量ビルド対応で丸ごと除去した（宣言の無い定義は C2039 を起こすため）。
// 残っているのは説明コメント行のみで、コードは一切残っていない。
// 後継は PollBicycleHandshake（接続直後の情報応答検証）+ PollBicycleStream（逐次消費）。
// 2026.10.28 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 回転方向(0x23)は接続直後 1 回 + 30 秒毎の低頻度更新へ変更。
	// 旧来の 1 秒毎の割込み送信（元コード）は、応答遅延時にフレーム食い違いを起こしやすい
	// （「RPS応答異常」→ 強制再接続の原因）。前作『自転車でGo!』は方向を輪番ポーリングに
	// 含めておらず、方向は実質固定値（取付方向）であるため頻繁に取得する必要がない。
	// 元コード（消さずにコメントで残す）:
	//     NextDirectionPollTimeSec = NowSec + 1.0;
	//     if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdGetDirection) == 0)
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// （旧: 接続直後 1 回 + 30 秒毎の回転方向(0x23)低頻度ポーリング。実装は削除済み）

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

	// （旧: RPS(0x22) 直接ポーリング。実装は削除済み）

	// （旧: RPS 応答からの換算と踩踏検知ログ。実装は削除済み）

	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 踩踏検知ログ（RPS が初めて 0 以外になった時のみ 1 回）。
	// 実機の踏み込みがデータとして届いているかの最初の確認点。 出力されない場合は
	// 更新フラグ(0x20)が立っていない＝デバイス側で RPS が更新されていない疑いがある。
	// （旧: 踩踏検知ログ出力。実装は削除済み）
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// 2026.09.03 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 診断のため RPM(0x21) も併せて取得する。実機で RPS(0x22) の値が踏板に追従しないため、
	// 0x21（符号無し 2Byte, ビッグエンディアン）が追従するかを比較する。
	// ・0x21 が追従する → データ経路を RPM/60 へ切替可能（0x22 は参考値扱いへ）
	// ・両方とも追従しない → デバイス側の測定そのものが未動作（固側への確認事項）
	// 応答異常は致命扱いしない（0x21 未実装の固件でも切断しない。滞留バイトは次回送信前 clear で廃棄）
	// （旧: RPM(0x21) 併取得。実装は削除済み）
	// 2026.09.03 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// （旧: RPS/RPM/方向を FFishingDeviceSampleCore へ格納。実装は削除済み）

// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
void FFishingSerialWorker::PollBicycleHandshake()
{
	const double NowSec = FPlatformTime::Seconds();

	// --- 確認期限の判定: 超過したらポートを解放して再接続へ（半開きのまま滞留させない） ---
	if (NowSec > BicycleHandshakeDeadlineSec)
	{
		ForceReconnect(TEXT("有効情報応答の検証タイムアウト(3秒)"));
		return;
	}

	if (!SerialIF)
	{
		ForceReconnect(TEXT("シリアルIF消失"));
		return;
	}

	// --- 受信バッファを 1 バイトずつ消費し、有効な情報フレームの到達を待つ ---
	// 2026.10.29 Lee(9): 1 回分の消費バイトに予算（MaxDrainBytesPerPoll）を設け、
	// 各バイト処理前に停止要求を検査する。available() の異常値やバースト受信でも
	// この関数が長時間戻らなくなることを構造的に防ぐ（残りは次回ポーリングへ持ち越し）
	int32 Avail = SerialIF->available();
	if (Avail < 0)
	{
		HandleBicycleCommError(TEXT("受信バッファ状態取得失敗"), true);
		return;
	}

	ASerialDataStruct::ASerialData Buf;
	int32 DrainedBytes = 0;
	while (FFishingSerialWorker::ShouldContinueDrain(Avail, DrainedBytes, StopTaskCounter.GetValue() != 0))
	{
		const int32 St = Controller->ReadDataProcess(&Buf);
		if (St == -2)
		{
			// ポート読取失敗（抜去等）: ハード IO エラーとして少数回で切断へ
			HandleBicycleCommError(TEXT("受信読取失敗(検証中)"), true);
			return;
		}
		if (St < 0)
		{
			if (HandleBicycleCommError(TEXT("フレーム解析エラー(検証中)"), false))
			{
				return; // 切断（再接続）へ移行済み
			}
		}
		else
		{
			ConsecutiveReadErrors = 0;
			if (St == 1)
			{
				// 完成フレーム = 校験合法（プラグインの ReadPacketData が完成時のみ返す）。
				// 有効情報応答の判定 = 長さ 4 + payload[0]=デバイスID + payload[1]=デバイスVer
				if (Buf.data_num == 4 && Buf.data[0] == TargetDeviceId && Buf.data[1] == DeviceVerMin)
				{
					// --- 握手確定: 接続代次を +1 し、旧接続の快照・計数を即時クリアしてから
					//     状態ストリーム開始(0x22)を 1 回だけ送り、Connected へ昇格する ---
					bAwaitingBicycleHandshake = false;
					ConnectionGeneration.fetch_add(1, std::memory_order_relaxed);
					ResetBicycleStreamState();
					if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdGetState) != 0)
					{
						ForceReconnect(TEXT("状態フレーム開始(0x22)送信失敗"));
						return;
					}
					SetState(EFishingWiredDeviceState::Connected);
					UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike 握手確認完了: ID=0x%02X Ver=%d → 0x22 送信 (COM%d, Gen=%u)"),
						Buf.data[0], static_cast<int32>(Buf.data[1]),
						ConnectedCom, ConnectionGeneration.load(std::memory_order_relaxed));
					return;
				}
				// 情報フレーム以外（状態/その他）は検証に使わず捨てる（計数のみ）
				ReceiveCount.fetch_add(1, std::memory_order_relaxed);
			}
		}

		++DrainedBytes;
		Avail = SerialIF->available();
		if (Avail < 0)
		{
			HandleBicycleCommError(TEXT("受信バッファ状態取得失敗"), true);
			return;
		}
	}
}

void FFishingSerialWorker::PollBicycleStream()
{
	const double NowSec = FPlatformTime::Seconds();

	// --- 心跳: 10 秒毎にデバイス情報リクエスト(0x01)を送る。
	//     送信前の受信バッファクリアは行わない（滞留中の状態フレームを廃棄しないため） ---
	// 2026.10.29 Lee(9): 送信失敗はハード IO エラーへ分類（抜去の直接的証拠）。
	// 以前は解析ノイズと同じ 20 回許容で、心跳間隔(10秒)との合わせ技で検知が
	// 200 秒以上遅れるケースがあった
	if (NowSec >= NextHeartbeatTimeSec)
	{
		NextHeartbeatTimeSec = NowSec + HeartbeatIntervalSec;
		if (Controller->WriteData(UFishingWiredDeviceSettings::BicycleCmdHeartbeat) != 0)
		{
			HandleBicycleCommError(TEXT("心跳(0x01)送信失敗"), true);
			return;
		}
	}

	// --- 心跳タイムアウト判定: 生存判定は有効情報応答（4 バイト）のみを根拠にする。
	//     静止中は状態フレームが来ないのが正常なため、状態フレームの停止は断線に数えない ---
	// （静止時の誤切断防止の前提は本判定のまま変更しない。情報応答は静止中も心跳ごとに届く）
	if (NowSec - LastInfoResponseSec > HeartbeatTimeoutSec)
	{
		ForceReconnect(TEXT("情報応答タイムアウト(心跳無応答)"));
		return;
	}

	if (!SerialIF)
	{
		ForceReconnect(TEXT("シリアルIF消失"));
		return;
	}

	// --- 受信バッファに滞留しているバイトを 1 バイトずつ消費する（クリアしない）。
	//     ReadDataProcess は 1 呼び出しで高々 1 バイト消費し、フレーム完成(1)/解析中(0)/
	//     解析エラー(-1)/ポート読取失敗(-2)を返す。逐次消費なので分割・粘着フレームどちらも漏らさない。
	//     2026.10.29 Lee(9): 消費バイトに予算を設け、各バイト処理前に停止要求を検査する
	//     （available() の異常値による実質無限ループ＝抜去後の PIE 終了固死の根因対策） ---
	int32 Avail = SerialIF->available();
	if (Avail < 0)
	{
		HandleBicycleCommError(TEXT("受信バッファ状態取得失敗"), true);
		return;
	}

	ASerialDataStruct::ASerialData Buf;
	int32 DrainedBytes = 0;
	while (FFishingSerialWorker::ShouldContinueDrain(Avail, DrainedBytes, StopTaskCounter.GetValue() != 0))
	{
		const int32 St = Controller->ReadDataProcess(&Buf);
		if (St == -2)
		{
			// ポート読取失敗（抜去等）: ハード IO エラーとして少数回で切断へ
			HandleBicycleCommError(TEXT("受信読取失敗"), true);
			return;
		}
		if (St < 0)
		{
			// 解析エラー（校験不一致/AD 連続）。1 バイト単位のノイズは日常範囲として
			// 連続計数のみで監視し、閾値超過時のみ切断へ
			if (HandleBicycleCommError(TEXT("フレーム解析エラー"), false))
			{
				return; // 切断（再接続）へ移行済み
			}
		}
		else
		{
			ConsecutiveReadErrors = 0;
			if (St == 1)
			{
				// 1 フレーム完成（状態/情報/その他のいずれか）
				ProcessBicycleFrame(Buf, NowSec);
				ReceiveCount.fetch_add(1, std::memory_order_relaxed);
			}
		}

		++DrainedBytes;
		Avail = SerialIF->available();
		if (Avail < 0)
		{
			HandleBicycleCommError(TEXT("受信バッファ状態取得失敗"), true);
			return;
		}
	}
}

// 2026.10.29 Lee(4) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
int32 FFishingSerialWorker::ClassifyBicycleFrame(int32 DataNum, int32 FirstByte, int32 SecondByte,
	uint8 TargetDeviceId, uint8 DeviceVer)
{
	// 有効情報フレーム = 長さ 4 + payload[0]=デバイスID + payload[1]=デバイスVer
	if (DataNum == 4)
	{
		return (FirstByte == static_cast<int32>(TargetDeviceId)
			&& SecondByte == static_cast<int32>(DeviceVer)) ? 0 : 2;
	}

	// 状態フレーム = 長さ 1 + 値 0..3
	if (DataNum == 1)
	{
		return (FirstByte >= 0 && FirstByte <= 3) ? 1 : 2;
	}

	// その他（長さ 0 / 2,3 / 5 以上）
	return 2;
}
// 2026.10.29 Lee(4) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

void FFishingSerialWorker::ProcessBicycleFrame(const ASerialDataStruct::ASerialData& Buf, double NowSec)
{
	// --- フレーム分類（純関数。テスト対象）---
	// 2026.10.29 Lee(4): 従来は data_num==4 を無条件で情報フレーム（生存根拠）としていたが、
	// ID/Ver 不一致の 4 バイト（他機器の応答・旧固件の RPS 応答など）まで LastInfoResponseSec
	// を更新していた。有効情報（長さ 4 + ID/Ver 一致。校験合法性は ReadPacketData が
	// 完成フレームのみ返すことで保証済み）だけを生存根拠とし、それ以外の 4 バイトは他フレーム扱い
	const int32 FrameKind = ClassifyBicycleFrame(
		static_cast<int32>(Buf.data_num),
		static_cast<int32>(Buf.data[0]),
		static_cast<int32>(Buf.data[1]),
		TargetDeviceId, DeviceVerMin);

	// --- 有効情報フレーム: 心跳/握手応答。生存判定の唯一の根拠（運動には取込まない） ---
	if (FrameKind == 0)
	{
		++InfoFrameCount;
		LastInfoResponseSec = NowSec;
		if (!bLoggedHandshakeOk)
		{
			bLoggedHandshakeOk = true;
			UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike 有効な情報応答を確認: ID=0x%02X Ver=%d"),
				Buf.data[0], static_cast<int32>(Buf.data[1]));
		}
		return;
	}

	// --- 状態フレーム（1 バイト, 0..3）: デコーダへ取込む（静止時は送られてこない） ---
	if (FrameKind == 1)
	{
		++StateFrameCount;
		BicycleDecoder.Feed(NowSec, Buf.data[0]);

		// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// 完走周期イベントをゲームスレッド転送キューへ転写する（接続代次を添付）。
		// 1 辺沿の Feed で完成する周期は高々 1 つだが、一般化して drain で全件転写する
		FBicycleCycleEvent RawEvents[16];
		const int NumEvents = BicycleDecoder.DrainCycleEvents(RawEvents, 16);
		for (int i = 0; i < NumEvents; ++i)
		{
			FBicycleCycleEventCore CoreEvent;
			CoreEvent.DeviceTimeSec = RawEvents[i].TimestampSec;
			CoreEvent.Dir = RawEvents[i].Dir;
			CoreEvent.AvgIntervalSec = RawEvents[i].AvgIntervalSec;
			CoreEvent.Generation = ConnectionGeneration.load(std::memory_order_relaxed);
			EnqueueBicycleCycleEvent(CoreEvent);
		}
		// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

		if (!bLoggedFirstStateFrame)
		{
			bLoggedFirstStateFrame = true;
			UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike 状態フレーム受信開始: 初回状態=%d"), static_cast<int32>(Buf.data[0]));
		}
		UpdateBicycleMotionSnapshot(NowSec);
		return;
	}

	// --- その他のフレーム（ID/Ver 不一致の 4 バイト / 長さ 0 / 2,3 バイト / 状態値域外など）は計数のみ。
	//     生存判定（LastInfoResponseSec）は更新しない ---
	++OtherFrameCount;
}

void FFishingSerialWorker::UpdateBicycleMotionSnapshot(double NowSec)
{
	const FBicycleMotionDecoderSnapshot Snap = BicycleDecoder.MakeSnapshot(NowSec);

	FBicycleMotionSnapshotCore Core;
	Core.bValid = true;
	Core.TimestampSec = NowSec;
	Core.Rpm = Snap.Rpm;
	Core.PhysDir = Snap.PhysDir;
	Core.Phase = Snap.Phase;
	Core.ForwardCycles = Snap.ForwardCycles;
	Core.ReverseCycles = Snap.ReverseCycles;
	Core.NetEdges = Snap.NetEdges;
	Core.InvalidTransitions = Snap.InvalidTransitions;
	// 2026.10.29 Lee: 完走圈の裏付けの古さ判定用（使用側は AgeSeconds と加算して使う）
	Core.SecondsSinceLastCycle = Snap.SecondsSinceLastCycle;
	Core.Generation = ConnectionGeneration.load(std::memory_order_relaxed);
	Core.StateFrameCount = StateFrameCount;
	Core.InfoFrameCount = InfoFrameCount;
	Core.OtherFrameCount = OtherFrameCount;

	{
		FScopeLock Lock(&DataMutex);
		LatestBicycleMotion = Core;
	}
}

void FFishingSerialWorker::ResetBicycleStreamState()
{
	BicycleDecoder.Reset();
	StateFrameCount = 0;
	InfoFrameCount = 0;
	OtherFrameCount = 0;
	ConsecutiveReadErrors = 0;
	bLoggedFirstStateFrame = false;
	bLoggedHandshakeOk = false;
	// 2026.10.28 Lee(2): 握手待ちフラグも併せて解除（断線時の取り残しを防ぐ）
	bAwaitingBicycleHandshake = false;

	const double NowSec = FPlatformTime::Seconds();
	NextHeartbeatTimeSec = NowSec + HeartbeatIntervalSec;
	LastInfoResponseSec = NowSec; // 接続時点（ID/Ver 照合済み）を生存基準の初期値へ

	{
		FScopeLock Lock(&DataMutex);
		LatestBicycleMotion = FBicycleMotionSnapshotCore(); // 旧接続の快照を即時クリア
		// 2026.10.29 Lee: 旧接続の完走周期イベントも即時破棄する（補算されない履歴として捨てる）
		BicycleCycleEventQueue.Empty();
		// 2026.10.29 Lee(7) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// 現在の接続代次を初回状態フレームを待たずに即時发布する（状態フレーム/RPM の
		// 伪造はしない。bValid=false・Rpm=0 のまま代次のみを更新する）。
		// 低フレームレートで握手直後の完走圈イベントが先着しても、Bridge が新代次を
		// 先に読めるため「新阶段の首圈を履歴として誤廃棄する」事故を防ぐ。
		// キューのクリアと代次发布を同一ロック内で行い、両者の読み取り一貫性を保証する
		LatestBicycleMotion.Generation = ConnectionGeneration.load(std::memory_order_relaxed);
		// 2026.10.29 Lee(7) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	}
}

// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
void FFishingSerialWorker::EnqueueBicycleCycleEvent(const FBicycleCycleEventCore& Event)
{
	FScopeLock Lock(&DataMutex);
	if (BicycleCycleEventQueue.Num() >= MaxCycleEventQueueSize)
	{
		// 満杯: 最古のイベントを破棄して警告を出す（静かに捨てない）。
		// 滞留は消費側（ゲームスレッド）の長時間停止・フェーズ管理漏れを示す
		BicycleCycleEventQueue.RemoveAt(0);
		UE_LOG(LogFishing, Warning, TEXT("[WiredDevice] Bike 完走周期イベントキュー満杯: 最古イベントを破棄 (容量=%d)。消費側のTick停止/フェーズ管理を確認してください"), MaxCycleEventQueueSize);
	}
	BicycleCycleEventQueue.Add(Event);
}
// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

// 2026.10.29 Lee(9) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
bool FFishingSerialWorker::HandleBicycleCommError(const TCHAR* Reason, bool bHardIoError)
{
	++ConsecutiveReadErrors;
	// ハード IO エラー（抜去等）と解析ノイズで閾値を分ける（純関数へ集約・テスト対象）
	if (ShouldReconnectOnCommError(bHardIoError, ConsecutiveReadErrors))
	{
		ForceReconnect(Reason);
		return true;
	}
	UE_LOG(LogFishing, Log, TEXT("[WiredDevice] Bike 通信異常をスキップ: %s (連続%d回 / %s)"),
		Reason, ConsecutiveReadErrors, bHardIoError ? TEXT("ハードIO") : TEXT("解析"));
	return false;
}

void FFishingSerialWorker::SleepInterruptible(float Seconds)
{
	const double EndSec = FPlatformTime::Seconds() + Seconds;
	// 20ms 刻みで停止要求を検査する（終了要求から最短 20ms で抜けられる）
	while (StopTaskCounter.GetValue() == 0 && FPlatformTime::Seconds() < EndSec)
	{
		FPlatformProcess::Sleep(0.02f);
	}
}
// 2026.10.29 Lee(9) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

void FFishingSerialWorker::ForceReconnect(const TCHAR* Reason)
{
	UE_LOG(LogFishing, Warning, TEXT("[WiredDevice] 再接続へ: %s (最終エラーコード=0x%04X)"), Reason, GetLastErrorCode());
	// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 自転車は再接続へ回る直前に旧快照・受信計数・解码器を即時クリアする
	// （切断後も旧データが参照され続けるのを防ぐ。接続代次は次の接続確立時に +1 される。
	//  アームトラッカーの経路には一切触れない）
	if (DeviceType == EFishingWiredDeviceType::Bicycle)
	{
		ResetBicycleStreamState();
	}
	// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	if (Controller->GetConnectionState())
	{
		Controller->DisConnectDevice();
	}
	ConnectedCom = 0;
	SetState(EFishingWiredDeviceState::Disconnected);
	// 次のループで再接続試行の前に待機させる
	// 2026.10.29 Lee(9): まとめ sleep から中断可能な待機へ（終了要求の応答性向上）
	SleepInterruptible(ReconnectIntervalSec);
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
