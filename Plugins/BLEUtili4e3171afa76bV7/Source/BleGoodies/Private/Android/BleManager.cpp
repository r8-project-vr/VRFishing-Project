// Copyright (c) 2020 Nineva Studios

#include "BleManager.h"

#include "Async/Async.h"
#include "BleDevice.h"
#include "BleGoodiesLog.h"
#include "Android/AndroidJavaEnv.h"
#include "Android/AndroidPlatform.h"
#include "Utils/BleJavaConvertor.h"
#include "Utils/BleGoodiesAndroidScopedObject.h"

void UBleManager::Init()
{
	UE_LOG(LogBleGoodies, Verbose, TEXT("Initialising Android BLE Manager"));

	BleManagerJObject = MakeShared<FJavaClassObject>(
		"com/ninevastudios/ble/BleGoodiesManager", "(Landroid/app/Activity;J)V", FJavaWrapper::GameActivityThis, (jlong)this);

	IsBleSupportedMethod = BleManagerJObject->GetClassMethod("isBleSupported", "()Z");
	IsBluetoothEnabledMethod = BleManagerJObject->GetClassMethod("isBluetoothEnabled", "()Z");
	SetBluetoothStateMethod = BleManagerJObject->GetClassMethod("setBluetoothState", "(Z)V");
	StartScanForDevicesMethod = BleManagerJObject->GetClassMethod("startScanForDevices", "([Ljava/lang/String;)V");
	StartScanForAdvertisementsMethod = BleManagerJObject->GetClassMethod("startScanForAdvertisements", "([Ljava/lang/String;)V");
	StopScanMethod = BleManagerJObject->GetClassMethod("stopScan", "()V");
}

bool UBleManager::IsBleSupported() const
{
	return BleManagerJObject->CallMethod<bool>(IsBleSupportedMethod);
}

bool UBleManager::IsBluetoothEnabled() const
{
	return BleManagerJObject->CallMethod<bool>(IsBluetoothEnabledMethod);
}

void UBleManager::SetBluetoothState(bool bState)
{
	BleManagerJObject->CallMethod<void>(SetBluetoothStateMethod, bState);
}

void UBleManager::ScanForDevices(const TArray<FString>& ServiceUUIDs, const FBleOnDeviceFoundDelegate& OnDeviceFound)
{
	if (bIsScanning)
	{
		UE_LOG(LogBleGoodies, Warning, TEXT("Scan is already in progress."));
		return;
	}

	OnDeviceFoundDelegate = OnDeviceFound;
	bIsScanning = true;

	BleManagerJObject->CallMethod<void>(StartScanForDevicesMethod,
	                                    BleJavaConvertor::ToJavaStringArray(ServiceUUIDs));
}

void UBleManager::ScanForAdvertisements(const TArray<FString>& NameFilters, const FBleOnAdvertisementDelegate& OnAdvertisement)
{
	if (bIsScanning)
	{
		UE_LOG(LogBleGoodies, Warning, TEXT("Scan is already in progress."));
		return;
	}

	OnAdvertisementDelegate = OnAdvertisement;
	bIsScanning = true;

	BleManagerJObject->CallMethod<void>(StartScanForAdvertisementsMethod,
	                                    BleJavaConvertor::ToJavaStringArray(NameFilters));
}

void UBleManager::StopScan()
{
	bIsScanning = false;
	BleManagerJObject->CallMethod<void>(StopScanMethod);
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesManager_onDeviceFound(
	JNIEnv* env, jclass clazz, jlong objAddr, jobject jDevice)
{
	UBleManager* BleManager = reinterpret_cast<UBleManager*>(objAddr);

	AsyncTask(ENamedThreads::GameThread, [JavaDevice = NewGlobalJavaObject(jDevice), BleManager]()
	{
		UBleDevice* Device = NewObject<UBleDevice>();
		Device->Init(*JavaDevice);
		BleManager->OnDeviceFound(Device);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesManager_onAdvertisement(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring deviceName, jbyteArray bytes)
{
	UBleManager* BleManager = reinterpret_cast<UBleManager*>(objAddr);
	FString DeviceName = FJavaHelper::FStringFromParam(env, deviceName);
	TArray<uint8> Data = BleJavaConvertor::ToByteArray(bytes);

	AsyncTask(ENamedThreads::GameThread, [=]()
	{
		BleManager->OnAdvertisement(DeviceName, Data);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesManager_onScanError(
	JNIEnv* env, jclass clazz, jstring errorMessage)
{
	FString ErrorMessage = FJavaHelper::FStringFromParam(env, errorMessage);
	UE_LOG(LogBleGoodies, Error, TEXT("%s"), *ErrorMessage);
}
