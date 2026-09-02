// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "FishingRodVisualizerComponent.generated.h"

class AActor;
class UCameraComponent;
class UHandHeightDetectorComponent;
class UMaterialInterface;
class UMotionControllerComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UFishingLineComponent;
class UFishingStateManagerComponent;

/**
 * @brief 独立型の釣り竿アクタを生成し、手の高さで竿を上下に伏せる視覚演出コンポーネント。
 * @note  データ経路:
 *        BeginPlay: RodActorClass（BP_FishingRod）をカメラ正面へスポーン（水平前向きの初期姿勢）。
 *                   この位置・基礎回転を CachedSpawnPose に固定する
 *        Tick:      Location は常にスポーン位置のまま（追従しない）。
 *                   Rotation はピッチ 1 軸のみ、同期元（センサの実効手 Z＝進行バーと同一データ経路。
 *                   外部デバイス/模擬モードでは注入データの仮想 Z に追従する）の
 *                   ワールド Z 変位 × PitchDegreesPerCm で駆動（FInterpTo 平滑）。
 *                   センサが値を持つまでのフォールバックとして MotionController の実 Z も使用。
 *        両手のスケルタルメッシュは非表示化し、竿を視覚の主役にする。
 *        入力・判定系（ステートマシン/センサ/ウィジェット）には一切触れない純視覚コンポーネント。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingRodVisualizerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFishingRodVisualizerComponent();

	// --- UActorComponent overrides ---
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==================== スポーン設定 ====================

	/** @brief 生成する竿アクタのクラス（既定: /Game/Blueprints/BP_FishingRod） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView")
	TSoftClassPtr<AActor> RodActorClass;

	/** @brief 同期元の手。MotionControllerComponent の TrackingSource / MotionSource と照合する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView")
	EControllerHand RodHand = EControllerHand::Right;

	/** @brief 初回スポーン位置: カメラ前方への距離 [cm]。竿の Location はここに固定される */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView", meta = (ClampMin = "0.0"))
	float SpawnDistanceAhead = 120.0f;

	/** @brief スポーン位置の左右オフセット [cm]（カメラ右方向が +、BeginPlay 時の向き基準） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView", meta = (ClampMin = "-500.0", ClampMax = "500.0"))
	float SpawnOffsetRight = 0.0f;

	/** @brief スポーン位置の上下オフセット [cm]（上が +） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView", meta = (ClampMin = "-500.0", ClampMax = "500.0"))
	float SpawnOffsetUp = 0.0f;

	/** @brief 竿メッシュのモデル軸補正回転（竿ローカル軸で適用）。既定 Pitch=90 は縦建模（竿頭 −Z）を水平前向きへ起こす */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView")
	FRotator BaseRotationOffset = FRotator(90.0f, 0.0f, 0.0f);

	/** @brief 両手のスケルタルメッシュを非表示にする（竿のみ見せる） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView")
	bool bHideHandMeshes = true;

	// ==================== ピッチ追従設定 ====================

	/** @brief 手の高さ変位に応じた竿ピッチ追従を有効化 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Pitch")
	bool bEnableHeightPitch = true;

	/** @brief ピッチ追従を釣りフェーズ（上下/リール/釣り上げ）中のみに限定する（false で常時） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Pitch")
	bool bPitchOnlyDuringFishing = true;

	/** @brief 中立高さからの変位 1cm あたりのピッチ角 [度/cm]（手を上げると竿頭が上がる） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Pitch", meta = (ClampMin = "0.0"))
	float PitchDegreesPerCm = 1.5f;

	/** @brief ピッチ補正の上限角 [度] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Pitch", meta = (ClampMin = "0.0"))
	float MaxPitchOffsetDegrees = 45.0f;

	/** @brief ピッチ中立とする手のワールド Z 高さ [cm]。0 以下なら初回フレームの高さで自動校正 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Pitch")
	float NeutralHeightCm = 0.0f;

	/** @brief ピッチ補正の平滑化速度（FInterpTo の補間速度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Pitch", meta = (ClampMin = "0.1"))
	float PitchSmoothSpeed = 12.0f;

	// ==================== 釣り糸の代理設定（自動生成時のみ適用） ====================

	/** @brief スポーンした竿に FishingLineComponent が無い場合に自動生成する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Line")
	bool bAutoCreateLineComponent = true;

	/** @brief 自動生成した糸に適用するマテリアル（未指定ならエンジン既定） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Line")
	TObjectPtr<UMaterialInterface> LineMaterial;

	/** @brief 自動生成した糸の幅 [cm] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Line", meta = (ClampMin = "0.01"))
	float CableWidth = 0.8f;

	/** @brief 自動生成時に適用する針メッシュ（未指定なら針は表示されない） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Line")
	TObjectPtr<UStaticMesh> HookMeshAsset;

	/** @brief 針のスケール */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Line")
	FVector HookScale = FVector(1.0f);

	/** @brief 針の向きの微調整（基準: 竿先から離れる向きを +X とする） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Line")
	FRotator HookRotationOffset = FRotator::ZeroRotator;

	/** @brief 自動生成した糸に流し込む竿先基準点の共通キー（FishingLineComponent::RodTipPointName。竿側のタグ/コンポーネント名/ソケット名） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|RodView|Line")
	FName LineRodTipPointName = TEXT("RodTip");

private:
	/** @brief 竿アクタをカメラ正面へスポーンする（失敗時はログのみで続行） */
	void SpawnRodActor();

	/** @brief 竿アクタの FishingLineComponent を取得する。無くて自動生成が有効なら実行時生成する */
	UFishingLineComponent* GetOrAddLineComponent(AActor* Rod);

	/** @brief RodHand に一致するモーションコントローラを検索する（見つからなければ nullptr） */
	UMotionControllerComponent* FindHandMotionController() const;

	/** @brief 手のメッシュを非表示化する（MotionController 配下を優先、無ければ全スケルタルメッシュ） */
	void HideHandMeshes();

	/** @brief 現在が釣りフェーズ（上下運動/リール/釣り上げ）かを判定する */
	bool IsFishingPhaseActive() const;

	/** @brief 竿アクタの表示/非表示を切替える（結果状態表示中は非表示、準備復帰で再表示） */
	void SetRodHidden(bool bHidden);

	/** @brief 同期元の手のワールド Z 高さを取得する（センサ実効値優先、コントローラ実 Z フォールバック。bOutValid=同期元有効時 true） */
	float GetRodHandHeight(bool& bOutValid) const;

	// 実行時キャッシュ（カメラ / 状態マネージャ / センサ / 同期元コントローラ）
	TObjectPtr<UCameraComponent> CachedCamera = nullptr;
	TObjectPtr<UMotionControllerComponent> CachedHandController = nullptr;
	TObjectPtr<UFishingStateManagerComponent> CachedStateManager = nullptr;
	TObjectPtr<UHandHeightDetectorComponent> CachedDetector = nullptr;

	// 非表示化対象の手メッシュ（BeginPlay で一度だけ収集）
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> HiddenHandMeshes;

	// 手メッシュ収集済みフラグ
	bool bHandMeshesResolved = false;

	// 生成した竿アクタ
	TWeakObjectPtr<AActor> RodActor;

	// 竿の非表示状態（結果状態に入ったら true、Ready 復帰で false）
	bool bRodHidden = false;

	// スポーン時の基礎姿勢（Location 固定・基礎回転）。Tick ではこれを書き続ける
	FTransform CachedSpawnPose = FTransform::Identity;
	bool bSpawnPoseCached = false;

	// ピッチ追従の現在補正角 [度]（平滑化済み）
	float CurrentPitchOffsetDeg = 0.0f;

	// ピッチ中立高さ（自動校正込み）
	float ResolvedNeutralHeightCm = 0.0f;
	bool bNeutralResolved = false;
};
