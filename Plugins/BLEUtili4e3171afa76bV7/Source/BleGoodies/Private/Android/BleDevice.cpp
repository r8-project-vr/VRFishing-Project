// Copyright (c) 2020 Nineva Studios

#include "BleDevice.h"

#include "Async/Async.h"
#include "BleGoodiesLog.h"
#include "Utils/BleJavaConvertor.h"

UBleDevice::~UBleDevice()
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	Env->DeleteGlobalRef(JDevice);
}

void UBleDevice::Init(jobject Device)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	JDevice = Env->NewGlobalRef(Device);

	jclass BleDeviceClass = FAndroidApplication::FindJavaClass("com/ninevastudios/ble/BleGoodiesDevice");

	jmethodID InitCallbacksMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "initCallbacks", "(J)V", false);
	Env->CallVoidMethod(JDevice, InitCallbacksMethod, (jlong)this);

	ConnectdMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "connect", "()V", false);
	DisconnectMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "disconnect", "()V", false);
	IsConnectedMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "isConnected", "()Z", false);
	GetDeviceNameMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "getName", "()Ljava/lang/String;", false);
	GetDeviceIDMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "getId", "()Ljava/lang/String;", false);
	GetServicesMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "getServicesUUIDs", "()[Ljava/lang/String;", false);
	HasServiceMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "hasService", "(Ljava/lang/String;)Z", false);
	ReadCharacteristicMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "readCharacteristic", "(Ljava/lang/String;Ljava/lang/String;)V", false);
	WriteCharacteristicMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "writeCharacteristic", "(Ljava/lang/String;Ljava/lang/String;[B)V", false);
	SubscribeToCharacteristicMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "subscribeToCharacteristic", "(Ljava/lang/String;Ljava/lang/String;Z)V", false);
	UnsubscribeFromCharacteristicMethod = FJavaWrapper::FindMethod(Env, BleDeviceClass, "unsubscribeFromCharacteristic", "(Ljava/lang/String;Ljava/lang/String;)V", false);
}

void UBleDevice::Connect(const FBleDelegate& OnConnect, const FBleErrorDelegate& OnConnectError)
{
	OnConnectDelegate = OnConnect;
	OnConnectErrorDelegate = OnConnectError;

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	Env->CallVoidMethod(JDevice, ConnectdMethod);
}

void UBleDevice::Disconnect(const FBleDelegate& OnDisconnect, const FBleErrorDelegate& OnDisconnectError)
{
	OnDisconnectDelegate = OnDisconnect;
	OnDisconnectErrorDelegate = OnDisconnectError;

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	Env->CallVoidMethod(JDevice, DisconnectMethod);
}

bool UBleDevice::IsConnected() const
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	return Env->CallBooleanMethod(JDevice, IsConnectedMethod);
}

FString UBleDevice::GetDeviceName() const
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jstring Name = static_cast<jstring>(Env->CallObjectMethod(JDevice, GetDeviceNameMethod));
	return FJavaHelper::FStringFromLocalRef(Env, Name);
}

FString UBleDevice::GetDeviceId() const
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jstring Id = static_cast<jstring>(Env->CallObjectMethod(JDevice, GetDeviceIDMethod));
	return FJavaHelper::FStringFromLocalRef(Env, Id);
}

TArray<FString> UBleDevice::GetServices(const TArray<FString>& FilterUUIDs) const
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobjectArray JArray = static_cast<jobjectArray>(Env->CallObjectMethod(JDevice, GetServicesMethod));
	TArray<FString> ServiceUUIDs = BleJavaConvertor::ToStringArray(JArray);

	if (FilterUUIDs.Num() > 0)
	{
		ServiceUUIDs = ServiceUUIDs.FilterByPredicate([&FilterUUIDs](const FString& ServiceUUID) -> bool {
			return FilterUUIDs.Contains(ServiceUUID);
		});
	}

	return ServiceUUIDs;
}

bool UBleDevice::IsServiceAvailable(const FString& ServiceUUID) const
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	auto JServiceUUID = FJavaClassObject::GetJString(ServiceUUID);
	return Env->CallBooleanMethod(JDevice, HasServiceMethod, *JServiceUUID);
}

void UBleDevice::ReadCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	auto JServiceUUID = FJavaClassObject::GetJString(ServiceUUID);
	auto JCharacteristicUUID = FJavaClassObject::GetJString(CharacteristicUUID);
	Env->CallVoidMethod(JDevice, ReadCharacteristicMethod, *JServiceUUID, *JCharacteristicUUID);
}

