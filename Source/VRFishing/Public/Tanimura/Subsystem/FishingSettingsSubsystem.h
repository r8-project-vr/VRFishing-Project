// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FishingSettingsSubsystem.generated.h"

/**
 * タイトル画面で選択した設定値をゲームプレイへ引き継ぐためのサブシステム
 */
UCLASS()
class VRFISHING_API UFishingSettingsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 回転の負荷レベル（0=Low, 1=Medium, 2=High）を設定する
    UFUNCTION(BlueprintCallable, Category = "Fishing|Settings")
    void SetRotationLoadLevel(int32 NewLevel);

    // 回転の負荷レベル（0=Low, 1=Medium, 2=High）を取得する
    UFUNCTION(BlueprintPure, Category = "Fishing|Settings")
    int32 GetRotationLoadLevel() const;

private:
    // 回転の負荷レベル（デフォルトは1=Medium）
    int32 RotationLoadLevel = 1;
};
