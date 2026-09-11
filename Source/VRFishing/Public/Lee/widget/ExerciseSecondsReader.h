// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UFishingStateComponentBase;

/**
 * @brief 運動ステートの残り運動時間（RemainingExerciseSeconds）の共通読み取りユーティリティ。
 * @note UFishingStateHandUpDown は public、UFishingReelStateComponent は protected のため
 *       （チーム規約により本人のコードへ getter を追加できない）、取得手段が両者で異なる。
 *       本ユーティリティへ一本化することで、表示側（ExerciseTimeBarWidget）は型の差を意識しない。
 */
namespace LeeFishingTime
{
	/**
	 * @brief ステートの残り運動時間（秒）を読み取る。
	 * @param State               読み取り元のステート（HandUpDown／Reel のみ対応。nullptr 可）
	 * @param OutRemainingSeconds 読み取り結果（秒）
	 * @return HandUpDown は public フィールドを直接読む（コンパイル時の改名検査が効く）。
	 *         Reel は protected のため FindFProperty による反射読み取り（ReelRPMThresholdReader と同一手法）。
	 *         反射に失敗した場合は警告を 1 回のみ出力する。
	 */
	bool ReadRemainingExerciseSeconds(const UFishingStateComponentBase* State, float& OutRemainingSeconds);
}
