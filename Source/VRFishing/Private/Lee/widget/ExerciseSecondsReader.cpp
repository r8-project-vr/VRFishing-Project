// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/ExerciseSecondsReader.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "UObject/UnrealType.h"
#include "VRFishingLog.h"

namespace
{
	/** 読み取り失敗の警告を出力済みか（多重度の警告を防ぐ。ReelRPMThresholdReader と同一方式） */
	bool bExerciseSecondsWarned = false;
}

namespace LeeFishingTime
{
	bool ReadRemainingExerciseSeconds(const UFishingStateComponentBase* State, float& OutRemainingSeconds)
	{
		if (!State)
		{
			return false;
		}

		// 手上下：public フィールドのため直読みする
		if (const UFishingStateHandUpDown* Hand = Cast<UFishingStateHandUpDown>(State))
		{
			OutRemainingSeconds = Hand->RemainingExerciseSeconds;
			return true;
		}

		// リール：protected フィールドのため反射で読む（GetClass() は動的クラスなので
		// BP 派生クラスでも継承プロパティが見つかる）
		if (Cast<UFishingReelStateComponent>(State))
		{
			if (const FFloatProperty* Prop = FindFProperty<FFloatProperty>(State->GetClass(), TEXT("RemainingExerciseSeconds")))
			{
				OutRemainingSeconds = Prop->GetPropertyValue_InContainer(State);
				return true;
			}

			if (!bExerciseSecondsWarned)
			{
				bExerciseSecondsWarned = true;
				UE_LOG(LogFishing, Warning, TEXT("[ExerciseBar] ReelState の残り運動時間が読み取れません（RemainingExerciseSeconds の改名？）。進捗は前回値を保持します"));
			}
			return false;
		}

		// 対応外のステート（Ready など）は警告なしで失敗扱い
		return false;
	}
}
