// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/Queue.h"
#include <atomic>
#include "Lee/device/FishingWiredDeviceTypes.h"
#include "Lee/device/FishingBicycleMotionDecoder.h"

class UASerialLibControllerWin;
class WindowsSerial;

// 2026.10.28 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
// ASerialDataStruct::ASerialData の前方宣言。
// 実体は Win64 専用プラグイン ASerialCom（ASerialCore/ASerialPacket.h）が定義するが、
// 同ヘッダは Windows.h 経由の依存を含むためワーカー ヘッダからは include できない
// （Android ビルドではプラグインごと存在しない）。本クラスは参照渡しのみで使用するため
// 前方宣言で足り、実体の include は cpp 側の PLATFORM_WINDOWS ガード内で行う。
// （2026-10-28 全量ビルドの C2653/C4430/C2143 対応）
namespace ASerialDataStruct
{
	struct ASerialData;
}
// 2026.10.28 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーー

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

	// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 自転車の運動復号スナップショットのコピーを取得（mutex 保護・ゲームスレッドから呼ぶ）。
	 *  @return bValid=false の場合は接続後に状態フレームを未受信（旧接続の快照は再接続時に即時クリア済み） */
	FBicycleMotionSnapshotCore GetLatestBicycleMotionSnapshot() const;

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 蓄積した完走周期イベントを全て取り出してキューを空にする（mutex 保護・ゲームスレッドから呼ぶ）。
	 * @note フェーズ切替・再接続・方向設定変更時の「履歴破棄」も本メソッドで行う
	 *       （取り出した配列を破棄すればよい）。戻り値は受信順（時系列順）に並ぶ。
	 */
	TArray<FBicycleCycleEventCore> DrainBicycleCycleEvents();

	// 2026.10.29 Lee(8) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 接続代次・復号快照・完走周期イベント批次を同一ロック内で取得する（ゲームスレッドから呼ぶ）。
	 * @return 取得後キューは空になる。代次（消費基準）とイベント（対象）の対応が
	 *         同一ロック取得分として保証されるため、Bridge 側で快照（旧経路では ticker
	 *         缓存）を先に読んでから drain する間に再接続が挟まる競合（旧代次の読み取り後に
	 *         新代次イベントを取得して全破棄する事故）が閉じる。
	 */
	FBicycleDrainResult DrainBicycleState();
	// 2026.10.29 Lee(8) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 接続状態を取得（スレッド安全） */
	EFishingWiredDeviceState GetState() const;

	// 2026.10.29 Lee(4) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 受信フレームを分類する（純関数・自動テスト対象）。
	 * @param DataNum フレームの payload 長（0..32）
	 * @param FirstByte payload[0]（DataNum < 1 の場合は 0 を渡す）
	 * @param SecondByte payload[1]（DataNum < 2 の場合は 0 を渡す）
	 * @param TargetDeviceId 照合するデバイスID
	 * @param DeviceVer 照合するデバイスVer
	 * @return 0=有効情報フレーム（生存根拠）/ 1=状態フレーム（運動取込み）/ 2=その他
	 * @note 有効情報 = 長さ 4 + payload[0]=デバイスID + payload[1]=デバイスVer。
	 *       帧校験合法性はプラグインの ReadPacketData が校験合格の完成フレームのみ返すことで
	 *       前提保証される（本関数は ID/Ver/長さの組み合わせのみを判定する）。
	 *       ID/Ver 不一致の 4 バイト（他機器応答・旧固件の RPS 応答など）は生存根拠にしない。
	 */
	static int32 ClassifyBicycleFrame(int32 DataNum, int32 FirstByte, int32 SecondByte,
		uint8 TargetDeviceId, uint8 DeviceVer);
	// 2026.10.29 Lee(4) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// 2026.10.29 Lee(9) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** ポートレベルのハード IO エラー（available/read/write 失敗）の連続許容数。
	 *  抜去の直接的証拠のため少数で切断へ（解析ノイズの 20 回とは別枠） */
	static constexpr int32 MaxConsecutiveHardIoErrors = 2;

	/** 受信 drain の 1 ポーリング回あたりの消費バイト上限。
	 *  available() の異常値やデバイスのバースト送信でも 1 回分の処理時間が
	 *  上限バイト数で頭打ちになり、スレッド停止・心跳判定が遅れないようにする。
	 *  残りは次回ポーリング（8ms 後）へ持ち越し（解析状態は byte 単位で保持される） */
	static constexpr int32 MaxDrainBytesPerPoll = 256;

	/**
	 * @brief 受信 drain ループの継続可否を判定する（純関数・自動テスト対象）。
	 * @param AvailBytes ポート受信バッファの滞留バイト数（available() の戻り値）
	 * @param DrainedBytes 本ポーリング回で消費済みのバイト数
	 * @param bStopRequested スレッド停止要求済みか（StopTaskCounter != 0）
	 * @return true = ループ継続 / false = 打ち切り
	 * @note  滞留が残っていても「予算（MaxDrainBytesPerPoll）消化済み」または
	 *        「停止要求」なら打ち切る。以前は available() の異常値で実質無限ループに
	 *        なり得たため、毎回の継続判定を純関数へ集約した。
	 */
	static bool ShouldContinueDrain(int32 AvailBytes, int32 DrainedBytes, bool bStopRequested)
	{
		return AvailBytes > 0 && DrainedBytes < MaxDrainBytesPerPoll && !bStopRequested;
	}

	/**
	 * @brief 連続通信異常の再接続判定（純関数・自動テスト対象）。
	 * @param bHardIoError true = ポートレベルのハード IO エラー（available/read/write 失敗）/
	 *                     false = フレーム解析エラー（校験不一致などのソフトノイズ）
	 * @param ConsecutiveCount 連続異常回数（正常受信で 0 へ戻る）
	 * @return true = 再接続（ForceReconnect）へ移行すべき
	 * @note  ハード IO エラーは抜去の直接的証拠のため少数（2 回）で切断へ。
	 *        ソフト解析エラーは 1 byte ノイズが日常範囲のため従来どおり 20 回許容する
	 */
	static bool ShouldReconnectOnCommError(bool bHardIoError, int32 ConsecutiveCount)
	{
		return ConsecutiveCount >= (bHardIoError ? MaxConsecutiveHardIoErrors : MaxConsecutiveReadErrors);
	}
	// 2026.10.29 Lee(9) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

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

	// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 自転車デバイス用のストリーム受信（2026.10.28 に PollBicycle から置換）。
	 * @note 旧 PollBicycle（0x20 更新フラグ → 0x22 RPS、30 秒毎に 0x23 方向の一問一答）は
	 *       「送信前 clear + 阻塞 ReadData」方式のため、0x22 が状態フレーム連続送信に
	 *       切り替わった実機（2026-10-28 実測）ではフレームを廃棄し続ける。本方式では
	 *       受信バッファをクリアせず 1 バイトずつ消費し、完成フレームを分類する:
	 *       - 1 バイト(0..3) = 状態フレーム → FBicycleMotionDecoder へ取込み
	 *       - 4 バイト = 情報フレーム（心跳/握手応答）→ 生存判定のみ（運動には取込まない）
	 *       心跳(0x01)は 10 秒毎に送信し、有効情報応答が長時間無ければ切断へ。
	 *       静止時は状態フレームが来ないのが正常なため、断線判定には使わない。
	 */
	void PollBicycleStream();

	/**
	 * @brief 接続直後の情報応答検証（ConnectDevice の照合には長さ検証が無いため必要）。
	 * @note  0x01 送信後、実受信したフレームが「長さ 4 + payload[0]=デバイスID + payload[1]=デバイスVer」
	 *        （校験合法はプラグインの ReadPacketData が完成フレームのみ返すことで保証）であることを
	 *        確認した時点で Connected へ昇格し、状態ストリーム開始(0x22)を送る。
	 *        確認期限を超えたらポートを解放して再接続へ。それ以外のフレームは検証に使わず捨てる。
	 */
	void PollBicycleHandshake();

	/** @brief 完成した ASerial フレームを分類して処理する（状態/情報/その他） */
	void ProcessBicycleFrame(const ASerialDataStruct::ASerialData& Buf, double NowSec);

	/** @brief デコーダの現值から運動スナップショットを組んで共有バッファへ格納する */
	void UpdateBicycleMotionSnapshot(double NowSec);

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 完走周期イベントを転送キューへ積む（ワーカー内。満杯時は最古を破棄し警告ログ） */
	void EnqueueBicycleCycleEvent(const FBicycleCycleEventCore& Event);
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 自転車ストリーム系の内部状態を初期化する（接続確立時と再接続直前に呼ぶ）。
	 *  @note 解码器・受信計数・心跳タイマを初期化し、旧接続の快照を即時クリアする */
	void ResetBicycleStreamState();

	// 2026.10.29 Lee(9) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 自転車の通信異常を連続計数し、閾値超過で切断へ（旧 HandleBicycleReadError の拡張）。
	 *  @param bHardIoError true = ポートレベルの失敗（available/read/write のシステムコール異常）。
	 *                      false = フレーム解析エラー（校験不一致などのソフトノイズ）
	 *  @return true = 連続超過で再接続へ移行済み（呼び出し側は即座に処理を打ち切ること）。
	 *          ハード IO エラーは ShouldReconnectOnCommError(true, n)（2 回）で判定し、
	 *          ソフト解析エラーは同 (false, n)（20 回）で判定する */
	bool HandleBicycleCommError(const TCHAR* Reason, bool bHardIoError);
	// 2026.10.29 Lee(9) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 接続断とみなしてポートを閉じ、再接続シーケンスへ遷移させる */
	void ForceReconnect(const TCHAR* Reason);

	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 送信前に受信バッファをクリアしてからコマンドを送る（応答型一問一答の徹底）。
	 *  @return WriteData の戻り値（0=成功）
	 *  @note 遅延した前回応答を受信バッファへ残さないことで、ReadData の応答が
	 *        必ず直前の送信へのものになる（フレーム食い違いの根本対策） */
	int32 WriteCmdFlushed(uint8 Cmd);

	// 2026.10.29 Lee(9) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 停止要求を検査しながら小刻みにスリープする（終了パスの遅延上限用）。
	 *  @note  ForceReconnect 直後の再接続待ち等に使用。20ms 刻みで StopTaskCounter を
	 *         検査するため、停止要求から最短 20ms で起床する（旧来のまとめ sleep は
	 *         ReconnectIntervalSec 丸ごと待ってからでないと停止できなかった） */
	void SleepInterruptible(float Seconds);
	// 2026.10.29 Lee(9) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief ポーリング異常（応答長異常・タイムアウト）の復旧。バッファをクリアし連続失敗を加算、
	 *        閾値（20 回）超過時のみ再接続へ。前作『自転車でGo!』の「異常時も切断しない」方式に準拠 */
	void HandlePollFailure(const TCHAR* Reason);
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

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
	/** 照合するデバイスID（0x02=アームトラッカー / 0x03=自転車） */
	uint8 TargetDeviceId = 0;
	/** 許容するデバイスVerの下限 */
	uint8 DeviceVerMin = 0;
	/** 許容するデバイスVerの上限 */
	uint8 DeviceVerMax = 0;
	/** ポーリング内容を決めるデバイス種別 */
	EFishingWiredDeviceType DeviceType = EFishingWiredDeviceType::ArmTracker;
	/** ポーリング周期 [秒] */
	float PollIntervalSec = 0.008f;
	/** 再接続試行間隔 [秒] */
	float ReconnectIntervalSec = 1.0f;
	/** COMポート指定（0=自動探査） */
	int32 ComPortOverride = 0;

	// --- ポーリング内部状態（ワーカースレッドのみ触る） ---
	/** 次回ポーリング予定時刻（FPlatformTime::Seconds() 基準） */
	double NextPollTimeSec = 0.0;
	/** 次回の回転方向(0x23)ポーリング予定時刻（自転車のみ使用・1秒毎） */
	double NextDirectionPollTimeSec = 0.0;
	/** 接続済み COM ポート番号（未接続 0。StateMutex 保護） */
	int32 ConnectedCom = 0;
	/** 応答タイムアウトの連続回数（20 超で切断扱い。成功時に 0 へ戻す） */
	int32 ConsecutiveFailures = 0;
	/** 最後に受信した回転方向（1 秒毎の 0x23 ポーリング結果を RPS 取得時に添付する） */
	uint8 LastDirection = 1;
	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 踩踏検知ログ（初回 RPS≠0）を出力済みか（自転車のみ使用・1 回限り） */
	bool bLoggedFirstNonZeroRps = false;
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 2026.09.03 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 直近の RPM(0x21) 応答の生値（診断併取得用。読取失敗時は前回値を維持） */
	float RpmFromDevice = 0.0f;
	// 2026.09.03 Lee(2) end

	// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// --- 自転車ストリーム受信の内部状態（ワーカースレッドのみ触る） ---
	/** 心跳送信間隔 [秒]（0x01 デバイス情報リクエスト） */
	static constexpr double HeartbeatIntervalSec = 10.0;
	/** 心跳タイムアウト [秒]。この間有情報応答が無ければ切断へ（2.5 周期分） */
	static constexpr double HeartbeatTimeoutSec = 25.0;
	/** 読取エラーの連続許容数（超過で切断へ。解析エラー 1 バイトは日常範囲） */
	static constexpr int32 MaxConsecutiveReadErrors = 20;

	/** 接続後の有効情報応答（握手）確認の制限時間 [秒]。超過でポート解放→再接続へ */
	static constexpr double HandshakeConfirmTimeoutSec = 3.0;

	/** @brief 接続済みだが有効情報応答（長さ4+ID/Ver 一致）を未確認の間 true（Connected 前の待ち） */
	bool bAwaitingBicycleHandshake = false;

	/** 握手確認の期限時刻（FPlatformTime::Seconds() 基準） */
	double BicycleHandshakeDeadlineSec = 0.0;

	/** 自転車の生状態（0..3）を回転へ復号する独立デコーダ */
	FBicycleMotionDecoder BicycleDecoder;
	/** 運動復号スナップショットの共有バッファ（DataMutex 保護） */
	FBicycleMotionSnapshotCore LatestBicycleMotion;

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** 完走周期イベントの転送キュー（DataMutex 保護。ワーカーが enqueue・ゲームスレッドが drain）。
	 *  満杯時は最古を破棄して警告ログを出す（静かに捨てない） */
	TArray<FBicycleCycleEventCore> BicycleCycleEventQueue;

	/** 完走周期イベントキューの容量上限（低フレームレートの滞留吸収用。超過は警告付きで破棄） */
	static constexpr int32 MaxCycleEventQueueSize = 256;
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** 接続代次（接続確立のたび +1。快照へ複写する） */
	std::atomic<uint32> ConnectionGeneration{ 0 };
	/** 次回心跳送信予定時刻（FPlatformTime::Seconds() 基準） */
	double NextHeartbeatTimeSec = 0.0;
	/** 最後に有効情報フレーム（4 バイト）を受信した時刻（生存判定の基準） */
	double LastInfoResponseSec = 0.0;
	/** 解析/読取エラーの連続数（1 フレーム成功で 0 へ戻す） */
	int32 ConsecutiveReadErrors = 0;
	/** 状態フレーム（1 バイト 0..3）の受信累計（診断用・再接続でクリア） */
	uint32 StateFrameCount = 0;
	/** 情報フレーム（4 バイト）の受信累計（診断用・再接続でクリア） */
	uint32 InfoFrameCount = 0;
	/** その他フレームの受信累計（診断用・再接続でクリア） */
	uint32 OtherFrameCount = 0;
	/** @brief 初回状態フレーム受信のログ（1 回限り）を出力済みか */
	bool bLoggedFirstStateFrame = false;
	/** @brief 有効な情報応答（ID/Ver 一致）確認のログ（1 回限り）を出力済みか */
	bool bLoggedHandshakeOk = false;
	// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーー
};
