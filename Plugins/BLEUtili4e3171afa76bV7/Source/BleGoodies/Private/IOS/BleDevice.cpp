// Copyright (c) 2020 Nineva Studios

#include "BleDevice.h"

#include "BleManager.h"
#include "BleGoodiesLog.h"
#include "BleUtils.h"
#include "IOS/BleDeviceDelegate.h"

#import <CoreBluetooth/CoreBluetooth.h>

FString ConvertToFullUUID(const FString& ShortUUID)
{
	FString FullUUID = "";
	if (ShortUUID.Len() == 4) // this is a 16bit representation
	{
		FullUUID += "0000";
		FullUUID += ShortUUID;
		FullUUID += "-0000-1000-8000-00805F9B34FB";
	}
	else if (ShortUUID.Len() == 8) // this is a 32bit representation
	{
		FullUUID += ShortUUID;
		FullUUID += "-0000-1000-8000-00805F9B34FB";
	}
	else
	{
		FullUUID += ShortUUID;
	}

	return FullUUID.ToLower();
}

UBleDevice::~UBleDevice()
{
	if (Device) CFBridgingRelease(Device);
}

void UBleDevice::Init(UBleManager* InitManager, CBPeripheral* InitDevice)
{
	Manager = InitManager;
	Device = InitDevice;

	BleDeviceDelegate* DeviceDelegate = [[BleDeviceDelegate alloc] init];

	DeviceDelegate.onRead = ^(FString ServiceUuid, FString CharacteristicUuid, NSData* Data) {
		TArray<uint8> ByteData;
		uint8* ByteArray = (uint8*)Data.bytes;
		for (int i = 0; i < Data.length; i++)
		{
			ByteData.Add(ByteArray[i]);
		}
		OnReadDelegate.ExecuteIfBound(ConvertToFullUUID(ServiceUuid), ConvertToFullUUID(CharacteristicUuid), ByteData);
	};
	DeviceDelegate.onWrite = ^(FString ServiceUuid, FString CharacteristicUuid) {
		OnWriteSuccessDelegate.ExecuteIfBound(ConvertToFullUUID(ServiceUuid), ConvertToFullUUID(CharacteristicUuid));
	};
	DeviceDelegate.onNotification = ^(FString ServiceUuid, FString CharacteristicUuid, NSData* Data) {
		TArray<uint8> ByteData;
		uint8* ByteArray = (uint8*)Data.bytes;
		for (int i = 0; i < Data.length; i++)
		{
			ByteData.Add(ByteArray[i]);
		}
		OnNotificationDelegate.ExecuteIfBound(ConvertToFullUUID(ServiceUuid), ConvertToFullUUID(CharacteristicUuid), ByteData);
	};
	DeviceDelegate.onUnsubscribe = ^(FString ServiceUuid, FString CharacteristicUuid) {
		OnUnsubscribeDelegate.ExecuteIfBound(ConvertToFullUUID(ServiceUuid), ConvertToFullUUID(CharacteristicUuid));
	};
	DeviceDelegate.onError = ^(FString ServiceUuid, FString CharacteristicUuid, FString ErrorMessage) {
		UE_LOG(LogBleGoodies, Error, TEXT("Characteristic operation error: %s"), *ErrorMessage);
		OnCharacteristicErrorDelegate.ExecuteIfBound(ConvertToFullUUID(ServiceUuid), ConvertToFullUUID(CharacteristicUuid), ErrorMessage);
	};

	Device.delegate = DeviceDelegate;
}

void UBleDevice::Connect(const FBleDelegate& OnConnect, const FBleErrorDelegate& OnConnectError)
{
	OnConnectDelegate = OnConnect;
	OnConnectErrorDelegate = OnConnectError;

	Manager->SetTargetDevice(this);
	Manager->ConnectDevice(Device);
}

void UBleDevice::OnConnect()
{
	[Device discoverServices:nil];
	OnConnectDelegate.ExecuteIfBound();
}

void UBleDevice::Disconnect(const FBleDelegate& OnDisconnect, const FBleErrorDelegate& OnDisconnectError)
{
	OnDisconnectDelegate = OnDisconnect;
	OnDisconnectErrorDelegate = OnDisconnectError;

	Manager->SetTargetDevice(this);
	Manager->DisconnectDevice(Device);
}

bool UBleDevice::IsConnected() const
{
	return Device.state == CBPeripheralStateConnected;
}

FString UBleDevice::GetDeviceName() const
{
	return FString(Device.name);
}

FString UBleDevice::GetDeviceId() const
{
	return FString([Device.identifier UUIDString]);
}

