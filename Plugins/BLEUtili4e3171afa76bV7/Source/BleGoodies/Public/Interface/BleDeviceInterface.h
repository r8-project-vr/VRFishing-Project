// Copyright (c) 2020 Nineva Studios

#pragma once

#include "UObject/Interface.h"

#include "BleDeviceInterface.generated.h"

DECLARE_DYNAMIC_DELEGATE(FBleDelegate);
DECLARE_DYNAMIC_DELEGATE_OneParam(FBleErrorDelegate, FString, ErrorMessage);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FBleCharacteristicDelegate, FString, ServiceUUID, FString, CharacteristicUUID);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FBleCharacteristicDataDelegate, FString, ServiceUUID, FString, CharacteristicUUID, const TArray<uint8>&, Data);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FBleCharacteristicErrorDelegate, FString, ServiceUUID, FString, CharacteristicUUID, FString, ErrorMessage);

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class BLEGOODIES_API UBleDeviceInterface : public UInterface
{
	GENERATED_BODY()
};

class BLEGOODIES_API IBleDeviceInterface
{
	GENERATED_BODY()

public:
	/**
	* Connect to to a BLE device
	*
	* @param OnConnect - Called on successful connection
	* @param OnConnectError - Called in case an error occurs during connection
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies", meta = (AutoCreateRefTerm = "OnConnectError"))
	virtual void Connect(const FBleDelegate& OnConnect, const FBleErrorDelegate& OnConnectError) = 0;

	/**
	* Disconnect from a BLE device
	*
	* @param OnDisconnect - Called on successful disconnection
	* @param OnDisconnectError - Called in case an error occurs during disconnection
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies", meta = (AutoCreateRefTerm = "OnDisconnectError"))
	virtual void Disconnect(const FBleDelegate& OnDisconnect, const FBleErrorDelegate& OnDisconnectError) = 0;

	/**
	* Check if this device is connected to
	*
	* @return - True if device is connected, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual bool IsConnected() const = 0;

	/**
	* Get device name
	*
	* @return - Device name
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual FString GetDeviceName() const = 0;

	/**
	* Get device id
	*
	* @return - Device id
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual FString GetDeviceId() const = 0;

	/**
	* Get device service UUIDs
	*
	* @param FilterUUIDs - Array of UUIDs, the resulting list will only contain services that are specified here
	* @return - An array of available service UUIDs
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies", meta = (AutoCreateRefTerm = "FilterUUIDs", AdvancedDisplay = "FilterUUIDs"))
	virtual TArray<FString> GetServices(const TArray<FString>& FilterUUIDs) const = 0;

	/**
	* Check if a service is available on the device
	*
	* @return - True if services is available on the device, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual bool IsServiceAvailable(const FString& ServiceUUID) const = 0;

	/**
	* Read service characteristic value
	*
	* @param ServiceUUID - Service UUID that contains the specified characteristic
	* @param CharacteristicUUID - Characteristic UUID to read from
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void ReadCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID) = 0;

	/**
	* Write service characteristic value
	*
	* @param ServiceUUID - Service UUID that contains the specified characteristic
	* @param CharacteristicUUID - Characteristic UUID to write to
	* @param Data - Array of bytes tp write
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void WriteCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, const TArray<uint8>& Data) = 0;

	/**
	* Subscribe to a service characteristic to receive notifications
	*
	* @param ServiceUUID - Service UUID that contains the specified characteristic
	* @param CharacteristicUUID - Characteristic UUID to subscribe to
	* @param IsIndicationCharacteristic - Specify if this characteristic is indicative (Android only)
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void SubscribeToCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, bool bIsIndicationCharacteristic) = 0;

	/**
	* Stop receiving service characteristic notifications
	*
	* @param ServiceUUID - Service UUID that contains the specified characteristic
	* @param CharacteristicUUID - Characteristic UUID to unsubscribe from
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void UnsubscribeFromCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID) = 0;

	/**
	* Bind to characteristic read events
	*
	* @param OnRead - Called when a characteristic was read
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void BindToCharacteristicReadEvent(const FBleCharacteristicDataDelegate& OnRead)
	{
		OnReadDelegate = OnRead;
	}

	/**
	* Bind to characteristic write events
	*
	* @param OnWrite - Called when a characteristic was written to
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void BindToCharacteristicWriteEvent(const FBleCharacteristicDelegate& OnWrite)
	{
		OnWriteSuccessDelegate = OnWrite;
	}

	/**
	* Bind to subscribed characteristic notification events
	*
	* @param OnNotification - Called when a characteristic notification is received
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void BindToCharacteristicNotificationEvent(const FBleCharacteristicDataDelegate& OnNotification)
	{
		OnNotificationDelegate = OnNotification;
	}

	/**
	* Bind to characteristic unsubscribe events
	*
	* @param OnUnsubscribe - Called when unsubscribing from a characteristic
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void BindToCharacteristicUnsubscribeEvent(const FBleCharacteristicDelegate& OnUnsubscribe)
	{
		OnUnsubscribeDelegate = OnUnsubscribe;
	}

	/**
	* Bind to characteristic operation error events
	*
	* @param OnError - Called when any error occurs during read/write/subscribe
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	virtual void BindToCharacteristicErrorEvent(const FBleCharacteristicErrorDelegate& OnError)
	{
		OnCharacteristicErrorDelegate = OnError;
	}

protected:
	FBleDelegate OnConnectDelegate;
	FBleDelegate OnDisconnectDelegate;
	FBleErrorDelegate OnConnectErrorDelegate;
	FBleErrorDelegate OnDisconnectErrorDelegate;
	FBleCharacteristicDelegate OnUnsubscribeDelegate;
	FBleCharacteristicDelegate OnWriteSuccessDelegate;
	FBleCharacteristicDataDelegate OnReadDelegate;
	FBleCharacteristicDataDelegate OnNotificationDelegate;
	FBleCharacteristicErrorDelegate OnCharacteristicErrorDelegate;
};
