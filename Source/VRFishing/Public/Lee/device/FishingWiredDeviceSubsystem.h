// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Lee/device/FishingWiredDeviceTypes.h"
#include "FishingWiredDeviceSubsystem.generated.h"

class UASerialLibControllerWin;
class FFishingSerialWorker;

/**
 * @brief 有線デバイス（ASerial/UART）の接続ライフサイクルと最新データ配信を担う GameInstance サブシステム。
 * @note 役割:
 *       - Win64 実機モード: ASerialCom コントローラ + バックグラウンドスレッド（FFishingSerialWorker）で
 *         COM 自動探査 → デバイスID照合 → 応答型ポーリング。
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
	/** @brief 接続状態変化通知（true=接続済み / ComPort=接続先ポート番号、模擬モードでは 0） */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWiredDeviceStateChanged, bool, bConnected, int32, ComPort);

	// --- USubsystem インターフェース ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * @brief 接続処理を開始する（実機: スレッド起動+自動探査 / 模擬: 模擬データ生成開始）。
	 * @note 二重呼び出しは無視される。Win64 以外で実機モードの場合は模擬モードへフォールバックする。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	void StartConnection();

	/** @brief 接続を停止し、スレッド/模擬生成を終了する */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	void StopConnection();

	/** @brief 接続済みか（模擬モードの接続中も true） */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	bool IsConnected() const;

	/** @brief 模擬データモードで動作中か */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	bool IsSimulatorMode() const;

	/** @brief 現在の接続状態 */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	EFishingWiredDeviceState GetDeviceState() const;

	/** @brief 接続中の COM ポート番号（未接続/模擬モードは 0） */
	UFUNCTION(BlueprintPure, Category = "Fishing|WiredDevice")
	int32 GetConnectedComPort() const;

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

	/**
	 * @brief アームトラッカーのデバイス側キャリブレーション(0x20)送信を要求する。
	 * @note 返答の無いコマンドのため送信要求をキューに入れるのみ（ワーカースレッドで送信）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice")
	void RequestDeviceCalibration();

	/** @brief 接続状態変化イベント（状態が変わった直後にゲームスレッドで発火） */
	UPROPERTY(BlueprintAssignable, Category = "Fishing|WiredDevice")
	FOnWiredDeviceStateChanged OnStateChanged;

private:
	/** @brief ゲームスレッド定期処理（実機: サンプル複写+状態監視 / 模擬: 模擬データ生成） */
	bool HandleTicker(float DeltaTime);

	/** @brief 共有サンプルを更新する（mutex 保護） */
	void StoreSample(const FFishingDeviceSampleCore& Sample);

	/** @brief 共有サンプルのコピーを取得（mutex 保護） */
	FFishingDeviceSampleCore CopySample() const;

	/** @brief 状態が前回発火時と変わっていれば OnStateChanged を発火する */
	void BroadcastStateIfChanged();

	// --- ASerial 関連（Win64 実機モードのみ使用） ---
	/** ASerial コントローラ（ゲームスレッドで生成し GC から保護。ワーカーからはメソッド呼び出しのみ） */
	UPROPERTY()
	TObjectPtr<UASerialLibControllerWin> Controller;

	/** シリアルワーカースレッド（所有者は本クラス） */
	FFishingSerialWorker* Worker = nullptr;
	FRunnableThread* WorkerThread = nullptr;

	// --- 状態管理 ---
	bool bConnectionActive = false;
	bool bSimulatorActive = false;
	double SimStartSec = 0.0;
	EFishingWiredDeviceState LastBroadcastState = EFishingWiredDeviceState::Idle;
	FTSTicker::FDelegateHandle TickerHandle;

	// --- 最新サンプル（ワーカー/模擬のどちらかが書き込む） ---
	mutable FCriticalSection SampleMutex;
	FFishingDeviceSampleCore LatestSample;
};
