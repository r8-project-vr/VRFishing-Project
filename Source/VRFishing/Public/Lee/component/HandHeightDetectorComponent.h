// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HandHeightDetectorComponent.generated.h"

class UCameraComponent;
class USceneComponent;


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

	// ==================== 出力変数 ====================

	UPROPERTY(BlueprintReadOnly, Category = "Height Detection")
	float HandHeightPercent = 0.0f;

	// ==================== コンポーネント参照 ====================

	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, Category = "Height Detection|References")
	TWeakObjectPtr<UCameraComponent> CameraRef;

	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, Category = "Height Detection|References")
	TWeakObjectPtr<USceneComponent> HandRef;
};
