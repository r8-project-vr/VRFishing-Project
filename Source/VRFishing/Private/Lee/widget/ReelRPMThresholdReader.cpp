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
}
