// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/subsystem/FishingLoadSettingsSubsystem.h"
#include "VRFishingLog.h"

namespace
{
	// 上下運動のプリセット別 目標回数（Low / Medium / High）
	constexpr int32 VerticalCountTable[3] = { 3, 5, 8 };

	// リールのプリセット別 目標回転数（Low / Medium / High）
	constexpr int32 RotationCountTable[3] = { 5, 10, 15 };

	// 運動時間の下限（秒）
	constexpr float ExerciseTimeMinSeconds = 60.0f;

	// 運動時間の上限（秒）
	constexpr float ExerciseTimeMaxSeconds = 300.0f;

	// 運動時間の丸めステップ幅（秒）
	constexpr float ExerciseTimeStepSeconds = 30.0f;

	// 未設定時に UI 表示へ返す代替値（AFishingGameModeBase の C++ デフォルトに合わせる）
	constexpr float ExerciseTimeFallbackSeconds = 90.0f;
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
	// 未設定状態からの調整は代替値（90 秒）を基準にする
	const float BaseSeconds = HasExerciseTimeOverride() ? ExerciseTimeSecondsOverride : ExerciseTimeFallbackSeconds;
	ApplyExerciseTime(BaseSeconds + DeltaSeconds);
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
	return (VerticalTargetCountOverride > 0) ? VerticalTargetCountOverride
		: VerticalCountTable[PresetToIndex(VerticalPreset)];
}

int32 UFishingLoadSettingsSubsystem::GetRotationTargetCount() const
{
	return (RotationTargetCountOverride > 0) ? RotationTargetCountOverride
		: RotationCountTable[PresetToIndex(RotationPreset)];
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
	return HasExerciseTimeOverride() ? ExerciseTimeSecondsOverride : ExerciseTimeFallbackSeconds;
}

float UFishingLoadSettingsSubsystem::GetExerciseSliderValue() const
{
	return (GetExerciseTimeSeconds() - ExerciseTimeMinSeconds) / (ExerciseTimeMaxSeconds - ExerciseTimeMinSeconds);
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
	// ステップ幅に丸めてから範囲内へクランプする（60〜300 秒・30 秒刻み）
	const float RoundedSeconds = FMath::RoundToFloat(Seconds / ExerciseTimeStepSeconds) * ExerciseTimeStepSeconds;
	const float ClampedSeconds = FMath::Clamp(RoundedSeconds, ExerciseTimeMinSeconds, ExerciseTimeMaxSeconds);

	// 同じ値への再設定はログ・ブロードキャストを省略する（スライダー表示との相互反映による二重ログ防止）
	if (ExerciseTimeSecondsOverride > 0.0f && FMath::IsNearlyEqual(ExerciseTimeSecondsOverride, ClampedSeconds))
	{
		return;
	}

	ExerciseTimeSecondsOverride = ClampedSeconds;

	UE_LOG(LogFishing, Log, TEXT("[LoadSettings] 運動時間: %.0f 秒"), ExerciseTimeSecondsOverride);
	OnExerciseTimeChanged.Broadcast(ExerciseTimeSecondsOverride);
}
