// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Tanimura/DataAsset/BoidsFishSettingsTable.h"
#include "Tanimura/DataAsset/BoidsFishSettings.h"
#include "Takeuchi/Data/FishDataAsset.h"

UBoidsFishSettings* UBoidsFishSettingsTable::FindSettings(const UFishDataAsset* FishData) const
{
	if (!IsValid(FishData)) {
		return nullptr;
	}

	const FSoftObjectPath FishDataPath(FishData);

	for (const FBoidsFishSettingsTableEntry& Entry : Entries) {
		// パスが一致する魚種の設定を返す
		if (Entry.FishData.ToSoftObjectPath() == FishDataPath) {
			return Entry.Settings.LoadSynchronous();
		}
	}

	return nullptr;
}
