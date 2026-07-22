// Copyright (c) 2020 Nineva Studios

#pragma once

#include "Interface/BleManagerInterface.h"

#include "BleManager.generated.h"

class UBleDevice;
@class CBPeripheral;
@class CBCentralManager;
@class BleManagerDelegate;

UCLASS()
class UBleManager : public UObject, public IBleManagerInterface
{
	GENERATED_BODY()

public:
	virtual ~UBleManager() = default;

	virtual void Init() override;
	virtual bool IsBleSupported() const override;
	virtual bool IsBluetoothEnabled() const override;
	virtual void SetBluetoothState(bool bState) override;
	virtual void ScanForDevices(const TArray<FString>& ServiceUUIDs, const FBleOnDeviceFoundDelegate& OnDeviceFound) override;
	virtual void ScanForAdvertisements(const TArray<FString>& NameFilters, const FBleOnAdvertisementDelegate& OnAdvertisement) override;
	virtual void StopScan() override;

	void SetTargetDevice(UBleDevice* Target);
	void ConnectDevice(CBPeripheral* Device) const;
	void DisconnectDevice(CBPeripheral* Device) const;

private:
	CBCentralManager* CentralManager;
	BleManagerDelegate* ManagerDelegate;

	UPROPERTY()
	UBleDevice* DeviceTarget = nullptr;
};
