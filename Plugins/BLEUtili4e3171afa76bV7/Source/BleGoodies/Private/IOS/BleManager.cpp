// Copyright (c) 2020 Nineva Studios

#include "BleManager.h"
#include "BleUtils.h"
#include "BleDevice.h"
#include "BleGoodiesLog.h"
#include "BleManagerDelegate.h"

#import <CoreBluetooth/CoreBluetooth.h>

void UBleManager::Init()
{
	UE_LOG(LogBleGoodies, Verbose, TEXT("Initialising iOS BLE Manager"));

	ManagerDelegate = [[BleManagerDelegate alloc] init];

	ManagerDelegate.onDeviceFound = ^(CBPeripheral* Peripheral) {
		UBleDevice* Device = NewObject<UBleDevice>();
		Device->Init(this, Peripheral);

		OnDeviceFoundDelegate.ExecuteIfBound(Device);
	};
	ManagerDelegate.onAdvertisementReceived = ^(FString DeviceName, NSData* Data) {
		TArray<uint8> ByteData;
		uint8* ByteArray = (uint8*)Data.bytes;
		if (ByteArray)
		{
			// skip the first two prefix bytes (0x0201)
			const int StartIndex = ByteArray[0] == 0x02 && ByteArray[1] == 0x01 ? 2 : 0;
			for (int i = StartIndex; i < Data.length; i++)
			{
				ByteData.Add(ByteArray[i]);
			}
			[Data release];
		}

		OnAdvertisementDelegate.ExecuteIfBound(DeviceName, ByteData);
	};
	ManagerDelegate.onDeviceConnected = ^(CBPeripheral* Peripheral) {
		if (DeviceTarget)
		{
			DeviceTarget->OnConnect();
			DeviceTarget = nullptr;
		}
	};
	ManagerDelegate.onFailToConnect = ^(CBPeripheral* Peripheral, FString ErrorMessage) {
		if (DeviceTarget)
		{
			DeviceTarget->OnConnectError(ErrorMessage);
			DeviceTarget = nullptr;
		}
	};
	ManagerDelegate.onDeviceDisconnected = ^(CBPeripheral* Peripheral) {
		if (DeviceTarget)
		{
			DeviceTarget->OnDisconnect();
			DeviceTarget = nullptr;
		}
	};
	ManagerDelegate.onFailToDisconnect = ^(CBPeripheral* Peripheral, FString ErrorMessage) {
		if (DeviceTarget)
		{
			DeviceTarget->OnDisconnectError(ErrorMessage);
			DeviceTarget = nullptr;
		}
	};

	CentralManager = [[CBCentralManager alloc] initWithDelegate:ManagerDelegate queue:nil];
}

bool UBleManager::IsBleSupported() const
{
	return CentralManager.state != CBManagerStateUnsupported;
}

bool UBleManager::IsBluetoothEnabled() const
{
	return CentralManager.state == CBManagerStatePoweredOn;
}

void UBleManager::SetBluetoothState(bool bState)
{
	UE_LOG(LogBleGoodies, Warning, TEXT("This functionality is not available on iOS"));
}

void UBleManager::ScanForDevices(const TArray<FString>& ServiceUUIDs, const FBleOnDeviceFoundDelegate& OnDeviceFound)
{
	if (CentralManager.state != CBManagerStatePoweredOn)
	{
		UE_LOG(LogBleGoodies, Error, TEXT("BLE Manager is not initialized. Initialization may take some time - try to invoke this function after a delay."));
		return;
	}

	if (bIsScanning)
	{
		UE_LOG(LogBleGoodies, Warning, TEXT("Scan is already in progress."));
		return;
	}

	OnDeviceFoundDelegate = OnDeviceFound;
	bIsScanning = true;

	NSMutableArray* UUIDs = [NSMutableArray arrayWithCapacity:ServiceUUIDs.Num()];
	for (const auto& UUID : ServiceUUIDs)
	{
		if (!UUID.IsEmpty() && UBleUtils::IsUUIDValid(UUID))
			[UUIDs addObject:[CBUUID UUIDWithString:UUID.GetNSString()]];
	}

	ManagerDelegate.bScanningForAdvertisements = false;
	[CentralManager scanForPeripheralsWithServices:UUIDs
										   options:@{CBCentralManagerScanOptionAllowDuplicatesKey: @NO}];
}

void UBleManager::ScanForAdvertisements(const TArray<FString>& NameFilters, const FBleOnAdvertisementDelegate& OnAdvertisement)
{
	if (CentralManager.state != CBManagerStatePoweredOn)
	{
		UE_LOG(LogBleGoodies, Error, TEXT("BLE Manager is not initialized. Initialization may take some time - try to invoke this function after a delay."));
		return;
	}

	if (bIsScanning)
	{
		UE_LOG(LogBleGoodies, Warning, TEXT("Scan is already in progress."));
		return;
	}

	OnAdvertisementDelegate = OnAdvertisement;
	bIsScanning = true;

	NSMutableArray* NameFiltersArray = [NSMutableArray arrayWithCapacity:NameFilters.Num()];
	for (const auto& Name : NameFilters)
	{
		[NameFiltersArray addObject:Name.GetNSString()];
	}

	ManagerDelegate.bScanningForAdvertisements = true;
	ManagerDelegate.NameFilters = NameFiltersArray;
	[CentralManager scanForPeripheralsWithServices:nil
										   options:@{CBCentralManagerScanOptionAllowDuplicatesKey: @NO}];
}

void UBleManager::StopScan()
{
	bIsScanning = false;
	[CentralManager stopScan];
}

void UBleManager::SetTargetDevice(UBleDevice* Target)
{
	if (DeviceTarget)
	{
		UE_LOG(LogBleGoodies, Error, TEXT("Connect/Disconnect operation in progress. Try again later."));
		return;
	}
	DeviceTarget = Target;
}

void UBleManager::ConnectDevice(CBPeripheral* Device) const
{
	[CentralManager connectPeripheral:Device options:nil];
}

void UBleManager::DisconnectDevice(CBPeripheral* Device) const
{
	[CentralManager cancelPeripheralConnection:Device];
}

