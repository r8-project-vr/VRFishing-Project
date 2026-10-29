// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Lee/device/FishingWiredDeviceTypes.h"
#include "FishingWiredDeviceSubsystem.generated.h"

class UASerialLibControllerWin;
class FFishingSerialWorker;
class FRunnableThread;

/**
 * @brief 1 デバイス（アームトラッカー/自転車）分の接続資産を保持するスロット。
 * @note Controller のみ UObject であるため UPROPERTY で GC 保護する。Worker/Thread は純 C++ ポインタ（非反映）。
 *       サブシステムが FDeviceWorkerSlot[2] を所有し、[0]=アームトラッカー / [1]=自転車 として 2 台同時接続する。
 */
USTRUCT()
struct FDeviceWorkerSlot
{
	GENERATED_BODY()

	/** ASerial コントローラ（ゲームスレッドで生成し GC から保護。ワーカーからはメソッド呼び出しのみ） */
	UPROPERTY()
	TObjectPtr<UASerialLibControllerWin> Controller = nullptr;

	/** シリアルワーカースレッド実体（所有者は本スロット） */
	FFishingSerialWorker* Worker = nullptr;

	/** スレッドハンドル（Worker の実行主体。所有者は本スロット） */
	FRunnableThread* Thread = nullptr;
};

/**
 * @brief 有線デバイス（ASerial/UART）の接続ライフサイクルと最新データ配信を担う GameInstance サブシステム。
 * @note 役割:
 *       - Win64 実機モード: デバイス種別ごとに ASerialCom コントローラ + バックグラウンドスレッド
 *         （FFishingSerialWorker）を 1 本ずつ持ち、COM 自動探査 → デバイスID照合 → 応答型ポーリングを行う。
 *         アームトラッカー(0x02)と自転車(0x03)は同時接続可能（2026-09-02 Lee、双ワーカー化）。
 *       - 模擬モード（bUseSimulator=true, 実機未完成向け）: シリアルポートを開かず正弦波の模擬データを流す。
 *         模擬→実機の切替は Project Settings「Fishing Wired Device (有線デバイス設定)」のみで行える。
 * @note データ取得はプル型（ブリッジコンポーネントの Tick から GetLatestArmEuler/GetLatestBicycleSample を読む）。
 *       ゲームロジック（状態機・Widget）は本サブシステムを直接知る必要がない。
 */
UCLASS()
class VRFISHING_API UFishingWiredDeviceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** @brief 接続状態変化通知（DeviceType=どのデバイスか / bConnected=接続済みか / ComPort=接続先ポート番号、模擬モードでは 0） */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWiredDeviceStateChanged, EFishingWiredDeviceType, DeviceType, bool, bConnected, int32, ComPort);

	// --- USubsystem インターフェース ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * @brief 接続処理を開始する（実機: 有効種別ごとにワーカースレッド起動 / 模擬: 模擬データ生成開始）。
	 * @note 二重呼び出しは無視される。Win64 以外で実機モードの場合は模擬モードへフォールバックする。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	void StartConnection();

	/** @brief 接続を停止し、全スレッド/模擬生成を終了する */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	void StopConnection();

	/** @brief いずれかのデバイス（模擬モード含む）が接続済みか */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	bool IsConnected() const;

	/** @brief 模擬データモードで動作中か */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	bool IsSimulatorMode() const;

	/** @brief 指定デバイスの現在の接続状態 */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	EFishingWiredDeviceState GetDeviceState(EFishingWiredDeviceType DeviceType) const;

	/** @brief 指定デバイスが接続済みか */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	bool IsDeviceConnected(EFishingWiredDeviceType DeviceType) const;

	/** @brief 指定デバイスの接続中 COM ポート番号（未接続/模擬モードは 0） */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	int32 GetDeviceComPort(EFishingWiredDeviceType DeviceType) const;

	/**
	 * @brief アームトラッカーの最新オイラー角を取得する。
	 * @return bValid=false の場合は未接続/未受信/古いデータ（AgeSeconds で判定可）
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	FArmEulerSample GetLatestArmEuler();

	/**
	 * @brief 自転車デバイスの最新 RPS/RPM/回転方向を取得する。
	 * @return bValid=false の場合は未接続/未受信/古いデータ（AgeSeconds で判定可）
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	FBicycleSample GetLatestBicycleSample();

	// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 自転車デコーダ（生状態 0..3 復号）の最新運動スナップショットを取得する。
	 * @return 実際の RPM・物理方向・測定フェーズ・正逆の完了周回数・接続代次を含む。
	 *         bValid=false は実機未接続/状態フレーム未受信/模擬モード（模擬は本経路に快照を書かない）。
	 *         方向の正反对調は bDirectionInverted（Project Settings 設定の回示）を使用側で適用する。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	FBicycleMotionSample GetLatestBicycleMotion();

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 自転車の完走周期イベント（実時刻・物理方向・周期の実効 RPM・接続代次）を
	 *        ワーカーのキューから全て取り出して空にする（ゲームスレッドから呼ぶ）。
	 * @note 受信順（時系列順）で返す。フェーズ切替・再接続・方向設定変更時の履歴破棄も
	 *       本メソッドで行う（取り出した配列を破棄すればよい）。未接続時は空配列。
	 */
	TArray<FBicycleCycleEventCore> DrainBicycleCycleEvents();

	// 2026.10.29 Lee(8) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 接続代次・復号快照・完走周期イベント批次を同一ロック内で取得する（ゲームスレッドから呼ぶ）。
	 * @return 取得後キューは空。未接続時は代次 0・空批次。消費基準と対象の対応を保証する
	 *         アトミックビュー（Bridge の再構築判定とイベント消費は本結果のみを根拠にする）。
	 */
	FBicycleDrainResult DrainBicycleState();
	// 2026.10.29 Lee(8) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/**
	 * @brief アームトラッカーのデバイス側キャリブレーション(0x20)送信を要求する。
	 * @note 返答の無いコマンドのため送信要求をキューに入れるのみ（ワーカースレッドで送信）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	void RequestDeviceCalibration();

	/** @brief 両デバイスの詳細状態をログへ出力する（コンソールコマンド Fishing.DeviceStatus から呼ぶ） */
	void LogStatusReport() const;

	/** @brief 接続状態変化イベント（状態が変わった直後にゲームスレッドで発火。デバイス種別ごと） */
	UPROPERTY(BlueprintAssignable, Category = "Fishing|WiredDevice")
	FOnWiredDeviceStateChanged OnStateChanged;