void UBleDevice::WriteCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, const TArray<uint8>& Data)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	auto JServiceUUID = FJavaClassObject::GetJString(ServiceUUID);
	auto JCharacteristicUUID = FJavaClassObject::GetJString(CharacteristicUUID);
	Env->CallVoidMethod(JDevice, WriteCharacteristicMethod, *JServiceUUID, *JCharacteristicUUID,
		BleJavaConvertor::ToJavaByteArray(Data));
}

void UBleDevice::SubscribeToCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID, bool bIsIndicationCharacteristic)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	auto JServiceUUID = FJavaClassObject::GetJString(ServiceUUID);
	auto JCharacteristicUUID = FJavaClassObject::GetJString(CharacteristicUUID);
	Env->CallVoidMethod(JDevice, SubscribeToCharacteristicMethod, *JServiceUUID, *JCharacteristicUUID, bIsIndicationCharacteristic);
}

void UBleDevice::UnsubscribeFromCharacteristic(const FString& ServiceUUID, const FString& CharacteristicUUID)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	auto JServiceUUID = FJavaClassObject::GetJString(ServiceUUID);
	auto JCharacteristicUUID = FJavaClassObject::GetJString(CharacteristicUUID);
	Env->CallVoidMethod(JDevice, UnsubscribeFromCharacteristicMethod, *JServiceUUID, *JCharacteristicUUID);
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onConnect(JNIEnv* env, jclass clazz, jlong objAddr)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnConnect();
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onDisconnect(JNIEnv* env, jclass clazz, jlong objAddr)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnDisconnect();
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onConnectError(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring errorMessage)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	FString ErrorMessage = FJavaHelper::FStringFromParam(env, errorMessage);

	UE_LOG(LogBleGoodies, Error, TEXT("Failed during connect: %s"), *ErrorMessage);
	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnConnectError(ErrorMessage);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onDisconnectError(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring errorMessage)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	FString ErrorMessage = FJavaHelper::FStringFromParam(env, errorMessage);

	UE_LOG(LogBleGoodies, Error, TEXT("Failed during disconnect: %s"), *ErrorMessage);
	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnDisconnectError(ErrorMessage);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onRead(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring service, jstring characteristic, jbyteArray bytes)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	FString ServiceUUID = FJavaHelper::FStringFromParam(env, service);
	FString CharacteristicUUID = FJavaHelper::FStringFromParam(env, characteristic);
	TArray<uint8> Data = BleJavaConvertor::ToByteArray(bytes);

	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnRead(ServiceUUID, CharacteristicUUID, Data);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onWrite(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring service, jstring characteristic)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	FString ServiceUUID = FJavaHelper::FStringFromParam(env, service);
	FString CharacteristicUUID = FJavaHelper::FStringFromParam(env, characteristic);

	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnWrite(ServiceUUID, CharacteristicUUID);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onNotification(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring service, jstring characteristic, jbyteArray bytes)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	FString ServiceUUID = FJavaHelper::FStringFromParam(env, service);
	FString CharacteristicUUID = FJavaHelper::FStringFromParam(env, characteristic);
	TArray<uint8> Data = BleJavaConvertor::ToByteArray(bytes);

	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnNotification(ServiceUUID, CharacteristicUUID, Data);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onUnsubscribe(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring service, jstring characteristic)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	FString ServiceUUID = FJavaHelper::FStringFromParam(env, service);
	FString CharacteristicUUID = FJavaHelper::FStringFromParam(env, characteristic);

	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnUnsubscribe(ServiceUUID, CharacteristicUUID);
	});
}

JNI_METHOD void Java_com_ninevastudios_ble_BleGoodiesDevice_onError(
	JNIEnv* env, jclass clazz, jlong objAddr, jstring service, jstring characteristic, jstring errorMessage)
{
	UBleDevice* Device = reinterpret_cast<UBleDevice*>(objAddr);
	FString ServiceUUID = FJavaHelper::FStringFromParam(env, service);
	FString CharacteristicUUID = FJavaHelper::FStringFromParam(env, characteristic);
	FString ErrorMessage = FJavaHelper::FStringFromParam(env, errorMessage);

	UE_LOG(LogBleGoodies, Error, TEXT("Characteristic operation error: %s"), *ErrorMessage);
	AsyncTask(ENamedThreads::GameThread, [=]() {
		Device->OnError(ServiceUUID, CharacteristicUUID, ErrorMessage);
	});
}
