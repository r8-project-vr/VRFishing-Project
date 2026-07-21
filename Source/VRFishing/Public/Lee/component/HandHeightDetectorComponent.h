// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HandHeightDetectorComponent.generated.h"

class UCameraComponent;
class USceneComponent;

/** 手の移動速度の判定状態 */
UENUM(BlueprintType)
enum class EHandSpeedState : uint8
{
	Good		UMETA(DisplayName = "適正"),
	TooSlow		UMETA(DisplayName = "遅すぎ"),
	TooFast		UMETA(DisplayName = "速すぎ")
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UHandHeightDetectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHandHeightDetectorComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==================== 設定パラメータ ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection")
	float BottomOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection")
	float TopOffset = 30.0f;

	/// @brief Debug表示を有効にするかどうか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection|Debug")
	bool bShowDebug = true;

	// ==================== 移動速度 設定 ====================

	/** 適正と判定する最小速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Detection")
	float MinGoodSpeed = 5.0f;

	/** 適正と判定する最大速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Detection")
	float MaxGoodSpeed = 30.0f;

	// ==================== 出力変数 ====================

	UPROPERTY(BlueprintReadOnly, Category = "Height Detection")
	float HandHeightPercent = 0.0f;

	/** 手の現在の移動速度 (cm/s) */
	UPROPERTY(BlueprintReadOnly, Category = "Speed Detection")
	float CurrentHandSpeed = 0.0f;

	/** 手の移動速度の判定状態 */
	UPROPERTY(BlueprintReadOnly, Category = "Speed Detection")
	EHandSpeedState HandSpeedState = EHandSpeedState::Good;

	// ==================== コンポーネント参照 ====================

	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, Category = "Height Detection|References")
	TWeakObjectPtr<UCameraComponent> CameraRef;

	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, Category = "Height Detection|References")
	TWeakObjectPtr<USceneComponent> HandRef;

private:
	/** 前フレームの手のワールド位置（速度計算用） */
	FVector PreviousHandLocation = FVector::ZeroVector;

	/** PreviousHandLocation が有効か（最初のフレームは無効） */
	bool bHasPreviousLocation = false;
};
