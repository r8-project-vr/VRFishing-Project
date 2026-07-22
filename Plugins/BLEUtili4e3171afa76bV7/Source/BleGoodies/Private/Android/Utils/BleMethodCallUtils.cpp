// Copyright (c) 2018 Nineva Studios

#include "BleMethodCallUtils.h"

namespace BleMethodCallUtils
{
void CallStaticVoidMethod(const ANSICHAR* ClassName, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, ...)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jclass Class = FAndroidApplication::FindJavaClass(ClassName);
	jmethodID Method = FJavaWrapper::FindStaticMethod(Env, Class, MethodName, MethodSignature, false);

	va_list Args;
	va_start(Args, MethodSignature);
	Env->CallStaticVoidMethodV(Class, Method, Args);
	va_end(Args);

	Env->DeleteLocalRef(Class);
}

bool CallStaticBoolMethod(const ANSICHAR* ClassName, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, ...)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jclass Class = FAndroidApplication::FindJavaClass(ClassName);
	jmethodID Method = FJavaWrapper::FindStaticMethod(Env, Class, MethodName, MethodSignature, false);

	va_list Args;
	va_start(Args, MethodSignature);
	bool Result = Env->CallStaticBooleanMethodV(Class, Method, Args);
	va_end(Args);

	Env->DeleteLocalRef(Class);

	return Result;
}
}
