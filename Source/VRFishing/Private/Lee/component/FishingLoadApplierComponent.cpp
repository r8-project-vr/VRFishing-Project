// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/component/FishingLoadApplierComponent.h"
#include "Lee/subsystem/FishingLoadSettingsSubsystem.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "VRFishingLog.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/UnrealType.h"

UFishingLoadApplierComponent::UFishingLoadApplierComponent()
{
	// 適用は BeginPlay の一回のみのため Tick は無効
	PrimaryComponentTick.bCanEverTick = false;
}

void UFishingLoadApplierComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyLoadSettings();
}

void UFishingLoadApplierComponent::ApplyLoadSettings()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogFishing, Warning, TEXT("[LoadSettings] GameInstance が取得できないため負荷設定を適用できません"));
		return;
	}

	UFishingLoadSettingsSubsystem* Subsystem = GameInstance->GetSubsystem<UFishingLoadSettingsSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogFishing, Warning, TEXT("[LoadSettings] FishingLoadSettingsSubsystem が取得できないため負荷設定を適用できません"));
		return;
	}

	AActor* Owner = GetOwner();

	// ==================== 1. 上下運動の目標回数（public ため直接書き込み） ====================
	int32 VerticalCount = Subsystem->GetVerticalTargetCount();
	if (UFishingStateHandUpDown* HandUpDown = Owner ? Owner->FindComponentByClass<UFishingStateHandUpDown>() : nullptr)
	{
		HandUpDown->TargetUpAndDownCount = FMath::Max(1, VerticalCount);
	}
	else
	{
		UE_LOG(LogFishing, Warning, TEXT("[LoadSettings] UFishingStateHandUpDown が見つからないため上下運動の負荷を適用できません"));
	}

	// ==================== 2. リールの目標回転数（protected ため反射で書き込み） ====================
	int32 RotationCount = Subsystem->GetRotationTargetCount();
	if (UFishingReelStateComponent* ReelState = Owner ? Owner->FindComponentByClass<UFishingReelStateComponent>() : nullptr)
	{
		// 2026.08.19 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// TargetRevolutionCount は Tanimura 氏の protected UPROPERTY。チーム規約により
		// 本人のコードへ setter を追加できないため、リフレクションで直接書き込む。
		WriteProtectedIntProperty(ReelState, TEXT("TargetRevolutionCount"), FMath::Max(1, RotationCount));
		// 2026.08.19 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	}
	else
	{
		UE_LOG(LogFishing, Warning, TEXT("[LoadSettings] UFishingReelStateComponent が見つからないためリールの負荷を適用できません"));
	}

	// ==================== 3. 運動時間（未設定時は GameMode アセット値を尊重） ====================
	FString TimeSummary = TEXT("未設定(GameMode値を尊重)");
	if (Subsystem->HasExerciseTimeOverride())
	{
		if (AGameModeBase* GameMode = World->GetAuthGameMode())
		{
			// 2026.08.19 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
			// TotalGameTime は Tanimura 氏の protected UPROPERTY。チーム規約により
			// 本人のコードへ setter を追加できないため、リフレクションで直接書き込む。
			if (WriteProtectedFloatProperty(GameMode, TEXT("TotalGameTime"), Subsystem->GetExerciseTimeSeconds()))
			{
				TimeSummary = FString::Printf(TEXT("%.0f秒"), Subsystem->GetExerciseTimeSeconds());
			}
			// 2026.08.19 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		}
		else
		{
			UE_LOG(LogFishing, Warning, TEXT("[LoadSettings] GameMode が取得できないため運動時間を適用できません"));
		}
	}

	// ==================== 4. 適用結果のログ ====================
	UE_LOG(LogFishing, Log, TEXT("[LoadSettings] 適用完了: 上下=%d回(%s) / 巻取=%d回(%s) / 時間=%s"),
		VerticalCount,
		*UEnum::GetDisplayValueAsText(Subsystem->GetVerticalPreset()).ToString(),
		RotationCount,
		*UEnum::GetDisplayValueAsText(Subsystem->GetRotationPreset()).ToString(),
		*TimeSummary);
}

bool UFishingLoadApplierComponent::WriteProtectedIntProperty(UObject* Object, const TCHAR* PropertyName, int32 Value)
{
	if (!Object)
	{
		return false;
	}

	// FindFProperty はクラスから継承チェーンへ向かって検索する（最近定義が最優先）
	FIntProperty* Property = FindFProperty<FIntProperty>(Object->GetClass(), PropertyName);
	if (!Property)
	{
		UE_LOG(LogFishing, Error, TEXT("[LoadSettings] int32 プロパティが見つかりません: %s::%s"),
			*Object->GetClass()->GetName(), PropertyName);
		return false;
	}

	Property->SetPropertyValue_InContainer(Object, Value);
	return true;
}

bool UFishingLoadApplierComponent::WriteProtectedFloatProperty(UObject* Object, const TCHAR* PropertyName, float Value)
{
	if (!Object)
	{
		return false;
	}

	// FindFProperty はクラスから継承チェーンへ向かって検索する（最近定義が最優先）
	FFloatProperty* Property = FindFProperty<FFloatProperty>(Object->GetClass(), PropertyName);
	if (!Property)
	{
		UE_LOG(LogFishing, Error, TEXT("[LoadSettings] float プロパティが見つかりません: %s::%s"),
			*Object->GetClass()->GetName(), PropertyName);
		return false;
	}

	Property->SetPropertyValue_InContainer(Object, Value);
	return true;
}
