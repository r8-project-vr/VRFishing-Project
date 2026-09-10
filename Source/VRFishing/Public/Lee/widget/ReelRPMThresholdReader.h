// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lee/component/HandHeightDetectorComponent.h"

class UFishingReelStateComponent;

/**
 * @brief リール RPM 判定閾値の共通読み取りユーティリティ。
 * @note FishFightMeterWidget と RpmGaugeWidget で判定表示の基準（閾値・デバイス解決・分類）を
 *       統一するため抽出した。閾値はリールステートの protected UPROPERTY のため、
 *       チーム規約により本人のコードへ getter を追加できずリフレクションで読み取る。
 *       上限は「判定が実際に使用した値」（ReelState が公開する出力）を最優先で参照する。
 */
namespace LeeReelRpm
{
	/**
	 * @brief リールステートの RPM 判定閾値を反射で読み取る。
	 * @param ReelState      読み取り元のリールステート
	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	 * @param OutMinRPM      遅すぎ閾値（スティック／ASerial 入力用）
	//* @param OutMinRPM      遅すぎ閾値（入力デバイス共通）
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	 * @param OutWheelMaxRPM 速すぎ閾値（マウスホイール入力用）
	 * @param OutStickMaxRPM 速すぎ閾値（スティック入力用）
	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	 * @param OutWheelMinRPM 遅すぎ閾値（マウスホイール入力用）
	 * @return 4 つの閾値をすべて取得できた場合 true
	//* @return 3 つの閾値をすべて取得できた場合 true
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	 */
	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// ホイール専用の下限が増えたため、出力に OutWheelMinRPM を追加する
	bool ReadReelRPMThresholds(const UFishingReelStateComponent* ReelState, float& OutMinRPM, float& OutWheelMaxRPM, float& OutStickMaxRPM, float& OutWheelMinRPM);
	//bool ReadReelRPMThresholds(const UFishingReelStateComponent* ReelState, float& OutMinRPM, float& OutWheelMaxRPM, float& OutStickMaxRPM);
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/**
	 * @brief 現在の入力デバイスに応じた速すぎ閾値を返す。
	 * @note HMD のステレオ描画が有効＝VR 起動中はスティック、非 VR 実行（デスクトップ PIE など）は
	 *       マウスホイールの閾値を使用する。HMD が接続していても VR が無効なデスクトップ実行では
	 *       StereoRenderingDevice は無効になるため、接続ではなく実行状態の判定として機能する。
	 */
	float ResolveMaxAllowedRPM(float WheelMaxRPM, float StickMaxRPM);

	/**
	 * @brief ゲーム判定（UFishingReelStateComponent::JudgeRPM）が実際に使用した速すぎ上限を返す。
	 * @param ReelState   読み取り元のリールステート（nullptr 可）
	 * @param WheelMaxRPM ホイール用上限（まだ回転入力が無い間の予測に使用）
	 * @param StickMaxRPM スティック用上限（まだ回転入力が無い間の予測に使用）
	 * @return 直近の入力で判定に渡された上限。未入力（0.0）の間のみ ResolveMaxAllowedRPM の予測値。
	 * @note 判定側が記録した値を最優先するため、入力デバイスの推定が外れる状況
	 *       （例：非 VR 実行＋自転車デバイス＝スティック扱い）でも、表示が判定と食い違わない。
	 *       ReelState 側の LastAppliedMaxAllowedRPM は public のため反射は不要。
	 */
	float ResolveJudgedMaxAllowedRPM(const UFishingReelStateComponent* ReelState, float WheelMaxRPM, float StickMaxRPM);

	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 現在の入力デバイスに応じた遅すぎ閾値を返す（ResolveMaxAllowedRPM の下限版）。
	 * @note 判定条件は ResolveMaxAllowedRPM と同一（VR 起動中＝スティック／非 VR 実行＝ホイール）。
	 */
	float ResolveMinAllowedRPM(float WheelMinRPM, float StickMinRPM);

	/**
	 * @brief ゲーム判定（UFishingReelStateComponent::JudgeRPM）が実際に使用した遅すぎ下限を返す。
	 * @param ReelState   読み取り元のリールステート（nullptr 可）
	 * @param StickMinRPM スティック／ASerial 用の下限（まだ回転入力が無い間の予測に使用）
	 * @param WheelMinRPM ホイール用の下限（まだ回転入力が無い間の予測に使用）
	 * @return 直近の入力で判定に渡された下限。未入力（0.0）の間のみ ResolveMinAllowedRPM の予測値。
	 * @note ホイールとスティックで下限が異なるため、表示・SE が本値を参照しないと判定と食い違う。
	 */
	float ResolveJudgedMinAllowedRPM(const UFishingReelStateComponent* ReelState, float StickMinRPM, float WheelMinRPM);
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/**
	 * @brief JudgeRPM と同一基準・同一優先順で RPM を 3 状態に分類する（判定表示の唯一の実装）。
	 * @param MinRPM 遅すぎ閾値（これ未満で TooSlow）
	 * @param MaxRPM 速すぎ閾値（これ超過で TooFast）
	 * @param RPM    分類対象の回転速度
	 * @note 上限超過を先に判定する優先順は JudgeRPM と同一（MinRPM > MaxRPM の誤設定時も判定と一致させる）。
	 *       通常は MaxRPM >= MinRPM を前提とする。
	 */
	EHandSpeedState ClassifyRPM(float MinRPM, float MaxRPM, float RPM);
}
