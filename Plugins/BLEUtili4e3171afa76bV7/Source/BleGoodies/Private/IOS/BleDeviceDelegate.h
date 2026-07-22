// Copyright (c) 2020 Nineva Studios

#pragma once

#include "Containers/UnrealString.h"
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

@interface BleDeviceDelegate : NSObject<CBPeripheralDelegate>

@property(nonatomic, copy) void (^onWrite)(FString ServiceUuid, FString CharacteristicUuid);
@property(nonatomic, copy) void (^onRead)(FString ServiceUuid, FString CharacteristicUuid, NSData* Data);
@property(nonatomic, copy) void (^onNotification)(FString ServiceUuid, FString CharacteristicUuid, NSData* Data);
@property(nonatomic, copy) void (^onUnsubscribe)(FString ServiceUuid, FString CharacteristicUuid);
@property(nonatomic, copy) void (^onError)(FString ServiceUuid, FString CharacteristicUuid, FString ErrorMessage);

@end
