// Copyright (c) 2020 Nineva Studios

#include "BleDeviceDelegate.h"

#include "Async/Async.h"
#include "BleGoodiesLog.h"

@implementation BleDeviceDelegate
{
}

- (void)peripheral:(CBPeripheral*)peripheral didDiscoverServices:(NSError*)error
{
	if (error)
	{
		const FString ErrorMessage([error localizedDescription]);
		UE_LOG(LogBleGoodies, Error, TEXT("Failed to discover services: %s"), *ErrorMessage);
		return;
	}

	for (CBService* service in peripheral.services)
	{
		[peripheral discoverCharacteristics:nil forService:service];
	}
}

- (void)peripheral:(CBPeripheral*)peripheral didDiscoverCharacteristicsForService:(CBService*)service error:(NSError*)error
{
	if (error)
	{
		const FString ErrorMessage([error localizedDescription]);
		UE_LOG(LogBleGoodies, Error, TEXT("Failed to discover characteristics: %s"), *ErrorMessage);
		return;
	}
}

- (void)peripheral:(CBPeripheral*)peripheral didWriteValueForCharacteristic:(CBCharacteristic*)characteristic error:(NSError*)error
{
	const FString ServiceUuid(characteristic.service.UUID.UUIDString);
	const FString CharacteristicUuid(characteristic.UUID.UUIDString);

	if (error)
	{
		const FString ErrorMessage([error localizedDescription]);
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onError(ServiceUuid, CharacteristicUuid, ErrorMessage);
		});
		return;
	}

	AsyncTask(ENamedThreads::GameThread, [=]() {
		_onWrite(ServiceUuid, CharacteristicUuid);
	});
}

- (void)peripheral:(CBPeripheral*)peripheral didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic error:(NSError*)error
{
	const FString ServiceUuid(characteristic.service.UUID.UUIDString);
	const FString CharacteristicUuid(characteristic.UUID.UUIDString);

	if (error)
	{
		const FString ErrorMessage([error localizedDescription]);
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onError(ServiceUuid, CharacteristicUuid, ErrorMessage);
		});
		return;
	}

	if (characteristic.isNotifying)
	{
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onNotification(ServiceUuid, CharacteristicUuid, characteristic.value);
		});
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onRead(ServiceUuid, CharacteristicUuid, characteristic.value);
		});
	}
}

- (void)peripheral:(CBPeripheral*)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic*)characteristic error:(NSError*)error
{
	const FString ServiceUuid(characteristic.service.UUID.UUIDString);
	const FString CharacteristicUuid(characteristic.UUID.UUIDString);

	if (error)
	{
		const FString ErrorMessage([error localizedDescription]);
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onError(ServiceUuid, CharacteristicUuid, ErrorMessage);
		});
		return;
	}

	if (!characteristic.isNotifying)
	{
		AsyncTask(ENamedThreads::GameThread, [=]() {
			_onUnsubscribe(ServiceUuid, CharacteristicUuid);
		});
		return;
	}
}

@end
