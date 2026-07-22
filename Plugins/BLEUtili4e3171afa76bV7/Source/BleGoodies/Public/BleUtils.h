// Copyright (c) 2020 Nineva Studios

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "BleUtils.generated.h"

UCLASS()
class BLEGOODIES_API UBleUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* Create a BLE Manager
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	static TScriptInterface<IBleManagerInterface> CreateBleManager();

	/**
	 * Convert an array of bytes to a hex string
	 *
	 * @return - Hex string representing array of bytes
	 */
	UFUNCTION(BlueprintPure, Category = "BLE Goodies")
	static FString ConvertBytesToHexString(const TArray<uint8>& Bytes);

	/**
	* Parse a 16 bit hex value into a UUID string
	*
	* @return - UUID string
	*/
	UFUNCTION(BlueprintPure, Category = "BLE Goodies")
	static FString HexToUUID(const FString& HexString);

	/**
	* Parse an array of 16 bit hex values into an array of UUID strings
	*
	* @return - Array of UUID strings
	*/
	UFUNCTION(BlueprintPure, Category = "BLE Goodies")
	static TArray<FString> HexToUUIDs(const TArray<FString>& HexStrings);

	/**
	* Check if a provided UUID is a valid fully qualified UUID in the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
	* This will return false for short hex UUIDs.
	*
	* @return - True if the UUID string is valid, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "BLE Goodies")
	static bool IsUUIDValid(const FString& UUID);
};
