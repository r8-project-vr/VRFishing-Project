// Copyright (c) 2020 Nineva Studios

#pragma once

#include "Interface/BleDeviceInterface.h"

#include "BleDevice.generated.h"

class UBleManager;
@class CBPeripheral;
@class CBCharacteristic;

UCLASS()
class UBleDevice : public UObject, public IBleDeviceInterface
{
	GENERATED_BODY()

public:
	virtual ~UBleDevice();

	void Init(UBleManager* InitManager, CBPeripheral* InitDevice);

	virtual void Connect(const FBleDelegate& OnConnect, const FBleErrorDelegate& OnConnectError) override;
	virtual void Disconnect(const FBleDelegate& OnDisconnect, const FBleErrorDelegate& OnDisconnectError) override;
	virtual bool IsConnected() const override;
	virtual FString GetDeviceName() const override;
	virtual FString GetDeviceId() const override;
	virtual TArray<FString> GetServices(const TArray<FString>& FilterUUIDs) const override;
	virtual bool IsServiceAvailable(const FString& ServiceUUID) const override;
	virtual void ReadCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID) override;
	virtual void WriteCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, const TArray<uint8>& Data) override;
	virtual void SubscribeToCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, bool bIsIndicationCharacteristic) override;
	virtual void UnsubscribeFromCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID) override;

	void OnConnect();
	void OnDisconnect() { OnDisconnectDelegate.ExecuteIfBound(); }
	void OnConnectError(const FString& ErrorMessage) { OnConnectErrorDelegate.ExecuteIfBound(ErrorMessage); }
	void OnDisconnectError(const FString& ErrorMessage) { OnDisconnectErrorDelegate.ExecuteIfBound(ErrorMessage); }

private:
	CBCharacteristic* GetCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID) const;
	CBPeripheral* Device;
	
	UPROPERTY()
	UBleManager* Manager = nullptr;
};
