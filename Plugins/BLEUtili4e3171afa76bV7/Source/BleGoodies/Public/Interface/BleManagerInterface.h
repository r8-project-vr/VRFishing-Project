// Copyright (c) 2020 Nineva Studios

#pragma once

#include "UObject/Interface.h"

#include "BleManagerInterface.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FBleOnDeviceFoundDelegate, TScriptInterface<IBleDeviceInterface>, Device);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FBleOnAdvertisementDelegate, FString, DeviceName, const TArray<uint8>&, Data);

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class BLEGOODIES_API UBleManagerInterface : public UInterface
{
	GENERATED_BODY()
};

class BLEGOODIES_API IBleManagerInterface
{
	GENERATED_BODY()

public:
	virtual void Init() = 0;

	/**
	* Check if Bluetooth Low Energy is supported
	*
	* @return - True if BLE is supported, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual bool IsBleSupported() const = 0;

	/**
	* Check if Bluetooth is enabled
	*
	* @return - True if Bluetooth is enabled, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual bool IsBluetoothEnabled() const = 0;

	/**
	* Enable/disable Bluetooth (Android only)
	*
	* @param bState - New Bluetooth state
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void SetBluetoothState(bool bState) = 0;

	/**
	* Start searching for nearby BLE devices
	*
	* @param ServiceUUIDs - An array of service UUIDs to filter devices
	* @param OnDeviceFound - Called when a new device is found
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies", meta = (AutoCreateRefTerm = "ServiceUUIDs"))
	virtual void ScanForDevices(const TArray<FString>& ServiceUUIDs, const FBleOnDeviceFoundDelegate& OnDeviceFound) = 0;

	/**
	* Start listening for advertisements
	*
	* @param NameFilters - An array of device names to listen to for advertisements
	* @param OnAdvertisement - Called when an advertisement is picked up
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies", meta = (AutoCreateRefTerm = "NameFilters"))
	virtual void ScanForAdvertisements(const TArray<FString>& NameFilters, const FBleOnAdvertisementDelegate& OnAdvertisement) = 0;

	/**
	* Stop searching for nearby BLE devices
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void StopScan() = 0;

	/**
	* Check if scan is in progress
	*
	* @return - True if the subsystem is currently scanning for devices/advertisements
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual bool IsScanning() const { return bIsScanning; }

protected:
	bool bIsScanning = false;

	FBleOnDeviceFoundDelegate OnDeviceFoundDelegate;
	FBleOnAdvertisementDelegate OnAdvertisementDelegate;
};