TArray<FString> UBleDevice::GetServices(const TArray<FString>& FilterUUIDs) const
{
	TArray<FString> ServiceUUIDs;
	if (Device.services != nil)
	{
		for (CBService* Service in Device.services)
		{
			FString ServiceUUID(Service.UUID.UUIDString);
			ServiceUUID = ConvertToFullUUID(ServiceUUID);

			if (FilterUUIDs.Num() > 0)
			{
				if (FilterUUIDs.Contains(ServiceUUID))
					ServiceUUIDs.Add(ServiceUUID);
			}
			else
			{
				ServiceUUIDs.Add(ServiceUUID);
			}
		}
	}

	return ServiceUUIDs;
}

bool UBleDevice::IsServiceAvailable(const FString& ServiceUUID) const
{
	bool bResult = false;
	if (!UBleUtils::IsUUIDValid(ServiceUUID))
	{
		UE_LOG(LogBleGoodies, Error, TEXT("Invalid UUID: %s"), *ServiceUUID);
		return bResult;
	}

	CBUUID* ServiceCBUUID = [CBUUID UUIDWithString:ServiceUUID.GetNSString()];
	if (Device.services != nil)
	{
		for (CBService* Service in Device.services)
		{
			if ([Service.UUID isEqual:ServiceCBUUID])
			{
				bResult = true;
				break;
			}
		}
	}

	return bResult;
}

CBCharacteristic* UBleDevice::GetCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID) const
{
	if (!IsConnected())
	{
		OnCharacteristicErrorDelegate.ExecuteIfBound(ServiceUUID, CharacteristicUUID, "Cannot perform operation. Device is not connected.");
		return nullptr;
	}

	if (!UBleUtils::IsUUIDValid(ServiceUUID))
	{
		OnCharacteristicErrorDelegate.ExecuteIfBound(ServiceUUID, CharacteristicUUID, "Invalid service UUID.");
		return nullptr;
	}

	if (!UBleUtils::IsUUIDValid(CharacteristicUUID))
	{
		OnCharacteristicErrorDelegate.ExecuteIfBound(ServiceUUID, CharacteristicUUID, "Invalid characteristic UUID.");
		return nullptr;
	}

	CBUUID* ServiceCBUUID = [CBUUID UUIDWithString:ServiceUUID.GetNSString()];
	CBUUID* CharacteristicCBUUID = [CBUUID UUIDWithString:CharacteristicUUID.GetNSString()];

	if (Device.services == nil)
	{
		OnCharacteristicErrorDelegate.ExecuteIfBound(ServiceUUID, CharacteristicUUID, "Service unavailable");
		return nullptr;
	}

	CBService* TargetService = nil;
	if (Device.services != nil)
	{
		for (CBService* Service in Device.services)
		{
			if ([Service.UUID isEqual:ServiceCBUUID])
			{
				TargetService = Service;
				break;
			}
		}
	}

	if (TargetService == nil)
	{
		OnCharacteristicErrorDelegate.ExecuteIfBound(ServiceUUID, CharacteristicUUID, "Service unavailable");
		return nullptr;
	}

	CBCharacteristic* TargetCharacteristic = nil;
	if (TargetService.characteristics != nil)
	{
		for (CBCharacteristic* Characteristic in TargetService.characteristics)
		{
			if ([Characteristic.UUID isEqual:CharacteristicCBUUID])
			{
				TargetCharacteristic = Characteristic;
				break;
			}
		}
	}

	if (TargetCharacteristic == nil)
	{
		OnCharacteristicErrorDelegate.ExecuteIfBound(ServiceUUID, CharacteristicUUID, "Characteristic unavailable");
		return nullptr;
	}

	return TargetCharacteristic;
}

void UBleDevice::ReadCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID)
{
	CBCharacteristic* Characteristic = GetCharacteristic(ServiceUUID, CharacteristicUUID);
	if (Characteristic)
	{
		[Device readValueForCharacteristic:Characteristic];
	}
}

void UBleDevice::WriteCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, const TArray<uint8>& Data)
{
	CBCharacteristic* Characteristic = GetCharacteristic(ServiceUUID, CharacteristicUUID);
	if (Characteristic)
	{
		[Device writeValue:[NSData dataWithBytes:(void*)Data.GetData() length:Data.Num()]
			forCharacteristic:Characteristic
						 type:CBCharacteristicWriteWithResponse];
	}
}

void UBleDevice::SubscribeToCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, bool /*bIsIndicationCharacteristic*/)
{
	CBCharacteristic* Characteristic = GetCharacteristic(ServiceUUID, CharacteristicUUID);
	if (Characteristic)
	{
		[Device setNotifyValue:YES forCharacteristic:Characteristic];
	}
}

void UBleDevice::UnsubscribeFromCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID)
{
	CBCharacteristic* Characteristic = GetCharacteristic(ServiceUUID, CharacteristicUUID);
	if (Characteristic)
	{
		[Device setNotifyValue:NO forCharacteristic:Characteristic];
	}
}
