// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoidsFish.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UFishDataAsset;
class UBoidsFishSettings;
class ABoidsFishManager;

/**
 * Boidsの1匹分の速度と状態を持つ魚アクター
 */
UCLASS(Blueprintable)
class VRFISHING_API ABoidsFish : public AActor
{
	GENERATED_BODY()

public:
	ABoidsFish();

	// ルートコンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boids|Components")
	TObjectPtr<USceneComponent> RootScene;

	// 魚種データのメッシュを表示するコンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boids|Components")
	TObjectPtr<USkeletalMeshComponent> FishMesh;

	// 個別パラメータ。未設定なら魚種設定、それも無ければマネージャーの設定を使う
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Settings")
	TObjectPtr<UBoidsFishSettings> SettingsOverride;

	// 表示する魚種データ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Visual")
	TObjectPtr<UFishDataAsset> FishData;

	// 魚種データを設定して見た目へ反映する
	UFUNCTION(BlueprintCallable, Category = "Boids|Visual")
	void SetFishData(UFishDataAsset* InFishData);

	// 初期速度（cm/s）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float InitialSpeed = 120.0f;

	// 初期の進行方向をランダムにするか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids|Movement")
	bool bRandomizeInitialDirection = true;

	// 速度の向きへアクターを回転させるか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boids|Movement")
	bool bOrientToVelocity = true;

	// 現在の速度（cm/s）
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boids|Movement")
	FVector Velocity = FVector::ZeroVector;

	// 現在の速度を取得
	UFUNCTION(BlueprintPure, Category = "Boids|Movement")
	FVector GetBoidsVelocity() const;

	// 現在の速さを取得
	UFUNCTION(BlueprintPure, Category = "Boids|Movement")
	float GetBoidsSpeed() const;

	// 所属しているマネージャーを取得
	UFUNCTION(BlueprintPure, Category = "Boids|Movement")
	ABoidsFishManager* GetBoidsManager() const;

	// 速度を外部から設定。ランタイム生成時にも使う
	UFUNCTION(BlueprintCallable, Category = "Boids|Movement")
	void SetBoidsVelocity(const FVector& InVelocity);

	// マネージャーへ自身を登録
	UFUNCTION(BlueprintCallable, Category = "Boids|Movement")
	void RegisterWithManager(ABoidsFishManager* InManager);

	// マネージャーから自身を解除
	UFUNCTION(BlueprintCallable, Category = "Boids|Movement")
	void UnregisterFromManager();

	// マネージャーが移動更新後に呼ぶBPイベント
	UFUNCTION(BlueprintImplementableEvent, Category = "Boids|Movement")
	void OnBoidsUpdated(float DeltaTime, float Speed);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 登録先のマネージャー
	UPROPERTY(Transient)
	TObjectPtr<ABoidsFishManager> Manager;

	// 初期速度を設定済みか
	bool bVelocityInitialized = false;

	// 初期速度を一度だけ設定
	void InitializeVelocity();

	// 魚種データのメッシュ・スケール・回転を反映
	void ApplyFishData();
};
