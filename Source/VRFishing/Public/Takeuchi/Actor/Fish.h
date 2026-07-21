// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Fish.generated.h"

UENUM(BlueprintType)
enum class EFishType : uint8
{
	Circling		UMETA(DisplayName = "Circling"),
	MovingToCenter	UMETA(DisplayName = "Moving To Center"),
	Poking			UMETA(DisplayName = "Poking"),
	Struggling		UMETA(DisplayName = "Struggling"),	
	Caught			UMETA(DisplayName = "Caught")
};
UCLASS()
class VRFISHING_API AFish : public AActor
{
	GENERATED_BODY()
	
public:	
	AFish();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Fish|State") 
	EFishType CurrentState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish|Movement") 
	FVector CenterLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float CircleRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float StruggleRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float CircleSpeed = 1.5f;

	UFUNCTION(BlueprintCallable, Category = "Fish")
	void StartStruggling();

	UFUNCTION(BlueprintImplementableEvent, Category = "Fish|Events")
	void OnStartStruggling();

	UFUNCTION(BlueprintImplementableEvent, Category = "Fish|Events")
	void OnCaught();

private:
	float RunningTime = 0.0f;
	FTimerHandle StateTimerHandle;
	FTimerHandle PokeTimerHandle;

	bool bApproaching = false;
	FVector PokeTargetLocation;

	void TransitionToMoveToCenter();
	void DoPoke();
	void SetupInput();
};
