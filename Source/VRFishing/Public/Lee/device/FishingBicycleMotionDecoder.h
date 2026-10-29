// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include <cmath>
#include <cstdint>

/**
 * @brief 自転車デコーダの現在フェーズ（スナップショット用の表示・判定分類）。
 * @note  UI 文言への対応は呼び出し側（Widget 等）が行う。RpmMonitor.cs の MotionTracker
 *        が返す状態文字列（正向/反向/停止/测量中）に相当する分類。
 */
enum class EBicycleMotionPhase : std::uint8_t
{
	/** 停止（周期を 1 つも完了していない、または停止タイムアウト超過）。RPM=0 */
	Stopped,

	/** 測定中（初回周期は完了済みだが平均間隔が未取得。次の周期まで待機）。RPM=0 */
	Measuring,

	/** 正方向回転中（状態環 0→1→3→2→0 の進行方向） */
	Forward,

	/** 逆方向回転中（状態環の逆向き） */
	Reverse
};

/**
 * @brief 1 周期（符号付き辺 4 個）完成イベント（Feed の時系列順に発行される純データ）。
 * @note  デコーダはスレッド非安全（単スレッド前提）のため、本イベント配列も Feed を呼ぶ
 *        スレッド内でのみ出し入れする。ゲームスレッドへの転送は呼び出し側（ワーカー）が行う。
 */
struct FBicycleCycleEvent
{
	/** 周期完成時刻（Feed に渡した単調時刻 [秒]） */
	double TimestampSec = 0.0;

	/** 周期の方向（+1=正方向 / -1=逆方向） */
	std::int32_t Dir = 0;

	/** 完成時点の平均周期間隔 [秒]（-1=測定中＝初回周期で速度未確定） */
	double AvgIntervalSec = -1.0;
};

/**
 * @brief FBicycleMotionDecoder の計算結果スナップショット（純データ構造体）。
 * @note  MakeSnapshot() が都度生成するもので、取得してもデコーダ内部状態は変わらない。
 */
struct FBicycleMotionDecoderSnapshot
{
	/** 推定 RPM（直近 3 周期の同方向間隔平均から換算。停止・測定中は 0） */
	double Rpm = 0.0;

	/** 現在のフェーズ（停止 / 測定中 / 正方向 / 逆方向） */
	EBicycleMotionPhase Phase = EBicycleMotionPhase::Stopped;

	/** 物理回転方向（+1=正方向 / -1=逆方向 / 0=停止・未確定） */
	std::int32_t PhysDir = 0;

	/** 符号付き辺の総和（正方向 +1 / 逆方向 -1。往復運動は相殺される。診断用） */
	std::int64_t NetEdges = 0;

	/** 正方向の完了周回数（符号付き辺 4 個で 1 周） */
	std::int64_t ForwardCycles = 0;

	/** 逆方向の完了周回数 */
	std::int64_t ReverseCycles = 0;

	/** 対角ジャンプ（0↔3 / 1↔2）不正遷移の検出回数（診断用） */
	std::int64_t InvalidTransitions = 0;

	/** 直近の生状態値 0..3（診断表示用。未受信時は -1） */
	std::int32_t Baseline = -1;

	/** 生状態を 1 つでも受信済みか（Baseline が有効か） */
	bool bHasBaseline = false;

	/** 速度窓内の周期間隔平均（秒。窓が空の場合は -1） */
	double AvgIntervalSec = -1.0;

	/** 最後の周期完了から NowSec までの経過秒（周期が未完了の場合は -1） */
	double SecondsSinceLastCycle = -1.0;

	/** 現在適用中の停止判定タイムアウト秒（GetStopTimeoutSeconds() と同一値） */
	double StopTimeoutSeconds = 0.0;
};

/**
 * @brief 自転車デバイスの生状態（0..3）から回転を復号する独立アルゴリズム（純 C++・UObject 非依存）。
 * @note  docs/自行车设备排查与RPM标定资料_2026-10-28/RPM工具/RpmMonitor.cs の
 *        MotionTracker と同一口径の移植版。判定規則:
 *        - 初回状態は基準（baseline）の確定にのみ使い、辺を捏造しない
 *        - 隣接状態への遷移のみを符号付き辺 ±1 として計上（0→1→3→2→0 が正方向）
 *        - 対角状態（0↔3 / 1↔2）は不正遷移: 基準の張り替えと未完成辺の破棄のみで計数しない
 *        - 符号付き辺 4 個で 1 周回。正逆の周回数と正味辺数を別々に累計する
 *        - 方向反転時は速度窓（直近 3 周期の間隔）をクリアする
 *        - RPM は直近 3 周期分の同方向間隔の平均から推定する
 *        - 停止タイムアウトは max(3秒, 2.5×平均間隔)、上限 15 秒
 *        - 停転後の再始動では停転間隔を速度窓へ混入させない
 *        スレッドセーフではないため排他は呼び出し側で保証すること
 *        （FFishingDeviceSampleCore と同一の契約）。時計には非依存で、
 *        Feed には単調増加する時刻（秒）を渡すこと。
 */
class FBicycleMotionDecoder
{
public:
	/** 停止判定の基礎秒 */
	static constexpr double StopBaseSeconds = 3.0;

	/** 直近平均間隔への適応係数（タイムアウト = 係数 × 平均間隔） */
	static constexpr double StopAdaptFactor = 2.5;

