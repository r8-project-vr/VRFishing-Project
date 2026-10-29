// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/Box.h"
#include "BoidsFishManager.generated.h"

class USceneComponent;
class UBoxComponent;
class UBoidsFishSettings;
class UBoidsFishSettingsTable;
class UFishDataAsset;
class ABoidsFish;

/**
 * 魚群全体の操舵を1つのTickで計算するマネージャー
 */
UCLASS(Blueprintable)
class VRFISHING_API ABoidsFishManager : public AActor
{
	GENERATED_BODY()

public:
	ABoidsFishManager();

	// ルートコンポーネント
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boids|Components")
	TObjectPtr<USceneComponent> RootScene;

	// 魚が泳ぐ範囲。この箱の内側へ押し戻す
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boids|Components")
	TObjectPtr<UBoxComponent> BoundsVolume;

	// 既定の操舵パラメータ。未指定なら実行時に既定値で生成する
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Settings")
	TObjectPtr<UBoidsFishSettings> DefaultSettings;

	// 魚種ごとの操舵パラメータ。未登録の魚種はDefaultSettingsを使う
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Settings")
	TObjectPtr<UBoidsFishSettingsTable> SettingsTable;

	// 保存済みの釣果から生成する魚クラス
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Spawn")
	TSubclassOf<ABoidsFish> FishClass;

	// BeginPlayで保存済みの釣果から魚を生成するか
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Spawn")
	bool bSpawnFromCatchSave = true;

	// 生成する魚の全体上限。魚種ごとの上限はSettingsTableで指定する
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Spawn", meta = (ClampMin = "0"))
	int32 MaxSpawnCount = 100;

	// 境界から内側へどれだけ離して生成するか（cm）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnMargin = 150.0f;

	// BeginPlayでレベル内の魚を自動登録するか
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Simulation")
	bool bAutoRegisterAgents = true;

	// 1フレームの最大DeltaTime。ヒッチ時の過大な移動を防ぐ
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Simulation", meta = (ClampMin = "0.001", ClampMax = "0.2"))
	float MaxDeltaTime = 0.05f;

	// デバッグ描画を有効にするか
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Debug")
	bool bDrawDebug = false;

	// デバッグ描画する魚の数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boids|Debug", meta = (ClampMin = "0", ClampMax = "100"))
	int32 DebugDrawAgentCount = 10;

	// 魚を登録。通常は魚側のRegisterWithManagerから呼ばれる
	void RegisterAgent(ABoidsFish* Agent);

	// 魚を解除
	void UnregisterAgent(ABoidsFish* Agent);

	// 登録中の魚の数を取得
	UFUNCTION(BlueprintPure, Category = "Boids")
	int32 GetAgentCount() const;

	// 保存済みの釣果から魚を生成
	UFUNCTION(BlueprintCallable, Category = "Boids|Spawn")
	void SpawnFishFromCatchSave();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 登録中の魚
	UPROPERTY(Transient)
	TArray<TObjectPtr<ABoidsFish>> Agents;

	// 毎フレーム再利用する計算バッファ
	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	TArray<FVector> Accelerations;
	TArray<const UBoidsFishSettings*> AgentSettings;
	TArray<const UFishDataAsset*> AgentSpecies;

	// 1フレーム分の群れ更新
	void UpdateFlock(float DeltaTime);

	// レベル内の魚を検索して登録
	void CollectAgents();

	// 3ルールと境界の力を合成して加速度を返す。結合と整列は同種のみ、分離は全魚種を対象にする
	FVector ComputeSteering(int32 AgentIndex, const UBoidsFishSettings& Settings, const FBox& Bounds) const;

	// 箱の内側へ戻す力を返す
	FVector ComputeBoundaryForce(const FVector& Position, const UBoidsFishSettings& Settings, const FBox& Bounds) const;

	// 魚ごとの設定を解決
	const UBoidsFishSettings* ResolveSettings(const ABoidsFish* Agent) const;

	// 現在の遊泳範囲を取得
	FBox GetBounds() const;

	// 境界内のランダムな生成位置を返す
	FVector GetRandomSpawnLocation(const FBox& Bounds, float Margin) const;

#if ENABLE_DRAW_DEBUG
	// 境界と速度を描画
	void DrawDebugInfo() const;
#endif
};
