// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "FishingAudioSettings.generated.h"

class USoundBase;

/**
 * @brief ゲーム内効果音（SE）の音源を Project Settings で割り当てるための DeveloperSettings。
 * @note 変更は DefaultGame.ini へ保存され、再コンパイル不要。実行中の読み取りは CDO を参照するため即時反映される。
 * @note 音源未設定（None）のスロットは完全な無音（エラー・警告なし）。素材は後から差し込む前提の設計。
 * @note 再生の検出・管理はすべて UFishingSeControllerSubsystem（World 層）が担当し、本クラスは音源と音量の保持のみを行う。
 *
 * ### 効果音を 1 つ追加するときの手順（3 ステップ固定）:
 *  1. 本クラスに音源スロット（TSoftObjectPtr<USoundBase>）を 1 行追加する
 *  2. EFishingUiSfx（UI 音の場合）に列挙値を 1 つ追加する
 *  3. UFishingSeControllerSubsystem の switch に case を 1 つ追加する
 *  既存の検出ロジックには一切触れない。
 *
 * ### BGM について（2026.09.10 Lee、二期実装用の設計メモ）:
 *  BGM はレベル遷移をまたいで再生し続ける必要があるため、GameInstance 層の専用チャンネルとして
 *  二期で追加する（タイトル曲 → 本編 → リザルト曲の連続再生）。本クラスは SE 専用とし、
 *  BGM スロットはここに混在させない。二期実装時も本クラス／SE 制御サブシステムの変更は不要。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Fishing Audio Settings (釣りオーディオ設定)"))
class VRFISHING_API UFishingAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ==================== 魚の効果音（3D・空間化。ワールド内再生） ====================

	/** @brief 暴れ（Struggling）中の水音。ループ再生するため SoundWave 側の Looping 推奨（未設定でも側で連続再生する） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|魚")
	TSoftObjectPtr<USoundBase> StruggleLoopSfx = nullptr;

	/** @brief 咬みつき（Poking）中の水中音。魚のつつき動作に同期できないため、Poking 中は一定間隔で繰り返し再生する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|魚")
	TSoftObjectPtr<USoundBase> PokeSfx = nullptr;

	/** @brief 逃走（Escape）音。魚の位置から再生し、遠ざかるにつれ減衰する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|魚")
	TSoftObjectPtr<USoundBase> EscapeSfx = nullptr;

	/** @brief 釣り上げ（Caught）音 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|魚")
	TSoftObjectPtr<USoundBase> CaughtSfx = nullptr;

	// ==================== UI 効果音（2D・非空間化） ====================

	/** @brief メニューのカーソル移動音（フォーカス移動のたび） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|UI")
	TSoftObjectPtr<USoundBase> MenuCursorMoveSfx = nullptr;

	/** @brief メニューの決定音（A ボタン・レーザークリック共通） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|UI")
	TSoftObjectPtr<USoundBase> MenuConfirmSfx = nullptr;

	/** @brief フェーズ切替音（よーい→うで→リール→つりあげ→リザルト の各遷移） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|UI")
	TSoftObjectPtr<USoundBase> PhaseChangeSfx = nullptr;

	/** @brief フェーズ成功音（上下・リール・つりあげの各ステート成功時） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|UI")
	TSoftObjectPtr<USoundBase> PhaseSuccessSfx = nullptr;

	/** @brief フェーズ失敗音（過速・過遅・停止タイムアウトなどのミス時） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|UI")
	TSoftObjectPtr<USoundBase> PhaseFailSfx = nullptr;

	/** @brief リザルト表示音（ResultState 完了＝リザルト画面が出るタイミング） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|UI")
	TSoftObjectPtr<USoundBase> ResultAppearSfx = nullptr;

	/** @brief RPM 判定変化音（遅すぎ／適正／速すぎの判定が切り替わった瞬間のみ） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|効果音|UI")
	TSoftObjectPtr<USoundBase> RpmJudgeChangeSfx = nullptr;

	// ==================== 音量 ====================

	/** @brief 魚の効果音（3D）全体の音量倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|音量", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WorldSfxVolume = 1.0f;

	/** @brief UI 効果音（2D）全体の音量倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|音量", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UiSfxVolume = 1.0f;
};
