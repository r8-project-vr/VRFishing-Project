// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/device/FishingBicycleMotionDecoder.h"

// ============================================================================
// 自転車の生状態（0..3）から回転を復号する独立デコーダの実装。
// 判定規則は docs の診断ツール RpmMonitor.cs（MotionTracker）と同一口径。
// ※ Doxygen 記法の関数説明はヘッダ側に記載済みのため、ここでは省略する。
// ============================================================================

void FBicycleMotionDecoder::Feed(double TimestampSec, std::int32_t State)
{
	// 防御: 状態値は 0..3 のみ（範囲外のフレームは無視）
	if (State < 0 || State > 3)
	{
		return;
	}

	// 2026.10.29 Lee(5): 非有限な時刻は異常入力として無視する。
	// 内部時刻（LastCycleTimeSec 等）の汚染と、以降の間隔計算の NaN 化を防ぐ
	// （単調時刻の契約は呼び出し側にあるが、無効値での崩壊は許容しない）
	if (!std::isfinite(TimestampSec))
	{
		return;
	}

	// 初回状態は基準（baseline）の確定のみに使い、辺を捏造しない
	if (BaselineState < 0)
	{
		BaselineState = State;
		return;
	}

	// 同一状態の繰り返しは辺にならない（静止時の重複パケットを計上しない）
	if (State == BaselineState)
	{
		return;
	}

	// 基準から見た正方向隣 / 対角 / 逆方向隣を状態環から求める
	const std::int32_t FwdNext = NextForwardState(BaselineState);
	const std::int32_t Diagonal = NextForwardState(FwdNext);
	const std::int32_t BwdNext = NextForwardState(Diagonal);

	// 対角（2 ステップ飛び）は不正遷移: 基準だけ張り替えて再同期し、
	// 未完成の辺累計も破棄する（辺・周回としては計数しない）
	if (State == Diagonal)
	{
		++InvalidTransitions;
		BaselineState = State;
		EdgesSinceCycle = 0;
		return;
	}

	// 正方向隣への遷移を +1、逆方向隣への遷移を -1 とする
	std::int32_t Dir = 0;
	if (State == FwdNext)
	{
		Dir = +1;
	}
	else if (State == BwdNext)
	{
		Dir = -1;
	}

	// 到達不能だが防御（0..3 の全遷移は上記 3 分岐で網羅済み）
	if (Dir == 0)
	{
		return;
	}

	// 符号付き辺として計上（往復運動は正負の辺が相殺される）
	NetEdges += Dir;
	BaselineState = State;
	EdgesSinceCycle += Dir;

	// 符号付き辺 4 個で 1 周回完了（逆方向は -4 到達で清算）
	if (EdgesSinceCycle >= EdgesPerCycle)
	{
		EdgesSinceCycle -= EdgesPerCycle;
		CompleteCycle(+1, TimestampSec);
	}
	else if (EdgesSinceCycle <= -EdgesPerCycle)
	{
		EdgesSinceCycle += EdgesPerCycle;
		CompleteCycle(-1, TimestampSec);
	}
}

