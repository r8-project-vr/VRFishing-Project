// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Tanimura/Subsystem/FishingSettingsSubsystem.h"

void UFishingSettingsSubsystem::SetRotationLoadLevel(int32 NewLevel)
{
    RotationLoadLevel = NewLevel;
}

int32 UFishingSettingsSubsystem::GetRotationLoadLevel() const
{
    return RotationLoadLevel;
}
