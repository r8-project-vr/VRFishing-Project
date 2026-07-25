// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingStateComponentBase.generated.h"

// 各ステート（モード）において、完了したことを通知するデリゲートの型
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFishingStateCompleted);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UFishingStateComponentBase : public UActorComponent
{
	GENERATED_BODY()

public:	
	UFishingStateComponentBase();

	// ステート開始時に呼び出す関数
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Fishing|State")
	void EnterState();

	// ステート終了時に呼び出す関数
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Fishing|State")
	void ExitState();

	// ステート完了時に発火するイベント
	UPROPERTY(BlueprintAssignable, Category = "Fishing|State")
	FOnFishingStateCompleted OnFishingStateCompleted;

protected:
	virtual void BeginPlay() override;

	// C++ 側の Enter/Exit 処理の実装用 
	virtual void EnterState_Implementation();
	virtual void ExitState_Implementation();
};