	/** 停止タイムアウトの上限秒 */
	static constexpr double StopMaxSeconds = 15.0;

	/** 1 周回に必要な符号付き辺数 */
	static constexpr std::int64_t EdgesPerCycle = 4;

	/** 速度窓に保持する周期間隔の最大サンプル数 */
	static constexpr std::int32_t MaxSpeedSamples = 3;

	/** @brief 状態フレーム値 State（0..3）を時刻 TimestampSec（秒）で取り込む。 */
	void Feed(double TimestampSec, std::int32_t State);

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 蓄積した完走周期イベントを取り出す（時系列順）。
	 *  @param OutEvents 受け先配列（OutCapacity 以上の要素数を要求）
	 *  @param OutCapacity 受け先の要素数
	 *  @return 取り出した件数。受け先が満杯で残った分は内部に保持される（次回の呼び出しで取出し）
	 *  @note 本クラスは UE コンテナに非依存（純 C++）とする設計のため C 配列で受け渡す。
	 *        Feed を呼ぶスレッド内でのみ使用すること。 */
	int DrainCycleEvents(FBicycleCycleEvent* OutEvents, int OutCapacity);

	/** 蓄積上限を超えて破棄した完走周期イベントの累計（診断用。原則 0 = 静かに捨てない） */
	std::int64_t DroppedCycleEventCount = 0;
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 現在時刻 NowSec（秒）での表示スナップショットを計算して返す（内部状態は不変）。 */
	FBicycleMotionDecoderSnapshot MakeSnapshot(double NowSec) const;

	/** @brief 停止判定タイムアウト秒（max(3秒, 2.5×平均間隔) を上限 15 秒へクランプ）。 */
	double GetStopTimeoutSeconds() const;

	/** @brief 速度窓内の周期間隔平均（秒。窓が空の場合は -1）。 */
	double GetAvgIntervalSeconds() const;

	// 2026.10.29 Lee(5) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 有効な平均周期間隔（厳密に正かつ有限）だけを返す（監視器 RpmMonitor.cs と同一口径）。
	 *  @return 有効な平均間隔 [秒]。無効（空窓 / 0 以下＝同時刻周期 / 非有限）は -1＝測定中扱い。
	 *  @note  完走周期イベント（AvgIntervalSec）とスナップショット（AvgIntervalSec / Rpm / Phase）
	 *         の双方が本関数経由で値を出すため、イベントとスナップショットは常に一致する。
	 *         無効値が無限大/NaN としてゲーム側へ流れることをここで遮断する
	 *        （FBicycleCycleEventCore::GetRpm 側でも二重防御する）。 */
	double GetValidAvgIntervalSeconds() const;
	// 2026.10.29 Lee(5) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 復号済みの累計を含む全内部状態を初期化する。 */
	void Reset();

private:
	/** @brief 符号付き 4 状態周期 1 周の完了処理（Dir は +1=正 / -1=逆）。 */
	void CompleteCycle(std::int32_t Dir, double TimestampSec);

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** 完走周期イベントの蓄積上限（ワーカーが Feed の都度 drain するため余裕を持つ定数） */
	static constexpr int MaxPendingEvents = 16;

	/** 完走周期イベントの蓄積先（固定長リング。Feed を呼ぶスレッドのみ触る） */
	FBicycleCycleEvent PendingEvents[MaxPendingEvents];

	/** 蓄積中のイベント件数 */
	int PendingEventCount = 0;

	/** リングの最古位置（満杯時に上書きする先） */
	int PendingEventOldestIndex = 0;
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 速度窓へ周期間隔を追加する（最大 3 サンプルのリング。満杯時は最古を上書き）。 */
	void AddInterval(double IntervalSec);

	/** @brief 速度窓を空にする。 */
	void ClearIntervalWindow();

	/** @brief 状態 State の正方向隣を返す（0→1→3→2→0）。 */
	static std::int32_t NextForwardState(std::int32_t State);

	// --- 復号状態 ---
	/** 直近の生状態値（未受信は -1。初回 Feed で確定する） */
	std::int32_t BaselineState = -1;

	/** 符号付き辺の総和（診断用） */
	std::int64_t NetEdges = 0;

	/** 対角ジャンプ不正遷移の検出回数（診断用） */
	std::int64_t InvalidTransitions = 0;

	/** 周期完成に向けた未完成の符号付き辺累計（±4 到達で 1 周として清算） */
	std::int64_t EdgesSinceCycle = 0;

	/** 最後に完成した周期の方向（+1=正 / -1=逆 / 0=未完了） */
	std::int32_t CycleDir = 0;

	/** 速度窓（直近の同方向周期間隔。秒。リングバッファ） */
	double IntervalWindow[MaxSpeedSamples] = { 0.0, 0.0, 0.0 };

	/** 速度窓内の有効サンプル数 */
	std::int32_t IntervalCount = 0;

	/** 速度窓リングの最古サンプル位置 */
	std::int32_t IntervalOldestIndex = 0;

	/** 最後の周期完了時刻（秒。未完了は -1） */
	double LastCycleTimeSec = -1.0;

	/** 1 周期でも完了したか（スナップショットの停止表示分岐用） */
	bool bHasCompletedCycle = false;

	/** 正方向の完了周回数 */
	std::int64_t ForwardCycles = 0;

	/** 逆方向の完了周回数 */
	std::int64_t ReverseCycles = 0;
};
