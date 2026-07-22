// Copyright (c) 2020 Nineva Studios

#include "BleManagerDelegate.h"

#include "Async/Async.h"
#include "BleGoodiesLog.h"

@implementation BleManagerDelegate{}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {}

- (void)centralManager:(CBCentralManager*)central didDiscoverPeripheral:(CBPeripheral*)peripheral
		advertisementData:(NSDictionary<NSString*, id>*)advertisementData
					 RSSI:(NSNumber*)RSSI
{
	if ([self bScanningForAdvertisements])
	{
		if ([[self NameFilters] count] > 0 && [[self NameFilters] containsObject:peripheral.name] == NO)
		{
			return;
		}

		const FString DeviceName(peripheral.name);
		NSData* Data = [[NSData alloc] initWithData:advertisementData[CBAdvertisementDataManufacturerDataKey]];

		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onAdvertisementReceived(DeviceName, Data);
		});
	}
	else
	{
		CBPeripheral* retainedPeripheral = (CBPeripheral*)CFBridgingRetain(peripheral);
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onDeviceFound(retainedPeripheral);
		});
	}
}

- (void)centralManager:(CBCentralManager*)central didConnectPeripheral:(CBPeripheral*)peripheral
{
	AsyncTask(ENamedThreads::GameThread, [=]() {
		_onDeviceConnected(peripheral);
	});
}

- (void)centralManager:(CBCentralManager*)central didFailToConnectPeripheral:(CBPeripheral*)peripheral error:(NSError*)error
{
	const FString ErrorMessage([error localizedDescription]);
	AsyncTask(ENamedThreads::GameThread, [=]() {
		_onFailToConnect(peripheral, ErrorMessage);
	});
}

- (void)centralManager:(CBCentralManager*)central didDisconnectPeripheral:(CBPeripheral*)peripheral error:(NSError*)error
{
	if (error)
	{
		const FString ErrorMessage([error localizedDescription]);
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onFailToDisconnect(peripheral, ErrorMessage);
		});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [=]() {
		_onDeviceDisconnected(peripheral);
	});
}

@end
