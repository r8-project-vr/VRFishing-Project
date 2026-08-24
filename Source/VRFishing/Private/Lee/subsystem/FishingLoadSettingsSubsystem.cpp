// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/subsystem/FishingLoadSettingsSubsystem.h"
#include "Lee/settings/FishingLoadSettingsDeveloperSettings.h"
#include "VRFishingLog.h"

namespace
{
	/** @brief 負荷パラメータの唯一の調整場所（Project Settings）から設定値を取得する */
	const UFishingLoadSettingsDeveloperSettings* GetLoadSettings()
	{
		return GetDefault<UFishingLoadSettingsDeveloperSettings>();
	}

	/**
	 * @brief プリセット表の要素を安全に読む。要素数が足りない場合は Medium 桁、それも無ければ旧既定値を返す
	 * @param Table  プリセット表（低/中/高 の順）
	 * @param Index  PresetToIndex の結果
	 * @param FallbackMedium  Medium 桁も欠けた場合の値（旧ハードコード値）
	 */
	int32 ReadCountTableValue(const TArray<int32>& Table, int32 Index, int32 FallbackMedium)
	{
		if (Table.IsValidIndex(Index))
		{
			return FMath::Max(1, Table[Index]);
		}
		if (Table.IsValidIndex(1))
		{
			return FMath::Max(1, Table[1]);
		}
		return FallbackMedium;
	}
}

void UFishingLoadSettingsSubsystem::SetVerticalLoad(EFishingLoadPreset Preset)
{
	VerticalPreset = Preset;
	VerticalTargetCountOverride = 0;

	UE_LOG(LogFishing, Log, TEXT("[LoadSettings] 上下運動プリセット: %s（目標 %d 回）"),
		*UEnum::GetDisplayValueAsText(Preset).ToString(), GetVerticalTargetCount());
}

void UFishingLoadSettingsSubsystem::SetRotationLoad(EFishingLoadPreset Preset)
{
	RotationPreset = Preset;
	RotationTargetCountOverride = 0;

	UE_LOG(LogFishing, Log, TEXT("[LoadSettings] リールプリセット: %s（目標 %d 回転）"),
		*UEnum::GetDisplayValueAsText(Preset).ToString(), GetRotationTargetCount());
}

void UFishingLoadSettingsSubsystem::StepExerciseTime(float DeltaSeconds)
{
	// 未設定状態からの調整は代替値（既定 90 秒）を基準にする
	const float BaseSeconds = HasExerciseTimeOverride()
		? ExerciseTimeSecondsOverride
		: GetLoadSettings()->ExerciseTimeFallbackSeconds;
	ApplyExerciseTime(BaseSeconds + DeltaSeconds);
}

void UFishingLoadSettingsSubsystem::SetExerciseTimeFromSliderValue(float SliderValue)
{
	// スライダー値を運動時間へ逆算する。換算式の情報源は Project Settings の下限/上限のみ。
	// （BP 側に同様の換算があると上限変更時に値が打架して往復ジャンプする）
	const UFishingLoadSettingsDeveloperSettings* Settings = GetLoadSettings();
	const float MinSeconds = Settings->ExerciseTimeMinSeconds;
	const float MaxSeconds = FMath::Max(MinSeconds, Settings->ExerciseTimeMaxSeconds);
	ApplyExerciseTime(MinSeconds + FMath::Clamp(SliderValue, 0.0f, 1.0f) * (MaxSeconds - MinSeconds));
}

void UFishingLoadSettingsSubsystem::SetExerciseTimeSecondsDirect(float Seconds)
{
	ApplyExerciseTime(Seconds);
}

void UFishingLoadSettingsSubsystem::SetVerticalTargetCountDirect(int32 Count)
{
	VerticalTargetCountOverride = FMath::Max(1, Count);

	UE_LOG(LogFishing, Log, TEXT("[LoadSettings] 上下運動の目標回数を直接設定: %d 回"), GetVerticalTargetCount());
}

