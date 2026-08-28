// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/Queue.h"
#include <atomic>
#include "Lee/device/FishingWiredDeviceTypes.h"

class UASerialLibControllerWin;
class WindowsSerial;

/**
 * @brief ASerial デバイスとの接続・ポーリングを担うバックグラウンドスレッド（FRunnable）。
 * @note 構成は Magic_Project の UDeviceThreadManager / FMagicDeviceCmdSender のパターンを踏襲。
 *       - Controller（UASerialLibControllerWin）はゲームスレッドで NewObject し、サブシステムが UPROPERTY で保持
 *         （ワーカーからは生成/破棄しないため GC の干渉を受けない。メソッド呼び出しは純粋な C++ 処理のみ）。
 *       - WindowsSerial（Win32 COM ポート）はワーカーが所有する非 UObject クラス。
 *       - ASerial は応答型通信（デバイスから自発的送信は無い）ため、Run() 内でコマンド送信→応答受信を
 *         常時繰り返す（自転車でGo! の DeviceCmdSender と同じ循環ポーリング方式）。
 * @note このクラスは Win64 専用プラグイン ASerialCom に依存する。ASerialCom の実体利用はすべて
 *       PLATFORM_WINDOWS ガード内に限定し、他プラットフォームでは何もしない空回転スレッドになる。
 */
class FFishingSerialWorker : public FRunnable
{
public:
	/**
	 * @brief ワーカーを生成する。
	 * @param InController ゲームスレッドで生成済みの ASerial コントローラ（初期化済み・IF 未設定）
	 * @param InTargetDeviceId 照合するデバイスID（0x02=アームトラッカー / 0x03=自転車）
	 * @param InDeviceVerMin 許容するデバイスVerの下限
	 * @param InDeviceVerMax 許容するデバイスVerの上限
	 * @param InDeviceType ポーリング内容を決めるデバイス種別
	 * @param InPollIntervalSec ポーリング周期[秒]
	 * @param InReconnectIntervalSec 再接続試行間隔[秒]
	 * @param InComPortOverride COMポート指定（0=自動探査）
	 */
	FFishingSerialWorker(UASerialLibControllerWin* InController,
		uint8 InTargetDeviceId, uint8 InDeviceVerMin, uint8 InDeviceVerMax,
		EFishingWiredDeviceType InDeviceType,
		float InPollIntervalSec, float InReconnectIntervalSec, int32 InComPortOverride);

	virtual ~FFishingSerialWorker() override;

	// --- FRunnable インターフェース ---
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/** @brief アームトラッカーのデバイス側キャリブレーション(0x20)送信を要求する（スレッド安全・キュー経由） */
	void RequestCalibration();

	/** @brief 最新サンプルのコピーを取得（mutex 保護・ゲームスレッドから呼ぶ） */
	FFishingDeviceSampleCore GetLatestSample() const;

	/** @brief 接続状態を取得（スレッド安全） */
	EFishingWiredDeviceState GetState() const;

	/** @brief 接続中の COM ポート番号（未接続時 0） */
	int32 GetConnectedComPort() const;

	/** @brief 直近の ASerial エラーコード（ASerial::ErrorCodeList 値） */
	uint16 GetLastErrorCode() const;

	/** @brief 累計受信成功回数（動作確認用カウンタ） */
	uint32 GetReceiveCount() const;

private:
	/** @brief デバイス種別に応じた 1 回分のポーリング（コマンド送信→応答解析→サンプル更新） */
	void PollDevice();

	/** @brief アームトラッカー用ポーリング（0x21 オイラー角） */
	void PollArmTracker();

	/** @brief 自転車デバイス用ポーリング（0x20 更新フラグ → 0x22 RPS、1秒毎に 0x23 方向） */
	void PollBicycle();

	/** @brief 接続断とみなしてポートを閉じ、再接続シーケンスへ遷移させる */
	void ForceReconnect(const TCHAR* Reason);

	/** @brief 状態を更新する（スレッド安全） */
	void SetState(EFishingWiredDeviceState NewState);

	/** @brief サンプルを共有バッファへ格納する（mutex 保護） */
	void StoreSample(const FFishingDeviceSampleCore& Sample);

	// --- スレッド制御 ---
	FThreadSafeCounter StopTaskCounter;

	// --- 共有データ ---
	mutable FCriticalSection DataMutex;
	FFishingDeviceSampleCore LatestSample;
	mutable FCriticalSection StateMutex;
	EFishingWiredDeviceState State = EFishingWiredDeviceState::Idle;
	std::atomic<uint32> ReceiveCount{ 0 };

	/** @brief ゲームスレッド→ワーカーへのコマンド要求キュー（値はそのまま ASerial コマンドバイト） */
	TQueue<int32, EQueueMode::Mpsc> CommandQueue;

	// --- ASerial 関連（Win64 のみ実体を持つ） ---
	/** ゲームスレッド生成の ASerial コントローラ（所有はサブシステムの UPROPERTY） */
	UASerialLibControllerWin* Controller = nullptr;
	/** ワーカー所有の Win32 シリアル IF */
	WindowsSerial* SerialIF = nullptr;

	// --- 設定（コンストラクタ以降不変） ---
	uint8 TargetDeviceId = 0;
	uint8 DeviceVerMin = 0;
	uint8 DeviceVerMax = 0;
	EFishingWiredDeviceType DeviceType = EFishingWiredDeviceType::ArmTracker;
	float PollIntervalSec = 0.008f;
	float ReconnectIntervalSec = 1.0f;
	int32 ComPortOverride = 0;

	// --- ポーリング内部状態（ワーカースレッドのみ触る） ---
	double NextPollTimeSec = 0.0;
	double NextDirectionPollTimeSec = 0.0;
	int32 ConnectedCom = 0;
	int32 ConsecutiveFailures = 0;
	uint8 LastDirection = 1;
};