private:
	/** @brief ゲームスレッド定期処理（実機: サンプル複写+状態監視+画面デバッグ / 模擬: 模擬データ生成） */
	bool HandleTicker(float DeltaTime);

	/** @brief 実機 1 デバイス分のワーカースレッドを生成・起動する（Win64 のみ実体あり） */
	void StartDeviceWorker(EFishingWiredDeviceType DeviceType, int32 ComPortOverride, FDeviceWorkerSlot& Slot);

	/** @brief スロットのスレッド停止（Stop 要求 → 完了待ち → 破棄。Magic_Project UDeviceThreadManager と同手順） */
	void StopDeviceWorker(FDeviceWorkerSlot& Slot);

	/** @brief 種別に対応するスロットを取得する（Index 範囲は種別列挙で保証） */
	FDeviceWorkerSlot* FindSlot(EFishingWiredDeviceType DeviceType);
	const FDeviceWorkerSlot* FindSlot(EFishingWiredDeviceType DeviceType) const;

	/** @brief 種別 → スロット配列インデックス（[0]=アームトラッカー / [1]=自転車） */
	static int32 GetTypeIndex(EFishingWiredDeviceType DeviceType)
	{
		return DeviceType == EFishingWiredDeviceType::ArmTracker ? 0 : 1;
	}

	/** @brief アームサンプルを共有バッファへ格納する（mutex 保護） */
	void StoreArmSample(const FFishingDeviceSampleCore& Sample);

	/** @brief 自転車サンプルを共有バッファへ格納する（mutex 保護） */
	void StoreBicycleSample(const FFishingDeviceSampleCore& Sample);

	// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 自転車の運動復号スナップショットを共有バッファへ格納する（mutex 保護） */
	void StoreBicycleMotion(const FBicycleMotionSnapshotCore& Sample);

	/** @brief 自転車の運動復号スナップショットのコピーを取得（mutex 保護） */
	FBicycleMotionSnapshotCore CopyBicycleMotion() const;
	// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief アームサンプルのコピーを取得（mutex 保護） */
	FFishingDeviceSampleCore CopyArmSample() const;

	/** @brief 自転車サンプルのコピーを取得（mutex 保護） */
	FFishingDeviceSampleCore CopyBicycleSample() const;

	/** @brief 画面デバッグ（fishing.WiredDevice.DebugStatus）: 種別ごとの状態行を描画する */
	void DrawDebugStatus();

	/** @brief ログ/画面デバッグ共用の 1 デバイス分の状態行文字列を組む */
	FString BuildDeviceStatusLine(EFishingWiredDeviceType DeviceType) const;

	/** @brief 種別ごとに状態を照合し、変わっていれば OnStateChanged を発火する */
	void BroadcastStateIfChanged();

	// --- デバイススロット（[0]=アームトラッカー / [1]=自転車） ---
	UPROPERTY()
	FDeviceWorkerSlot DeviceSlots[2];

	// --- 状態管理 ---
	bool bConnectionActive = false;
	bool bSimulatorActive = false;
	double SimStartSec = 0.0;
	/** 前回発火時の状態（スロット配列と同じインデックス） */
	EFishingWiredDeviceState LastBroadcastStates[2] = {
		EFishingWiredDeviceState::Idle,
		EFishingWiredDeviceState::Idle
	};
	FTSTicker::FDelegateHandle TickerHandle;

	// --- 最新サンプル（ワーカー/模擬のどちらかが書き込む。デバイス種別ごとに分離） ---
	mutable FCriticalSection ArmSampleMutex;
	FFishingDeviceSampleCore LatestArmSample;
	mutable FCriticalSection BicycleSampleMutex;
	FFishingDeviceSampleCore LatestBicycleSample;

	// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// --- 自転車の運動復号スナップショット（実機ワーカーのみ書き込む。模擬モードは書かない） ---
	mutable FCriticalSection BicycleMotionMutex;
	FBicycleMotionSnapshotCore LatestBicycleMotion;
	// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
};