void UFishingLoadSettingsSubsystem::SetRotationTargetCountDirect(int32 Count)
{
	RotationTargetCountOverride = FMath::Max(1, Count);

	UE_LOG(LogFishing, Log, TEXT("[LoadSettings] リールの目標回転数を直接設定: %d 回転"), GetRotationTargetCount());
}

int32 UFishingLoadSettingsSubsystem::GetVerticalTargetCount() const
{
	if (VerticalTargetCountOverride > 0)
	{
		return VerticalTargetCountOverride;
	}
	return ReadCountTableValue(GetLoadSettings()->VerticalCountTable, PresetToIndex(VerticalPreset), 5);
}

int32 UFishingLoadSettingsSubsystem::GetRotationTargetCount() const
{
	if (RotationTargetCountOverride > 0)
	{
		return RotationTargetCountOverride;
	}
	return ReadCountTableValue(GetLoadSettings()->RotationCountTable, PresetToIndex(RotationPreset), 10);
}

EFishingLoadPreset UFishingLoadSettingsSubsystem::GetVerticalPreset() const
{
	return VerticalPreset;
}

EFishingLoadPreset UFishingLoadSettingsSubsystem::GetRotationPreset() const
{
	return RotationPreset;
}

float UFishingLoadSettingsSubsystem::GetExerciseTimeSeconds() const
{
	return HasExerciseTimeOverride() ? ExerciseTimeSecondsOverride : GetLoadSettings()->ExerciseTimeFallbackSeconds;
}

float UFishingLoadSettingsSubsystem::GetExerciseSliderValue() const
{
	const float MinSeconds = GetLoadSettings()->ExerciseTimeMinSeconds;
	const float MaxSeconds = FMath::Max(MinSeconds, GetLoadSettings()->ExerciseTimeMaxSeconds);
	return (GetExerciseTimeSeconds() - MinSeconds) / (MaxSeconds - MinSeconds);
}

bool UFishingLoadSettingsSubsystem::HasExerciseTimeOverride() const
{
	return ExerciseTimeSecondsOverride > 0.0f;
}

int32 UFishingLoadSettingsSubsystem::PresetToIndex(EFishingLoadPreset Preset)
{
	switch (Preset)
	{
	case EFishingLoadPreset::Low:
		return 0;
	case EFishingLoadPreset::High:
		return 2;
	default:
		return 1;
	}
}

void UFishingLoadSettingsSubsystem::ApplyExerciseTime(float Seconds)
{
	// Project Settings のステップ幅に丸めてから範囲内へクランプする（既定 60〜300 秒・30 秒刻み）
	const UFishingLoadSettingsDeveloperSettings* Settings = GetLoadSettings();
	const float StepSeconds = FMath::Max(1.0f, Settings->ExerciseTimeStepSeconds);
	const float MinSeconds = Settings->ExerciseTimeMinSeconds;
	const float MaxSeconds = FMath::Max(MinSeconds, Settings->ExerciseTimeMaxSeconds);

	const float RoundedSeconds = FMath::RoundToFloat(Seconds / StepSeconds) * StepSeconds;
	const float ClampedSeconds = FMath::Clamp(RoundedSeconds, MinSeconds, MaxSeconds);

	// 同じ値への再設定はログ・ブロードキャストを省略する（スライダー表示との相互反映による二重ログ防止）
	if (ExerciseTimeSecondsOverride > 0.0f && FMath::IsNearlyEqual(ExerciseTimeSecondsOverride, ClampedSeconds))
	{
		return;
	}

	ExerciseTimeSecondsOverride = ClampedSeconds;

	UE_LOG(LogFishing, Log, TEXT("[LoadSettings] 運動時間: %.0f 秒"), ExerciseTimeSecondsOverride);
	OnExerciseTimeChanged.Broadcast(ExerciseTimeSecondsOverride);
}
