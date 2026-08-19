// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FishingLoadSettingsSubsystem.generated.h"

/**
 * @brief 負荷設定のプリセット（低/中/高）を表す列挙型。
 */
UENUM(BlueprintType)
enum class EFishingLoadPreset : uint8
{
	Low     UMETA(DisplayName = "低"),
	Medium  UMETA(DisplayName = "中"),
	High    UMETA(DisplayName = "高")
};

/** @brief 運動時間変更時に発火するデリゲートの型宣言 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExerciseTimeChanged, float, NewSeconds);

/**
 * @brief タイトル画面で選択された負荷設定を保持する GameInstance サブシステム。
 * @note レベル遷移（OpenLevel）をまたいで値を保持し、MainGame 側の UFishingLoadApplierComponent が適用する。
 * @note 上下運動・リールの目標回数は本サブシステムが唯一の正となる（Medium でも無条件で適用され、
 *       BP アセット側のデフォルト値は上書きされる）。運動時間のみ未設定時は GameMode アセット値を尊重する。
 */
UCLASS()
class VRFISHING_API UFishingLoadSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ==================== 選択 API（タイトル UI から呼ぶ） ====================

	/// @brief 上下運動の負荷プリセットを設定する
	UFUNCTION(BlueprintCallable, Category = "Fishing|Load")
	void SetVerticalLoad(EFishingLoadPreset Preset);

	/// @brief リール（巻き取り）の負荷プリセットを設定する
	UFUNCTION(BlueprintCallable, Category = "Fishing|Load")
	void SetRotationLoad(EFishingLoadPreset Preset);

	/// @brief 運動時間をステップ幅分だけ加減算する（メニューのスライダー行の左右操作用）。初回呼び出しで「設定済み」となる
	UFUNCTION(BlueprintCallable, Category = "Fishing|Load")
	void StepExerciseTime(float DeltaSeconds);

	/// @brief 運動時間（秒）を直接設定する（治療側カスタマイズ用）
	UFUNCTION(BlueprintCallable, Category = "Fishing|Load|Advanced")
	void SetExerciseTimeSecondsDirect(float Seconds);

	/// @brief 上下運動の目標回数を直接設定する（治療側カスタマイズ用）
	UFUNCTION(BlueprintCallable, Category = "Fishing|Load|Advanced")
	void SetVerticalTargetCountDirect(int32 Count);

	/// @brief リールの目標回転数を直接設定する（治療側カスタマイズ用）
	UFUNCTION(BlueprintCallable, Category = "Fishing|Load|Advanced")
	void SetRotationTargetCountDirect(int32 Count);

	// ==================== 参照 API（UI 表示・適用側用） ====================

	/// @brief 上下運動の目標回数（プリセット解決後の実効値）
	UFUNCTION(BlueprintPure, Category = "Fishing|Load")
	int32 GetVerticalTargetCount() const;

	/// @brief リールの目標回転数（プリセット解決後の実効値）
	UFUNCTION(BlueprintPure, Category = "Fishing|Load")
	int32 GetRotationTargetCount() const;

	/// @brief 選択中の上下運動プリセット（UI ハイライト用）
	UFUNCTION(BlueprintPure, Category = "Fishing|Load")
	EFishingLoadPreset GetVerticalPreset() const;

	/// @brief 選択中のリールプリセット（UI ハイライト用）
	UFUNCTION(BlueprintPure, Category = "Fishing|Load")
	EFishingLoadPreset GetRotationPreset() const;

	/// @brief 運動時間（秒）。未設定の場合は GameMode の C++ デフォルトと同じ 90 秒を返す
	UFUNCTION(BlueprintPure, Category = "Fishing|Load")
	float GetExerciseTimeSeconds() const;

	/// @brief 運動時間のスライダー値（0.0〜1.0）。スライダー初期位置の設定用
	UFUNCTION(BlueprintPure, Category = "Fishing|Load")
	float GetExerciseSliderValue() const;

	/// @brief 運動時間が設定済みか（未設定なら GameMode アセット値を上書きしない）
	UFUNCTION(BlueprintPure, Category = "Fishing|Load")
	bool HasExerciseTimeOverride() const;

	/// @brief 運動時間変更の通知（スライダー・テキストの表示更新用）
	UPROPERTY(BlueprintAssignable, Category = "Fishing|Load")
	FOnExerciseTimeChanged OnExerciseTimeChanged;

private:
	/** 上下運動の負荷プリセット */
	EFishingLoadPreset VerticalPreset = EFishingLoadPreset::Medium;

	/** リールの負荷プリセット */
	EFishingLoadPreset RotationPreset = EFishingLoadPreset::Medium;

	/** 上下運動の目標回数オーバーライド（0 = プリセット表を使用） */
	int32 VerticalTargetCountOverride = 0;

	/** リールの目標回転数オーバーライド（0 = プリセット表を使用） */
	int32 RotationTargetCountOverride = 0;

	/** 運動時間のオーバーライド（秒）。-1 = 未設定 */
	float ExerciseTimeSecondsOverride = -1.0f;

	/** @brief プリセットをプリセット表のインデックスへ変換する */
	static int32 PresetToIndex(EFishingLoadPreset Preset);

	/** @brief 運動時間をステップ幅に丸めて範囲内へクランプし、変更を通知する */
	void ApplyExerciseTime(float Seconds);
};
