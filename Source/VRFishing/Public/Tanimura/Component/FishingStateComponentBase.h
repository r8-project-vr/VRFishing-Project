// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingStateComponentBase.generated.h"

// 各ステート（モード）において、完了したことを通知するデリゲートの型
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFishingStateCompleted, bool, bIsSuccess);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UFishingStateComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFishingStateComponentBase();

	// ステート（モード）開始時に呼び出す
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Fishing|State")
	void EnterState();

	// ステート（モード）の更新処理（Tick相当）
	UFUNCTION(BlueprintCallable, Category = "Fishing|State")
	virtual void UpdateState(float DeltaTime);

	// ステート（モード）終了時に呼び出す
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Fishing|State")
	void ExitState();

	// デリゲートのインスタンス
	UPROPERTY(BlueprintAssignable, Category = "Fishing|Events")
	FOnFishingStateCompleted OnFishingStateCompleted;

protected:
	virtual void BeginPlay() override;

	// C++側の処理の実装用 
	virtual void EnterState_Implementation();
	virtual void ExitState_Implementation();
};
