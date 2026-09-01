// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FishingWiredDeviceTypes.generated.h"

/**
 * @brief スレッド間共有用のデバイスサンプル（純 C++ 構造体・UHT 非対象）。
 * @note ワーカースレッドが生成し、mutex 経由でゲームスレッドへ渡す。両デバイスのデータを
 *       1 構造体にまとめてある（使用するメンバは DeviceType により異なる）。
 */
struct FFishingDeviceSampleCore
{
	/** 有効なデータか（false = 未受信） */
	bool bValid = false;

	/** 受信時刻（FPlatformTime::Seconds() 値。読み取り側で Age 計算に使う） */
	double TimestampSec = 0.0;

	// --- アームトラッカー（オイラー角・度） ---
	/** X軸角度（deg）= Roll */
	float AxisXDeg = 0.0f;
	/** Y軸角度（deg）= Pitch。上下運動判定の既定軸 */
	float AxisYDeg = 0.0f;
	/** Z軸角度（deg）= Yaw */
	float AxisZDeg = 0.0f;

	// --- 自転車デバイス ---
	/** 回転速度 RPS（回転/秒） */
	float Rps = 0.0f;
	/** 回転速度 RPM（回転/分）= RPS×60 */
	float Rpm = 0.0f;
	/** 回転方向（1=正転 / 0=逆転。取付方向に依存） */
	uint8 Direction = 1;

	/** @brief 経過秒を計算して返す（未受信時は実質無限大） */
	float GetAgeSeconds(double NowSec) const
	{
		return bValid ? static_cast<float>(NowSec - TimestampSec) : TNumericLimits<float>::Max();
	}
};

/**
 * @brief 有線デバイス（ASerial）の種別。
 * @note デバイスID/Ver は docs/デバイス関連/ の各仕様書に準拠。
 *       - アームトラッカー: DeviceID=0x02 / DeviceVer=0x01（オイラー角・クォータニオン取得）
 *       - 自転車デバイス  : DeviceID=0x03 / DeviceVer=0x02（RPM/RPS・回転方向取得）
 */
UENUM(BlueprintType)
enum class EFishingWiredDeviceType : uint8
{
	/** アームトラッカー（腕/脚の姿勢センサ）。上下運動・準備状態の入力源 */
	ArmTracker	UMETA(DisplayName = "アームトラッカー (0x02)"),

	/** 自転車デバイス（エルゴメーター外付）。巻取（Reel）状態の入力源 */
	Bicycle		UMETA(DisplayName = "自転車デバイス (0x03)")
};

/**
 * @brief 有線デバイスの接続状態。
 * @note Connecting=接続試行中 / Connected=通信確立 / Disconnected=未接続(再試行待ち) / Idle=未起動
 */
UENUM(BlueprintType)
enum class EFishingWiredDeviceState : uint8
{
	Idle			UMETA(DisplayName = "未起動"),
	Connecting		UMETA(DisplayName = "接続中(探査含む)"),
	Connected		UMETA(DisplayName = "接続済み"),
	Disconnected	UMETA(DisplayName = "切断")
};

/**
 * @brief アームトラッカーのオイラー角1サンプル（ASerial 0x21 応答の解析結果）。
 * @note 各軸は 1000 倍スケーリングの符号付き int32（ビッグエンディアン）を 1000 で除した「度」。
 *       軸の割り当ては Magic_Project 実装に準拠: X=Roll / Y=Pitch / Z=Yaw。
 */
USTRUCT(BlueprintType)
struct FArmEulerSample
{
	GENERATED_BODY()

	/** @brief 有効なデータか（false = 未接続/未受信） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	bool bValid = false;

	/** @brief 受信からの経過秒（読み取り時点で計算） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float AgeSeconds = 0.0f;

	/** @brief X軸角度（deg）= Roll */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float AxisXDeg = 0.0f;

	/** @brief Y軸角度（deg）= Pitch（既定の上下運動判定軸） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float AxisYDeg = 0.0f;

	/** @brief Z軸角度（deg）= Yaw */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float AxisZDeg = 0.0f;
};

/**
 * @brief 自転車デバイスの回転データ1サンプル（ASerial 0x22/0x23 応答の解析結果）。
 * @note RPS は 100 倍整数（例: 123 → 1.23）で受信するため 100 で除して格納。RPM は RPS×60 で算出。
 */
USTRUCT(BlueprintType)
struct FBicycleSample
{
	GENERATED_BODY()

	/** @brief 有効なデータか（false = 未接続/未受信） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	bool bValid = false;

	/** @brief 受信からの経過秒（読み取り時点で計算） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float AgeSeconds = 0.0f;

	/** @brief 回転速度 RPS（回転/秒） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float Rps = 0.0f;

	/** @brief 回転速度 RPM（回転/分）= RPS×60 */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float Rpm = 0.0f;

	/** @brief 回転方向（1=正転 / 0=逆転。取付方向に依存） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	uint8 Direction = 1;
};
