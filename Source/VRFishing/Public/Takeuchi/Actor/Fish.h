// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Fish.generated.h"

// 2026.07.27 谷村　startーーーーーーーーーー
// 前方宣言を追加
class UFishingStateComponentBase;
// 2026.07.27 谷村　endーーーーーーーーーー
class UFishingStateManagerComponent;

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
	Escape			UMETA(DisplayName = "Escape"),				//逃げる
	CatchDelay		UMETA(DisplayName = "Catch Delay"),		//釣り上げ前待機
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

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	//中央へ移動する速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Movement", meta = (ClampMin = "0.0"))
	float MoveToCenterSpeed = 2.0f;

	//釣り上げる高さ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Caught", meta = (ClampMin = "0.0"))
	float CaughtHeight = 300.0f;

	//釣り上げる速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Caught", meta = (ClampMin = "0.0"))
	float CaughtMoveSpeed = 3.0f;

	//暴れる速度の倍率
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Struggle", meta = (ClampMin = "0.0"))
	float StruggleSpeedMultiplier = 4.0f;

	//リール完了後から釣り上げ開始までの待機時間
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Caught", meta = (ClampMin = "0.0"))
	float PreCatchingWaitTime = 1.0f;

	//逃げる距離
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Escape", meta = (ClampMin = "0.0"))
	float EscapeDistance = 500.0f;

	//逃げる速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fish|Escape", meta = (ClampMin = "0.0"))
	float EscapeSpeed = 3.0f;

	//リール操作開始時に、魚が暴れる状態へ切り替える
	UFUNCTION(BlueprintCallable, Category = "Fish")
	void StartStruggling();

	//吊り上げ状態に切り替える
	UFUNCTION(BlueprintCallable, Category = "Fish")
	void CatchFish();

	//魚が逃げる状態へ切り替える
	UFUNCTION(BlueprintCallable, Category = "Fish")
	void EscapeFish();

	//通知イベント
	//暴れ始めたときのエフェクト再生
	UFUNCTION(BlueprintImplementableEvent, Category = "Fish|Events")
	void OnStartStruggling();


private:
	FTimerHandle PokeTimerHandle;
	FTimerHandle PreCatchingTimerHandle;

	bool bApproaching = false;

	float StruggleElapsedTime = 0.0f;
	float StruggleStartAngle = 0.0f;
	float StruggleCurrentRadius = 0.0f;
	FVector PokeTargetLocation;
	FVector CaughtTargetLocation;

	FVector EscapeTargetLocation;
	FVector EscapeStartLocation;
	FVector EscapeStartScale;

	//ステート変更デリゲートの登録先を保持する
	UPROPERTY(Transient)
	TObjectPtr<UFishingStateManagerComponent> BoundStateManager;

	//釣り上げ完了デリゲートの登録先を保持する
	UPROPERTY(Transient)
	TObjectPtr<UFishingStateComponentBase> BoundCatchingState;

	void TransitionToMoveToCenter();
	void DoPoke();
	void StartCatchDelay();
	void RotateTowardCenter(float DeltaTime, float InterpSpeed);
	void ClearStateTimers();

	// 2026.07.27 谷村　startーーーーーーーーーー
	// マネージャーからの状態変更通知を受け取るハンドラー関数
	UFUNCTION()
	void OnFishingStateChanged(UFishingStateComponentBase* NewState);
	// 2026.07.27 谷村　endーーーーーーーーーー
	UFUNCTION()
	void OnCatchingStateCompleted(bool bIsSuccess);
};