void FBicycleMotionDecoder::CompleteCycle(std::int32_t Dir, double TimestampSec)
{
	if (CycleDir != Dir)
	{
		// 方向が変わった: 旧方向の速度窓をクリア（反転直後は再び測定中へ戻る）
		ClearIntervalWindow();
		CycleDir = Dir;
	}
	else if (LastCycleTimeSec >= 0.0)
	{
		const double Gap = TimestampSec - LastCycleTimeSec;

		// 停転後の再始動検出: 速度サンプルが残っているときだけ停止判定する。
		// 空窓で基礎 3 秒を適用すると、慢踏み（周期 > 3 秒）の初回周期まで
		// 誤廃棄されるため、サンプル無しのときは無条件に採用する。
		if (IntervalCount > 0 && Gap > GetStopTimeoutSeconds())
		{
			// 停転間隔を速度窓へ入れない（再始動後の虚低 RPM を防止）。
			// 旧窓を破棄して再測定へ。累計辺・周回数は保持する。
			ClearIntervalWindow();
		}
		else
		{
			AddInterval(Gap);
		}
	}

	LastCycleTimeSec = TimestampSec;
	bHasCompletedCycle = true;
	if (Dir > 0)
	{
		++ForwardCycles;
	}
	else
	{
		++ReverseCycles;
	}

	// 2026.10.29 Lee: 完走周期イベントを発行する。
	// 測定中（窓が空＝初回周期・方向反転直後・停転再始動直後）に加え、平均間隔が
	// 厳密に正かつ有限でない場合（2026.10.29 Lee(5): 同一時刻の複数周期で間隔 0 が
	// 窓へ入るケース等）も -1（測定中扱い）とする。監視器と同一の有効口径。
	// 消費側はこのイベントを「実時刻・実方向つきの完走記録」として扱う
	if (PendingEventCount >= MaxPendingEvents)
	{
		// 満杯（原則起こらない: ワーカーが Feed の都度 drain する）。最古を上書きして計数
		PendingEvents[PendingEventOldestIndex] = FBicycleCycleEvent();
		PendingEventOldestIndex = (PendingEventOldestIndex + 1) % MaxPendingEvents;
		--PendingEventCount;
		++DroppedCycleEventCount;
	}
	const int WriteIndex = (PendingEventOldestIndex + PendingEventCount) % MaxPendingEvents;
	PendingEvents[WriteIndex].TimestampSec = TimestampSec;
	PendingEvents[WriteIndex].Dir = Dir;
	PendingEvents[WriteIndex].AvgIntervalSec = GetValidAvgIntervalSeconds();
	++PendingEventCount;
}

// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
int FBicycleMotionDecoder::DrainCycleEvents(FBicycleCycleEvent* OutEvents, int OutCapacity)
{
	int Copied = 0;
	while (PendingEventCount > 0 && Copied < OutCapacity)
	{
		OutEvents[Copied] = PendingEvents[PendingEventOldestIndex];
		PendingEventOldestIndex = (PendingEventOldestIndex + 1) % MaxPendingEvents;
		--PendingEventCount;
		++Copied;
	}
	return Copied;
}
// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

void FBicycleMotionDecoder::AddInterval(double IntervalSec)
{
	if (IntervalCount < MaxSpeedSamples)
	{
		// 空きスロットへ末尾追加
		IntervalWindow[(IntervalOldestIndex + IntervalCount) % MaxSpeedSamples] = IntervalSec;
		++IntervalCount;
	}
	else
	{
		// 満杯: 最古のサンプルを上書きして窓をスライド（常に直近 3 周期を保持）
		IntervalWindow[IntervalOldestIndex] = IntervalSec;
		IntervalOldestIndex = (IntervalOldestIndex + 1) % MaxSpeedSamples;
	}
}

void FBicycleMotionDecoder::ClearIntervalWindow()
{
	IntervalCount = 0;
	IntervalOldestIndex = 0;
}

std::int32_t FBicycleMotionDecoder::NextForwardState(std::int32_t State)
{
	// 正方向の状態環（RpmMonitor.cs の FwdRing と同一: 0→1→3→2→0）
	static constexpr std::int32_t FwdRing[4] = { 0, 1, 3, 2 };

	std::int32_t Idx = 0;
	for (std::int32_t i = 0; i < 4; ++i)
	{
		if (FwdRing[i] == State)
		{
			Idx = i;
			break;
		}
	}
	return FwdRing[(Idx + 1) % 4];
}

double FBicycleMotionDecoder::GetAvgIntervalSeconds() const
{
	if (IntervalCount <= 0)
	{
		return -1.0;
	}

	// リングの最古から新しい順に足し合わせて平均を取る
	double Sum = 0.0;
	for (std::int32_t i = 0; i < IntervalCount; ++i)
	{
		Sum += IntervalWindow[(IntervalOldestIndex + i) % MaxSpeedSamples];
	}
	return Sum / IntervalCount;
}

