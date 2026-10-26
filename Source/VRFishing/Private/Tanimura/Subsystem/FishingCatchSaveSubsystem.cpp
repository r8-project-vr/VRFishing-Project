// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Tanimura/Subsystem/FishingCatchSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Takeuchi/Data/FishDataAsset.h"
#include "VRFishingLog.h"

const FString UFishingCatchSaveSubsystem::SaveSlotName = TEXT("FishingCatchSave");

void UFishingCatchSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// ゲーム起動時に保存済みの釣果を読み込む
	LoadFromDisk();
}

void UFishingCatchSaveSubsystem::AddCaughtFish(UFishDataAsset* FishData, int32 FishLevel, bool bIsRare)
{
	// 魚種データが無いと水族館で表示できないため保存しない
	if (!IsValid(FishData)) {
		UE_LOG(LogFishing, Warning, TEXT("UFishingCatchSaveSubsystem::AddCaughtFish: 魚種データが未設定のため保存しませんでした。"));
		return;
	}

	if (!SaveData) {
		SaveData = NewObject<UFishingCatchSaveGame>(this);
	}

	FFishCatchSaveEntry Entry;
	Entry.FishData = FishData;
	Entry.FishLevel = FishLevel;
	Entry.bIsRare = bIsRare;
	SaveData->CaughtFish.Add(Entry);

	// 釣った直後に保存し、ゲーム終了時にも釣果を残す
	SaveToDisk();
}

TArray<FFishCatchSaveEntry> UFishingCatchSaveSubsystem::GetCaughtFish() const
{
	if (!SaveData) {
		return TArray<FFishCatchSaveEntry>();
	}

	return SaveData->CaughtFish;
}

int32 UFishingCatchSaveSubsystem::GetCaughtFishCount() const
{
	return SaveData ? SaveData->CaughtFish.Num() : 0;
}

void UFishingCatchSaveSubsystem::ClearCaughtFish()
{
	if (!SaveData) {
		SaveData = NewObject<UFishingCatchSaveGame>(this);
	}

	SaveData->CaughtFish.Reset();
	SaveToDisk();
}

bool UFishingCatchSaveSubsystem::SaveToDisk()
{
	if (!SaveData) {
		return false;
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName, 0);
	if (!bSaved) {
		UE_LOG(LogFishing, Warning, TEXT("UFishingCatchSaveSubsystem::SaveToDisk: セーブに失敗しました。"));
	}

	return bSaved;
}

void UFishingCatchSaveSubsystem::LoadFromDisk()
{
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0)) {
		UFishingCatchSaveGame* LoadedSave = Cast<UFishingCatchSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
		if (IsValid(LoadedSave)) {
			SaveData = LoadedSave;
			UE_LOG(LogFishing, Log, TEXT("UFishingCatchSaveSubsystem: 保存済みの釣果 %d 件を読み込みました。"), SaveData->CaughtFish.Num());
			return;
		}

		UE_LOG(LogFishing, Warning, TEXT("UFishingCatchSaveSubsystem: セーブデータを読み込めませんでした。空のデータで開始します。"));
	}

	SaveData = NewObject<UFishingCatchSaveGame>(this);
}
