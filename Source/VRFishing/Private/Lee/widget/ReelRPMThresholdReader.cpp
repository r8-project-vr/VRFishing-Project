// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/ReelRPMThresholdReader.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"

namespace
{
	/** ReelState の閾値が読み取れなかった旨の警告を出力済みか（多重度の警告を防ぐ） */
	bool bReelRPMThresholdWarned = false;
}

namespace LeeReelRpm
{
	bool ReadReelRPMThresholds(const UFishingReelStateComponent* ReelState, float& OutMinRPM, float& OutWheelMaxRPM, float& OutStickMaxRPM)
	{
		if (!ReelState)
		{
			return false;
		}

		const UClass* ReelClass = ReelState->GetClass();
		const FFloatProperty* MinProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("MinAllowedRPM"));
		const FFloatProperty* WheelProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("WheelMaxAllowedRPM"));
		const FFloatProperty* StickProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("StickMaxAllowedRPM"));
		if (!MinProp || !WheelProp || !StickProp)
		{
			if (!bReelRPMThresholdWarned)
			{
				bReelRPMThresholdWarned = true;
				UE_LOG(LogTemp, Warning, TEXT("[FightMeter] ReelState の RPM 閾値が読み取れないため、デザイナー設定値で判定表示します"));
			}
			return false;
		}

		OutMinRPM = MinProp->GetPropertyValue_InContainer(ReelState);
		OutWheelMaxRPM = WheelProp->GetPropertyValue_InContainer(ReelState);
		OutStickMaxRPM = StickProp->GetPropertyValue_InContainer(ReelState);
		return true;
	}

	float ResolveMaxAllowedRPM(float WheelMaxRPM, float StickMaxRPM)
	{
		return (GEngine && GEngine->StereoRenderingDevice.IsValid()) ? StickMaxRPM : WheelMaxRPM;
	}

	float ResolveJudgedMaxAllowedRPM(const UFishingReelStateComponent* ReelState, float WheelMaxRPM, float StickMaxRPM)
	{
		// 判定側が記録した「実際に使った上限」を最優先で採用する（0.0＝まだ回転入力が無い）。
		// public プロパティのため反射は不要（const ポインタでも直接読み取り可能）。
		if (ReelState && ReelState->LastAppliedMaxAllowedRPM > 0.0f)
		{
			return ReelState->LastAppliedMaxAllowedRPM;
		}

		// 未入力の間だけ従来どおりデバイス予測（VR 起動中＝スティック／非 VR＝ホイール）へフォールバック
		return ResolveMaxAllowedRPM(WheelMaxRPM, StickMaxRPM);
	}

	EHandSpeedState ClassifyRPM(float MinRPM, float MaxRPM, float RPM)
	{
		// JudgeRPM と同じ優先順（上限超過を先に判定する）。誤設定で Min > Max のときも判定と一致させるため
		if (RPM > MaxRPM)
		{
			return EHandSpeedState::TooFast;
		}
		if (RPM < MinRPM)
		{
			return EHandSpeedState::TooSlow;
		}
		return EHandSpeedState::Good;
	}
}
