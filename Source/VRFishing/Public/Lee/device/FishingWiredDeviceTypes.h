// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lee/device/FishingBicycleMotionDecoder.h"
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
	// 2026.09.03 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** 回転速度 RPM（回転/分）。デバイスの 0x21 応答の生値（診断用の併取得値）。
	 *  旧仕様（消さずに残す）: RPS×60 で算出した値 */
	// 2026.09.03 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーー
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

	// 2026.09.03 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 回転速度 RPM。デバイスの 0x21 応答の生値（RPS 不推移の診断用併取得値）。
	 *  旧仕様（消さずに残す）: RPS×60 で算出 */
	// 2026.09.03 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float Rpm = 0.0f;

	/** @brief 回転方向（1=正転 / 0=逆転。取付方向に依存） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	uint8 Direction = 1;
};

// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
/**
 * @brief 完走周期イベント 1 件（ワーカー→ゲームスレッド転送用の純 C++ 構造体・UHT 非対象）。
 * @note デコーダ（FBicycleMotionDecoder）が 1 周期完成ごとに発行する記録に接続代次を添付したもの。
 *       順序保証あり（ワーカーが受信順に enqueue する）。ゲーム側は実時刻（DeviceTimeSec）を
 *       ゲーム時刻へ写像して使い、旧周期を現在時刻で扱わないこと。
 */
struct FBicycleCycleEventCore
{
	/** 周期完成時刻（FPlatformTime::Seconds() 軸・単調。ゲーム時刻軸とはオフセット変換して対応） */
	double DeviceTimeSec = 0.0;

	/** 周期の物理方向（+1=正方向 / -1=逆方向） */
	int32 Dir = 0;

	/** 完成時点の平均周期間隔 [秒]（-1=測定中＝初回周期で速度未確定） */
	double AvgIntervalSec = -1.0;

	/** 発行時の接続代次（現在代次と不一致のイベントは消費側で破棄する） */
	uint32 Generation = 0;

	/** @brief 測定中の初回周期か（true は速度判定・RPM 通知を行わない）。
	 *  @note  2026.10.29 Lee(5): 平均間隔が「厳密に正かつ有限」でない場合（-1＝測定中に加え、
	 *         0＝同一読み取り批次内の複数周期など）も測定中扱いにする。
	 *         無効値が無限大/NaN としてゲーム側へ流れることの二重防御。 */
	bool IsMeasuring() const
	{
		return !(AvgIntervalSec > 0.0 && FMath::IsFinite(AvgIntervalSec));
	}

	/** @brief 完成時点の実効 RPM（測定中は 0。無効間隔では決して無限大/NaN を返さない） */
	double GetRpm() const { return IsMeasuring() ? 0.0 : (60.0 / AvgIntervalSec); }
};

// 2026.10.29 Lee(8) startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
/**
 * ※ FBicycleDrainResult（アトミック取得結果）は値メンバで FBicycleMotionSnapshotCore の
 *   完全型を要求するため、依存順の関係で同構造体の定義後に定義している（本位置は不適格）。
 */
// 2026.10.29 Lee(8) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

// 2026.10.28 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
/**
 * @brief 自転車デコーダ（FBicycleMotionDecoder）の測定フェーズの BP 公開用列挙。
 * @note 数値はデコーダ側 EBicycleMotionPhase（Stopped=0 / Measuring=1 / Forward=2 / Reverse=3）
 *       と一致させてあり、static_cast で相互変換できる。並び順変更は禁止。
 */
UENUM(BlueprintType)
enum class EFishingBicycleMotionPhase : uint8
{
	/** 停止（周期未完了または停止タイムアウト超過）。RPM=0 */
	Stopped		UMETA(DisplayName = "停止"),
	/** 測定中（初回周期は完了済みだが平均間隔が未取得）。RPM=0 */
	Measuring	UMETA(DisplayName = "測定中"),
	/** 正方向回転中（状態環 0→1→3→2→0 の進行方向） */
	Forward		UMETA(DisplayName = "正方向"),
	/** 逆方向回転中（状態環の逆向き） */
	Reverse		UMETA(DisplayName = "逆方向")
};

/**
 * @brief 自転車の運動復号スナップショット（ワーカー→サブシステム間共有の純 C++ 構造体・UHT 非対象）。
 * @note ワーカースレッドが FBicycleMotionDecoder の復号結果から生成し、mutex 経由でゲームスレッドへ渡す。
 *       方向・フェーズはデコーダの生の物理値（取付方向基準）。UI 向けの正反对調は
 *       Project Settings「方向反転」設定を使用側で適用する（本構造体は変更しない）。
 */
struct FBicycleMotionSnapshotCore
{
	/** 有効な復号結果か（false = 接続後まだ状態フレームを 1 つも受信していない） */
	bool bValid = false;

	/** スナップショット生成時刻（FPlatformTime::Seconds() 値。Age 計算に使う） */
	double TimestampSec = 0.0;

	/** 推定 RPM（直近 3 周期の同方向間隔平均から換算。停止・測定中は 0） */
	double Rpm = 0.0;

	/** 物理回転方向（+1=正方向 / -1=逆方向 / 0=停止・未確定） */
	int32 PhysDir = 0;

