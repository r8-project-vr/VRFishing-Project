// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Fish.generated.h"

/**
 * 魚の状態を定義する列挙型
 */
UENUM(BlueprintType)
enum class EFishState : uint8
{
	Circling		UMETA(DisplayName = "Circling"),			//周回
	MovingToCenter	UMETA(DisplayName = "Moving To Center"),	//中心移動
	Poking			UMETA(DisplayName = "Poking"),				//突く
	Struggling		UMETA(DisplayName = "Struggling"),			//暴れる
	Caught			UMETA(DisplayName = "Caught")				//釣られた
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

	//現在の状態
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Fish|State") 
	EFishState CurrentState;

	//周回運動を担当するコンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category= "Fish|Movement")
	URotatingMovementComponent* RotatingMovementComp;

	//魚の中心点
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fish|Movement") 
	FVector CenterLocation;

	//暴れるときの半径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float StruggleRadius = 30.0f;

	//暴れるときの回転速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement")
	float CircleSpeed = 1.5f;

	//仮処理、F1で暴れる
	UFUNCTION(BlueprintCallable, Category = "Fish")
	void StartStruggling();

	//外部から釣られた状態（吊り上げ）を開始させる関数
	UFUNCTION(BlueprintCallable, Category = "Fish")
	void CatchFish();

	//通知イベント
	//暴れ始めたときのエフェクト再生
	UFUNCTION(BlueprintImplementableEvent, Category = "Fish|Events")
	void OnStartStruggling();

	//釣られたとき
	UFUNCTION(BlueprintImplementableEvent, Category = "Fish|Events")
	void OnCaught();

private:
	float RunningTime = 0.0f;
	FTimerHandle StateTimerHandle;
	FTimerHandle PokeTimerHandle;

	bool bApproaching = false;
	FVector PokeTargetLocation;
	FVector CaughtTargetLocation;

	void TransitionToMoveToCenter();
	void DoPoke();
};
