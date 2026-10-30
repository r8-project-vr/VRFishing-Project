// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Tanimura/DataAsset/BoidsFishSettingsTable.h"
#include "Tanimura/DataAsset/BoidsFishSettings.h"
#include "Takeuchi/Data/FishDataAsset.h"

UBoidsFishSettings* UBoidsFishSettingsTable::FindSettings(const UFishDataAsset* FishData) const
{
	if (const FBoidsFishSettingsTableEntry* Entry = FindEntry(FishData)) {
		return Entry->Settings.LoadSynchronous();
	}

	return nullptr;
}

int32 UBoidsFishSettingsTable::FindMaxCount(const UFishDataAsset* FishData) const
{
	if (const FBoidsFishSettingsTableEntry* Entry = FindEntry(FishData)) {
		return FMath::Max(Entry->MaxCount, 0);
	}

	// 未登録の魚種は上限なし
	return 0;
}

const FBoidsFishSettingsTableEntry* UBoidsFishSettingsTable::FindEntry(const UFishDataAsset* FishData) const
{
	if (!IsValid(FishData)) {
		return nullptr;
	}

	const FSoftObjectPath FishDataPath(FishData);

	for (const FBoidsFishSettingsTableEntry& Entry : Entries) {
		// パスが一致する魚種の項目を返す
		if (Entry.FishData.ToSoftObjectPath() == FishDataPath) {
			return &Entry;
		}
	}

	return nullptr;
}