double FBicycleMotionDecoder::GetValidAvgIntervalSeconds() const
{
	const double Avg = GetAvgIntervalSeconds();
	// 厳密に正かつ有限のみ有効（0＝同時刻周期の混入、非有限＝異常入力を測定中扱いへ落とす）
	return (Avg > 0.0 && std::isfinite(Avg)) ? Avg : -1.0;
}

double FBicycleMotionDecoder::GetStopTimeoutSeconds() const
{
	// 2026.10.29 Lee(5): 平均間隔は有効口径（>0 かつ有限）のみ使用する。
	// 無効（窓空き / 同時刻周期 / 非有限）では基礎秒へ落ち、NaN の伝播を防ぐ
	const double Avg = GetValidAvgIntervalSeconds();
	if (Avg <= 0.0)
	{
		return StopBaseSeconds;
	}

	// 適応タイムアウト = 2.5 × 平均周期間隔 を [3秒, 15秒] へクランプ
	double Timeout = StopAdaptFactor * Avg;
	Timeout = (Timeout < StopBaseSeconds) ? StopBaseSeconds : Timeout;
	Timeout = (Timeout > StopMaxSeconds) ? StopMaxSeconds : Timeout;
	return Timeout;
}

FBicycleMotionDecoderSnapshot FBicycleMotionDecoder::MakeSnapshot(double NowSec) const
{
	FBicycleMotionDecoderSnapshot Snap;
	Snap.NetEdges = NetEdges;
	Snap.ForwardCycles = ForwardCycles;
	Snap.ReverseCycles = ReverseCycles;
	Snap.InvalidTransitions = InvalidTransitions;
	Snap.Baseline = BaselineState;
	Snap.bHasBaseline = (BaselineState >= 0);
	// 2026.10.29 Lee(5): スナップショットも有効口径で統一（イベントと常に一致する）。
	// 無効（空窓 / 同時刻周期 / 非有限）は -1 となり、Rpm/Phase は測定中（RPM 0）へ落ちる
	Snap.AvgIntervalSec = GetValidAvgIntervalSeconds();
	Snap.StopTimeoutSeconds = GetStopTimeoutSeconds();
	Snap.Rpm = 0.0;
	Snap.PhysDir = 0;
	Snap.Phase = EBicycleMotionPhase::Stopped;

	if (!bHasCompletedCycle)
	{
		return Snap; // 周期なし: 停止表示のまま
	}

	Snap.SecondsSinceLastCycle = NowSec - LastCycleTimeSec;
	if (Snap.SecondsSinceLastCycle > GetStopTimeoutSeconds())
	{
		return Snap; // 停止タイムアウト: RPM 0 と「停止」を返す
	}

	if (Snap.AvgIntervalSec > 0.0)
	{
		// 直近 3 周期の平均間隔から RPM を推定（周期/秒 → 回/分）
		Snap.Rpm = 60.0 / Snap.AvgIntervalSec;
		Snap.PhysDir = CycleDir;
		Snap.Phase = (CycleDir > 0) ? EBicycleMotionPhase::Forward : EBicycleMotionPhase::Reverse;
	}
	else
	{
		// 最初の周期には前周期が無いため、第 2 周期まで測定中で待機
		Snap.PhysDir = CycleDir;
		Snap.Phase = EBicycleMotionPhase::Measuring;
	}
	return Snap;
}

void FBicycleMotionDecoder::Reset()
{
	BaselineState = -1;
	NetEdges = 0;
	InvalidTransitions = 0;
	EdgesSinceCycle = 0;
	CycleDir = 0;
	ClearIntervalWindow();
	LastCycleTimeSec = -1.0;
	bHasCompletedCycle = false;
	ForwardCycles = 0;
	ReverseCycles = 0;
	// 2026.10.29 Lee: 未転送の完走周期イベントも破棄する（リセット後の履歴混入を防ぐ）
	PendingEventCount = 0;
	PendingEventOldestIndex = 0;
}
