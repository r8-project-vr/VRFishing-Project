// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FishingLoadSettingsDeveloperSettings.generated.h"

/** @brief RPM 閾値 1 桁分（ホイール上限／スティック上限／下限）。要素順 = プリセット 低/中/高 */
USTRUCT(BlueprintType)
struct FRPMPresetThresholds
{
	GENERATED_BODY()

	/** マウスホイール操作時の上限 RPM（これを超えると速すぎミス） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|RPM", meta = (ClampMin = "0.0"))
	float WheelMaxAllowedRPM = 40.0f;

	/** スティック操作時の上限 RPM（これを超えると速すぎミス） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|RPM", meta = (ClampMin = "0.0"))
	float StickMaxAllowedRPM = 70.0f;

	/** 下限 RPM（これを下回ると遅すぎミス） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|RPM", meta = (ClampMin = "0.0"))
	float MinAllowedRPM = 30.0f;
};

/**
 * @brief 負荷設定の数値一式を Project Settings で調整するための DeveloperSettings。
 * @note 変更は DefaultGame.ini へ保存され、再コンパイル不要。実行中の読み取りは都度 CDO を参照するため即時反映される。
 * @note 本クラスが負荷パラメータの唯一の調整場所。上下/巻取の目標回数は UFishingLoadSettingsSubsystem が、
 *       RPM 閾値は UFishingLoadApplierComponent がそれぞれここから読んで各コンポーネントへ反映する
 *       （BP アセット側の既定値は実行時に上書きされるため、そちらを編集しても無効）。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Fishing Load Settings (釣り負荷設定)"))
class VRFISHING_API UFishingLoadSettingsDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ==================== 運動時間 ====================

	/** 運動時間の下限（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|運動時間", meta = (ClampMin = "10.0"))
	float ExerciseTimeMinSeconds = 60.0f;

	/** 運動時間の上限（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|運動時間", meta = (ClampMin = "10.0"))
	float ExerciseTimeMaxSeconds = 300.0f;

	/** 運動時間の丸めステップ幅（秒）。スライダー行の左右操作 1 回ぶんの間隔 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|運動時間", meta = (ClampMin = "5.0"))
	float ExerciseTimeStepSeconds = 30.0f;

	/** 未設定時の代替表示値（秒）。AFishingGameModeBase の TotalGameTime 既定と合わせること */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|運動時間", meta = (ClampMin = "10.0"))
	float ExerciseTimeFallbackSeconds = 90.0f;

	// ==================== プリセット表 ====================

	/** 上下運動の目標回数（要素順 = 低/中/高） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|プリセット")
	TArray<int32> VerticalCountTable = { 3, 5, 8 };

	/** リールの目標回転数（要素順 = 低/中/高） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|プリセット")
	TArray<int32> RotationCountTable = { 5, 10, 15 };

	/** RPM 閾値（要素順 = 低/中/高）。要素が足りない水位は ReelState 既定が維持される */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Config, Category = "Fishing|プリセット")
	TArray<FRPMPresetThresholds> RPMThresholdTable = {
		FRPMPresetThresholds{ 30.0f, 50.0f, 20.0f },
		FRPMPresetThresholds{ 40.0f, 70.0f, 30.0f },
		FRPMPresetThresholds{ 50.0f, 90.0f, 40.0f }
	};
};
