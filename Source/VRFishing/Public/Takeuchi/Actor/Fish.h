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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fish|Movement")
	FVector CenterLocation;

	//釣り上げ完了から魚を初期位置へ再生成するまでの時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Respawn", meta = (ClampMin = "0.0"))
	float RespawnDelay = 3.0f;

	//暴れるときの円運動の半径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0.0"))
	float StruggleRadius = 30.0f;

	//暴れるときの円運動の角速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0.0"))
	float CircleSpeed = 1.5f;

	//中央から離れるときに、左右へずれる最大角度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxRetreatSideAngle = 60.0f;

	//中央へつつきに行く速さ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Poking", meta = (ClampMin = "0.0"))
	float PokeApproachSpeed = 4.0f;

	//つついた後に離れる速さ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Poking", meta = (ClampMin = "0.0"))
	float PokeRetreatSpeed = 1.5f;

	//接近・後退を切り替える最小時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Poking", meta = (ClampMin = "0.0"))
	float PokeIntervalMin = 2.0f;

	//接近・後退を切り替える最大時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Poking", meta = (ClampMin = "0.0"))
	float PokeIntervalMax = 4.0f;

	//中央から離れる最小距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Poking", meta = (ClampMin = "0.0"))
	float PokeRetreatDistanceMin = 50.0f;

	//中央から離れる最大距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Poking", meta = (ClampMin = "0.0"))
	float PokeRetreatDistanceMax = 80.0f;

	//魚のActor原点から口先までの距離。口先が中央で止まるように使用する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Poking", meta = (ClampMin = "0.0"))
	float PokeContactOffset = 60.0f;

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
	FTimerHandle StateTimerHandle;
	FTimerHandle PokeTimerHandle;
	FTimerHandle RespawnTimerHandle;

	bool bApproaching = false;
	bool bRespawnScheduled = false;
	float StruggleElapsedTime = 0.0f;
	float StruggleStartAngle = 0.0f;
	float StruggleCurrentRadius = 0.0f;
	FVector PokeTargetLocation;
	FVector CaughtTargetLocation;
	FTransform InitialSpawnTransform;

	void TransitionToMoveToCenter();
	void DoPoke();
	void RespawnFish();
	void RotateTowardCenter(float DeltaTime, float InterpSpeed);
	void ClearStateTimers();
};
