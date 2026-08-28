// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Lee/device/FishingWiredDeviceTypes.h"
#include "FishingWiredDeviceSettings.generated.h"

/**
 * @brief 有線デバイス（ASerial/UART）連携の調整項目を Project Settings で設定するための DeveloperSettings。
 * @note 変更は DefaultGame.ini に保存され、再コンパイル不要。実行中の読み取りは都度 CDO を参照するため即時反映される。
 * @note 「Fishing Wired Device (有線デバイス設定)」カテゴリで表示される。
 *       デバイス実機が未完成の間は bUseSimulator=true（既定）で模擬データが流れる。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Fishing Wired Device (有線デバイス設定)"))
class VRFISHING_API UFishingWiredDeviceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ==================== 接続 ====================

	/** @brief GameInstance 起動時に自動で接続処理を開始するか（Win64 のみ動作。模擬モード時は模擬データ生成を開始） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "接続")
	bool bAutoConnectOnStartup = true;

	/** @brief 接続対象のデバイス種別（ASerial デバイスID 照合に使用） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "接続")
	EFishingWiredDeviceType DeviceType = EFishingWiredDeviceType::ArmTracker;

	/** @brief 接続先 COM ポート指定（0 = 自動探査: COM1〜255 を順に試す。ASerial 規格 6-5 の自動探査に相当） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "接続", meta = (ClampMin = "0", ClampMax = "255"))
	int32 ComPortOverride = 0;

	/** @brief データ取得（ポーリング）周期 [秒]。ASerial は応答型のため常時ポーリングが必要 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "接続", meta = (ClampMin = "0.002", ClampMax = "0.1"))
	float PollIntervalSeconds = 0.008f;

	/** @brief 接続失敗/切断後の再接続試行間隔 [秒] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "接続", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float ReconnectIntervalSeconds = 1.0f;

	/** @brief この秒数データが更新されなかったら古いデータとして無効扱いする [秒] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "接続", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float DataStaleTimeoutSeconds = 1.0f;

	// ==================== 模擬データ（実機未完成向け） ====================

	/** @brief 模擬データモード（シリアルポートを開かず模擬データを流す）。実機接続時は false へ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ConfigRestartRequired = "true"))
	bool bUseSimulator = true;

	/** @brief 模擬データ: 各往復の下端保持時間 [秒]。Ready 状態の RequiredWaitTime(既定 2 秒) + α に合わせる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ClampMin = "0.5"))
	float SimBottomHoldSeconds = 2.5f;

	/** @brief 模擬データ: 手の上下速度（HandHeightPercent/秒）。HandUpDown の RecommendedSpeed(既定 0.2) に一致させると高スコア検証ができる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float SimHandSpeedPercentPerSec = 0.2f;

	/** @brief 模擬データ: 1 サイクルの往復回数（HandUpDown の TargetUpAndDownCount 既定 5 回に対応） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ClampMin = "1"))
	int32 SimSweepCountPerCycle = 5;

	/** @brief 模擬データ: 上端での保持時間 [秒]（矢印の待機位相を想定） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ClampMin = "0.0"))
	float SimEndpointDwellSeconds = 0.8f;

	/** @brief 模擬データ: 自転車の基本 RPS（RPS×60 = RPM が判定帯内に収まる値。既定 0.8 → 48 RPM） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ClampMin = "0.0"))
	float SimRpsBase = 0.8f;

	/** @brief 模擬データ: 自転車 RPS の振幅（Base±Amplitude の範囲で変動） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ClampMin = "0.0"))
	float SimRpsAmplitude = 0.25f;

	/** @brief 模擬データ: 自転車 RPS 変動の周期 [秒] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "模擬データ", meta = (ClampMin = "1.0"))
	float SimRpsWavePeriodSeconds = 6.0f;

	// ==================== ASerial デバイス定数（仕様書値） ====================

	/** @brief アームトラッカーの DeviceID（仕様書: 0x02） */
	static constexpr uint8 ArmTrackerDeviceId = 0x02;

	/** @brief アームトラッカーの DeviceVer（仕様書: 0x01） */
	static constexpr uint8 ArmTrackerDeviceVer = 0x01;

	/** @brief 自転車デバイスの DeviceID（仕様書: 0x03） */
	static constexpr uint8 BicycleDeviceId = 0x03;

	/** @brief 自転車デバイスの DeviceVer（仕様書: 0x02） */
	static constexpr uint8 BicycleDeviceVer = 0x02;

	/** @brief アームトラッカー: オイラー角取得コマンド（仕様書: 0x21, 符号付き4Byte×3軸 X Y Z, 1000倍） */
	static constexpr uint8 ArmTrackerCmdGetEuler = 0x21;

	/** @brief アームトラッカー: クォータニオン取得コマンド（仕様書: 0x22, 符号付き4Byte×4成分 W X Y Z, 1000倍） */
	static constexpr uint8 ArmTrackerCmdGetQuat = 0x22;

	/** @brief アームトラッカー: キャリブレーションコマンド（仕様書: 0x20, 返答なし） */
	static constexpr uint8 ArmTrackerCmdCalibration = 0x20;

	/** @brief 自転車: RPM/RPS 更新フラグ取得コマンド（仕様書: 0x20, 符号なし1Byte） */
	static constexpr uint8 BicycleCmdGetUpdateFlag = 0x20;

	/** @brief 自転車: RPM 取得コマンド（仕様書: 0x21, 符号なし2Byte） */
	static constexpr uint8 BicycleCmdGetRpm = 0x21;

	/** @brief 自転車: RPS 取得コマンド（仕様書: 0x22, 符号付き4Byte, 100倍） */
	static constexpr uint8 BicycleCmdGetRps = 0x22;

	/** @brief 自転車: 回転方向取得コマンド（仕様書: 0x23, 符号なし1Byte, 1=正転/0=逆転） */
	static constexpr uint8 BicycleCmdGetDirection = 0x23;
};
