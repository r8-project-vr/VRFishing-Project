// Copyright (c) 2020 Nineva Studios

#pragma once

#include "Android/AndroidJava.h"
#include "Interface/BleManagerInterface.h"

#include "BleManager.generated.h"

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

	void OnDeviceFound(TScriptInterface<IBleDeviceInterface> Device) { OnDeviceFoundDelegate.ExecuteIfBound(Device); }
	void OnAdvertisement(const FString& DeviceName, const TArray<uint8>& Data) { OnAdvertisementDelegate.ExecuteIfBound(DeviceName, Data); }

private:
	TSharedPtr<FJavaClassObject> BleManagerJObject;
	FJavaClassMethod IsBleSupportedMethod;
	FJavaClassMethod IsBluetoothEnabledMethod;
	FJavaClassMethod SetBluetoothStateMethod;
	FJavaClassMethod StartScanForDevicesMethod;
	FJavaClassMethod StartScanForAdvertisementsMethod;
	FJavaClassMethod StopScanMethod;
};
