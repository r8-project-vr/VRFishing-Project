#pragma once

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidJava.h"

/**
* Create global object reference and automatically deletes when out of scope
 */
template <typename T>
class UBleGoodiesAndroidScopedObject
{
public:
	UBleGoodiesAndroidScopedObject(const T& InObjRef)
		: ObjRef(InObjRef)
	{
		check(ObjRef);

		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		check(Env);

		ObjRef = (T)Env->NewGlobalRef(ObjRef);
	}

	UBleGoodiesAndroidScopedObject(UBleGoodiesAndroidScopedObject&& Other)
		: ObjRef(Other.ObjRef)
	{
		Other.ObjRef = nullptr;
	}

	UBleGoodiesAndroidScopedObject(const UBleGoodiesAndroidScopedObject& Other) = delete;
	UBleGoodiesAndroidScopedObject& operator=(const UBleGoodiesAndroidScopedObject& Other) = delete;

	~UBleGoodiesAndroidScopedObject()
	{
		if (*this)
		{
			JNIEnv* Env = FAndroidApplication::GetJavaEnv();
			Env->DeleteGlobalRef(ObjRef);
		}
		ObjRef = nullptr;
	}

	operator bool() const
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (!Env || !ObjRef || Env->IsSameObject(ObjRef, NULL))
		{
			return false;
		}

		return true;
	}

	// Returns the underlying JNI pointer
	T operator*() const { return ObjRef; }

private:
	T ObjRef = nullptr;
};

/**
 Helper function that allows template deduction on the java object type, for example:
 auto ScopeObject = NewScopedJavaObject(Env, JavaString);
 instead of FScopedJavaObject<jstring> ScopeObject(Env, JavaString);
 */
template <typename T>
UBleGoodiesAndroidScopedObject<T> NewGlobalJavaObject(const T& InObjRef)
{
	return UBleGoodiesAndroidScopedObject<T>(InObjRef);
}