	/** 測定フェーズ（デコーダの EBicycleMotionPhase 値。数値対応は EFishingBicycleMotionPhase と一致） */
	EBicycleMotionPhase Phase = EBicycleMotionPhase::Stopped;

	/** 正方向の完了周回数（符号付き辺 4 個で 1 周） */
	int64 ForwardCycles = 0;

	/** 逆方向の完了周回数 */
	int64 ReverseCycles = 0;

	/** 符号付き辺の総和（診断用） */
	int64 NetEdges = 0;

	/** 対角ジャンプ（0↔3 / 1↔2）不正遷移の検出回数（診断用） */
	int64 InvalidTransitions = 0;

	// 2026.10.28 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** 直近の完走圈から快照生成時刻までの経過秒（周期未完走は -1）。
	 *  @note 使用側は本値 + AgeSeconds で「直近の完走圈の裏付けの古さ」を判定する
	 *        （停止後に古い推定 RPM を使い回して回転を虚構しないための門控用） */
	double SecondsSinceLastCycle = -1.0;
	// 2026.10.28 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** 接続代次（再接続のたびに +1。旧接続のデータと新接続のデータを区別する） */
	uint32 Generation = 0;

	/** 状態フレーム（1 バイト 0..3）受信累計（診断用。静止中は増えないのが正常） */
	uint32 StateFrameCount = 0;

	/** 情報フレーム（4 バイト・心跳応答）受信累計（診断用。10 秒毎の心跳ごとに増える） */
	uint32 InfoFrameCount = 0;

	/** その他フレームの受信累計（診断用） */
	uint32 OtherFrameCount = 0;

	/** @brief 経過秒を計算して返す（無効時は実質無限大） */
	float GetAgeSeconds(double NowSec) const
	{
		return bValid ? static_cast<float>(NowSec - TimestampSec) : TNumericLimits<float>::Max();
	}
};

// 2026.10.29 Lee(8) startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
/**
 * @brief ワーカー状態のアトミック取得結果（同一ロック内の接続代次・快照・イベント批次）。
 * @note  Bridge が ticker 缓存の快照とイベントキューを別々に読むと「旧代次を読んだ直後に
 *        再接続 → 新代次イベントが入隊 → drain で取得 → 代次不符で全破棄」という競合で
 *        新阶段の首圈を失う。本構造体は 3 者を同一 DataMutex 取得分の一致ビューとして
 *        渡すことで、消費基準（代次）と対象（イベント）の対応を常に保証する。
 *        値メンバで FBicycleMotionSnapshotCore / FBicycleCycleEventCore の完全型を
 *        要求するため、両者の定義後に配置する（C3646/C4430 対応）。
 */
struct FBicycleDrainResult
{
	/** 取得時点の接続代次（消費フィルタの基準値） */
	uint32 Generation = 0;

	/** 取得時点の復号スナップショット（ワーカーの最新値） */
	FBicycleMotionSnapshotCore Snapshot;

	/** 蓄積していた完走周期イベント（取得後にキューは空になる。受信順＝時系列順） */
	TArray<FBicycleCycleEventCore> Events;
};
// 2026.10.29 Lee(8) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 2026.10.28 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

/**
 * @brief 自転車の運動復号スナップショット（BP 公開用。GameInstance サブシステム経由で取得）。
 * @note Rpm/PhysDir/Phase はデコーダの復号値（物理方向のまま）。bDirectionInverted は
 *       Project Settings「方向反転」設定の現在値の回示であり、使用側（Bridge/Widget）が
 *       正反对調を適用するかどうかの判断に使う。本構造体の値自体は反転しない。
 */
USTRUCT(BlueprintType)
struct FBicycleMotionSample
{
	GENERATED_BODY()

	/** @brief 有効な復号結果か（false = 未接続/未受信） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	bool bValid = false;

	/** @brief 受信からの経過秒（読み取り時点で計算） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	float AgeSeconds = 0.0f;

	/** @brief 推定 RPM（直近 3 周期の同方向間隔平均から換算。停止・測定中は 0） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	double Rpm = 0.0;

	/** @brief 物理回転方向（+1=正方向 / -1=逆方向 / 0=停止・未確定） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	int32 PhysDir = 0;

	/** @brief 測定フェーズ（停止 / 測定中 / 正方向 / 逆方向） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	EFishingBicycleMotionPhase Phase = EFishingBicycleMotionPhase::Stopped;

	/** @brief 正方向の完了周回数（符号付き辺 4 個で 1 周） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	int64 ForwardCycles = 0;

	/** @brief 逆方向の完了周回数 */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	int64 ReverseCycles = 0;

	/** @brief 符号付き辺の総和（診断用） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	int64 NetEdges = 0;

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 直近の完走圈から快照生成時刻までの経過秒（周期未完走は -1）。
	 *  @note 実際の経過は本値 + AgeSeconds（使用側で加算して判定する） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	double SecondsSinceLastCycle = -1.0;
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 接続代次（再接続のたびに +1。変化したら旧接続のデータ系統が無効になった印） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	int32 Generation = 0;

	/** @brief Project Settings「方向反転」設定の現在値（使用側で正反对調を適用する場合の判定用） */
	UPROPERTY(BlueprintReadOnly, Category = "Fishing|WiredDevice")
	bool bDirectionInverted = false;
};
