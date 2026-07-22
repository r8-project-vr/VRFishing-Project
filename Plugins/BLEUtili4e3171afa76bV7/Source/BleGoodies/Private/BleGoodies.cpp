// Copyright (c) 2020 Nineva Studios

#include "BleGoodies.h"
#include "BleGoodiesLog.h"

#define LOCTEXT_NAMESPACE "FBleGoodiesModule"

void FBleGoodiesModule::StartupModule()
{
}

void FBleGoodiesModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBleGoodiesModule, BleGoodies)

DEFINE_LOG_CATEGORY(LogBleGoodies);
