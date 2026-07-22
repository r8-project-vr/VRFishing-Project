// Copyright (c) 2020 Nineva Studios

#include "BleUtils.h"

#include "BleGoodiesLog.h"
#include "Interface/BleManagerInterface.h"
#include "Misc/Parse.h"

#if PLATFORM_IOS || PLATFORM_ANDROID
#include "BleManager.h"
#endif

TScriptInterface<IBleManagerInterface> UBleUtils::CreateBleManager()
{
	TScriptInterface<IBleManagerInterface> BleManagerInterface;

#if PLATFORM_IOS || PLATFORM_ANDROID
	UBleManager* BleManager = NewObject<UBleManager>();
	BleManager->Init();

	BleManagerInterface.SetObject(BleManager);
	BleManagerInterface.SetInterface(Cast<IBleManagerInterface>(BleManager));
#endif

	return BleManagerInterface;
}

FString UBleUtils::ConvertBytesToHexString(const TArray<uint8>& Bytes)
{
	return BytesToHex(Bytes.GetData(), Bytes.Num());
}

FString UBleUtils::HexToUUID(const FString& InHexString)
{
	FString HexString = InHexString.TrimStartAndEnd();
	const int StartPos = HexString.StartsWith(TEXT("0x")) ? 2 : 0;

	if (HexString.Len() - StartPos > 4)
	{
		UE_LOG(LogBleGoodies, Error, TEXT("Cannot convert %s as it is not a 16 bit value"), *HexString);
		return FString();
	}

	for (int i = StartPos; i < HexString.Len(); i++)
	{
		if (!CheckTCharIsHex(HexString[i]))
		{
			UE_LOG(LogBleGoodies, Error, TEXT("Failed to convert %s to a valid UUID, found invalid character: %c"), *HexString, HexString[i]);
			return FString();
		}
	}

	uint32 Id = FParse::HexNumber(*HexString);
	FGuid Guid = FGuid(Id, 0x00001000, 0x80000080, 0x5f9b34fb);

	if (!Guid.IsValid())
	{
		UE_LOG(LogBleGoodies, Error, TEXT("Failed to convert %s to a valid UUID"), *HexString);
		return FString();
	}

	FString GuidString = Guid.ToString(EGuidFormats::DigitsWithHyphens);
	return GuidString.ToLower();
}

TArray<FString> UBleUtils::HexToUUIDs(const TArray<FString>& HexStrings)
{
	TArray<FString>UuidArray;

	for (const FString& HexString : HexStrings)
	{
		UuidArray.Add(UBleUtils::HexToUUID(HexString));
	}

	return UuidArray;
}

bool UBleUtils::IsUUIDValid(const FString& UUID)
{
	FGuid Guid;
	
	if (UUID.Len() == 36 && !FGuid::ParseExact(UUID, EGuidFormats::DigitsWithHyphens, Guid)) return false;
	return Guid.IsValid();
}
