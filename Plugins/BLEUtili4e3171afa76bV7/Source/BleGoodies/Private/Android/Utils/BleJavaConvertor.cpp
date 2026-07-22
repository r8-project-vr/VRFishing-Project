// Copyright (c) 2018 Nineva Studios

#include "BleJavaConvertor.h"

namespace BleJavaConvertor
{
jobjectArray ToJavaStringArray(const TArray<FString>& StringArray)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobjectArray JStringArray = (jobjectArray)Env->NewObjectArray(StringArray.Num(), FJavaWrapper::JavaStringClass, nullptr);

	for (int i = 0; i < StringArray.Num(); i++)
	{
		auto JString = FJavaClassObject::GetJString(StringArray[i]);
		Env->SetObjectArrayElement(JStringArray, i, *JString);
	}

	return JStringArray;
}

jbyteArray ToJavaByteArray(const TArray<uint8>& Data)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	const int DataLength = Data.Num();
	jbyteArray LocalArray = Env->NewByteArray(DataLength);

	jbyte* Buffer = new jbyte[DataLength];
	for (int i = 0; i < DataLength; i++)
	{
		Buffer[i] = Data[i];
	}

	Env->SetByteArrayRegion(LocalArray, 0, DataLength, Buffer);
	delete[] Buffer;
	jbyteArray ResultArray = (jbyteArray)Env->NewGlobalRef(LocalArray);
	Env->DeleteLocalRef(LocalArray);

	return ResultArray;
}

TArray<FString> ToStringArray(jobjectArray JavaArray)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	const int ArrayLength = Env->GetArrayLength(JavaArray);

	TArray<FString> StringArray;
	for (int i = 0; i < ArrayLength; i++)
	{
		jstring JavaString = static_cast<jstring>(Env->GetObjectArrayElement(JavaArray, i));
		StringArray.Add(FJavaHelper::FStringFromLocalRef(Env, JavaString));
	}

	return StringArray;
}

TArray<uint8> ToByteArray(jbyteArray JavaArray)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	const int DataLength = Env->GetArrayLength(JavaArray);
	jbyte* JavaBytes = Env->GetByteArrayElements(JavaArray, 0);

	TArray<uint8> ByteArray;
	for (int i = 0; i < DataLength; i++)
	{
		ByteArray.Add(JavaBytes[i]);
	}
	return ByteArray;
}
}
