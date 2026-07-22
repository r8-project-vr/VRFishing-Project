// Copyright (c) 2020 Nineva Studios

#pragma once

#include "Containers/UnrealString.h"
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

@interface BleManagerDelegate : NSObject <CBCentralManagerDelegate>

@property (nonatomic, assign) bool bScanningForAdvertisements;
@property (nonatomic, copy) NSMutableArray<NSString*>* NameFilters;

@property (nonatomic, copy) void (^onDeviceFound)(CBPeripheral* Peripheral);
@property (nonatomic, copy) void (^onAdvertisementReceived)(FString DeviceName, NSData* Data);
@property (nonatomic, copy) void (^onDeviceConnected)(CBPeripheral* Peripheral);
@property (nonatomic, copy) void (^onFailToConnect)(CBPeripheral* Peripheral, FString ErrorMessage);
@property (nonatomic, copy) void (^onDeviceDisconnected)(CBPeripheral* Peripheral);
@property (nonatomic, copy) void (^onFailToDisconnect)(CBPeripheral* Peripheral, FString ErrorMessage);

@end
