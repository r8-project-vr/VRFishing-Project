// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UFishingReelStateComponent;

/**
 * @brief リール RPM 判定閾値の共通読み取りユーティリティ。
 * @note FishFightMeterWidget と RpmGaugeWidget で判定表示の基準（閾値・デバイス解決）を
 *       統一するため抽出した。閾値はリールステートの protected UPROPERTY のため、
 *       チーム規約により本人のコードへ getter を追加できずリフレクションで読み取る。
 */
namespace LeeReelRpm
{
	/**
	 * @brief リールステートの RPM 判定閾値を反射で読み取る。
	 * @param ReelState      読み取り元のリールステート
	 * @param OutMinRPM      遅すぎ閾値（入力デバイス共通）
	 * @param OutWheelMaxRPM 速すぎ閾値（マウスホイール入力用）
	 * @param OutStickMaxRPM 速すぎ閾値（スティック入力用）
	 * @return 3 つの閾値をすべて取得できた場合 true
	 */
	bool ReadReelRPMThresholds(const UFishingReelStateComponent* ReelState, float& OutMinRPM, float& OutWheelMaxRPM, float& OutStickMaxRPM);

	/**
	 * @brief 現在の入力デバイスに応じた速すぎ閾値を返す。
	 * @note HMD のステレオ描画が有効＝VR 起動中はスティック、非 VR 実行（デスクトップ PIE など）は
	 *       マウスホイールの閾値を使用する。HMD が接続していても VR が無効なデスクトップ実行では
	 *       StereoRenderingDevice は無効になるため、接続ではなく実行状態の判定として機能する。
	 */
	float ResolveMaxAllowedRPM(float WheelMaxRPM, float StickMaxRPM);
}
