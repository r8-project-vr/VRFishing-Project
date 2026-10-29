// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

// ============================================================================
// 自転車デバイス駆動テスト（FishingBicycleDriveTests.cpp）専用のヘルパ UObject。
// ※ UHT は cpp 内 UCLASS の generated.h を生成しないため、UCLASS をこちらの
//   ヘッダへ切り出している（generated.h はヘッダ側の最後の include とする）。
//   モジュール内でしか使わないため Private 配置・API マクロなし。
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FishingBicycleDriveTestsHelper.generated.h"

/**
 * @brief OnRPMCalculated（動的デリゲート）の通知を受け取るテスト用シンク。
 * @note  通知回数と最終通知値を記録し、標定 RPM がそのまま届いているかを検証する。
 */
UCLASS()
class UTestRpmSink : public UObject
{
	GENERATED_BODY()

public:
	/** @brief RPM 通知の受け口（動的デリゲート用 UFUNCTION） */
	UFUNCTION()
	void OnRpm(float NewRpm)
	{
		LastRpm = NewRpm;
		++BroadcastCount;
	}

	/** 最後に通知された RPM（未通知は -1） */
	float LastRpm = -1.0f;

	/** 通知された回数 */
	int32 BroadcastCount = 0;
};
