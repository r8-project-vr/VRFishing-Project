// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Takeuchi/Actor/Fish.h"
#include "FishingLineComponent.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UFishingStateManagerComponent;

/**
 * @brief 釣り竿から釣り糸（直線メッシュ）と針を描画する純視覚コンポーネント。
 * @note  糸の描画は物理シミュレーションを行わない（2026-09-01 CableComponent 廃止。
 *          竿ピッチで竿先が高速に振り回され、verlet 縄の中央が慣性で浮き上がるムチ打ち現象が
 *          制御しきれなかったため）。細い円柱メッシュを毎フレーム「竿先→糸端」へ
 *          中点配置・Z 軸を線方向へ向け・Z スケール=長さで張る直線描画（VR レーザー手法）。
 * @note  データ経路（2026-09-01 最終仕様: 松線 → 咬合わせで接続）:
 *        毎フレーム AFish::CurrentState をポーリングして糸の接続先を決める
 *          - HookedStates（咬む/暴れる/釣り上げ前待機/釣られた）→ 竿先 ⇄ 魚の口先。
 *              口先は Cone 錐尖（コンポーネント世界変換で直接算出）を最優先とし、
 *              無ければ基準点「FishMouth」→ ActorLocation の順で代用する
 *          - それ以外（周回/誘引中・魚なし）→ 松線: 竿先から自然に垂らす
 *        糸端は毎フレーム口先へ直接ピン止め（平滑なし。竿先と同じ剛性扱い。
 *          方向平滑は Cable 物理時代のムチ打ち対策で、直線描画ではズレの原因になるため廃止）。
 *        断線判定もポーリングで行う（OnFishingStateChanged は購読しない）:
 *          同一デリゲート放送チェーン内で AFish 自身のハンドラが EscapeFish() を呼ぶため、
 *          購読者は放送順序によっては Escape 適用前の状態を読む一フレーム競合が起こる。
 *        竿先の解決はフォールバック:
 *          RodTipOverride → 基準点 RodTipPointName（タグ/コンポーネント名/メッシュソケットの共通キー。
 *            モデル側に事前配置したマーカーが最優先の実用経路。検出不要で精確に結線できる）
 *          → FallbackTipOffset（ゼロならメッシュ境界から自動推定）
 *        断線演出は「糸+針を即非表示」のみ（次セットの準備状態で復帰）。
 *        BP_FishingRod へ手動追加して使用する。ゲーム判定には一切関与しない。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingLineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFishingLineComponent();

	// --- UActorComponent overrides ---
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** @brief 糸と針のコンポーネントを生成する（冪等。BeginPlay と実行時自動生成直後の両方から呼ばれる） */
	void InitializeLine();

	/** @brief 実行時自動生成時に Visualizer 側の代理設定を適用する（InitializeLine より前に呼ぶこと） */
	void ApplyAutoCreatedOverrides(UMaterialInterface* InLineMaterial, float InCableWidth, UStaticMesh* InHookMeshAsset, const FVector& InHookScale, const FRotator& InHookRotationOffset, FName InRodTipPointName);

	// ==================== 竿先の解決 ====================

	/** @brief 竿先コンポーネントの明示指定（第1層）。未指定なら Tag → 自動推定の順で解決する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line")
	TObjectPtr<USceneComponent> RodTipOverride;

	/** @brief 竿先オフセットの明示指定 [cm]・ルート基準（第3層）。ゼロならメッシュ境界から自動推定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line")
	FVector FallbackTipOffset = FVector::ZeroVector;

	// ==================== 基準点マーカー（事前配置・検出不要の精確結線） ====================

	/** @brief 竿先基準点の共通キー。竿アクタ上の「タグ/コンポーネント名/メッシュソケット」をこの名前で探す（RodTipOverride 次点の第2層） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line")
	FName RodTipPointName = TEXT("RodTip");

	// ==================== 釣り糸の描画（直線メッシュ） ====================

	/** @brief 糸の直線描画に使うメッシュ。未指定なら /Engine/BasicShapes/Cylinder（直径/高さ 100cm 基準） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line")
	TObjectPtr<UStaticMesh> LineMeshAsset;

	/** @brief 糸に適用するマテリアル（未指定ならエンジン既定） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line")
	TObjectPtr<UMaterialInterface> LineMaterial;

	/** @brief 糸（直線円柱）の直径 [cm] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line", meta = (ClampMin = "0.01"))
	float CableWidth = 0.8f;

	// ==================== 松線（咬み合わせ前の自然な垂れ） ====================

	/** @brief 松線時の垂らし前方向距離 [cm]（竿の前方基準。0 で竿先の真下に垂らす） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line", meta = (ClampMin = "0.0"))
	float SlackForward = 0.0f;

	/** @brief 松線時の垂らし下方向距離 [cm] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line", meta = (ClampMin = "0.0"))
	float SlackDrop = 80.0f;

	/** @brief 糸を魚の口へ接続する魚の状態一覧（咬む/暴れる/釣り上げ前待機/釣られた） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line")
	TArray<EFishState> HookedStates = { EFishState::Poking, EFishState::Struggling, EFishState::CatchDelay, EFishState::Caught };

	/** @brief 糸端目標位置の平滑速度（VInterpTo）。暴れ期の口先の振れを線の遅れとして吸収する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line", meta = (ClampMin = "0.1"))
	float LineEndInterpSpeed = 8.0f;

	/** @brief 口先方向の平滑速度（VInterpTo）。暴れの首振りを平均化して糸端の振れを抑える（小さいほど穏やか） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line", meta = (ClampMin = "0.1"))
	float MouthDirInterpSpeed = 2.0f;

	// ==================== 針（任意） ====================

	/** @brief 針のメッシュ。未指定なら針は表示されない */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line|Hook")
	TObjectPtr<UStaticMesh> HookMeshAsset;

	/** @brief 針のスケール */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line|Hook")
	FVector HookScale = FVector(1.0f);

	/** @brief 針の向きの微調整（基準: 竿先から離れる向きを +X とする） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|Line|Hook")
	FRotator HookRotationOffset = FRotator::ZeroRotator;

private:
	/** @brief 竿先のルート相対位置を解決する（RodTipOverride → 基準点マーカー → FallbackTipOffset → メッシュ境界自動推定。各層でログ出力） */
	void ResolveRodTip(FVector& OutTipLocal);

	/** @brief アクター上の基準点を「タグ → コンポーネント名 → メッシュソケット」の順に探し、ワールド座標を返す（ChildActor 内部も下掘り） */
	static bool FindNamedPointWorld(const AActor* Actor, FName PointName, FVector& OutWorld);

	/** @brief ワールド上の最初の生存魚を取得する（いなければ nullptr） */
	AFish* FindFish() const;

	/** @brief 魚の口先（Cone 錐体の先端）ワールド位置を取得する。Cone が無ければ false */
	static bool GetFishConeTipLocation(const AFish* Fish, FVector& OutTipWorld);

	/** @brief プレイヤー Pawn の状態マシンマネージャーを遅延取得する */
	UFishingStateManagerComponent* FindStateManager();

	/** @brief 断線演出: 糸と針を即非表示にする */
	void BreakLine();

	/** @brief 糸と針を再表示して松線へ戻す */
	void RestoreLine();

	// 実行時生成の糸（直線メッシュ）と針
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LineMeshComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HookMeshComp = nullptr;

	// BeginPlay で解決した竿先のルート相対位置 [cm]
	FVector ResolvedTipLocal = FVector::ZeroVector;

	// 平滑後の糸端目標位置（ワールド）。初回と断線復帰直後は目標へ即座に貼り付ける
	FVector SmoothedEndWorld = FVector::ZeroVector;
	bool bEndSnapped = false;

	// 平滑後の口先方向（魚中心→口の単位ベクトル）。暴れの高速な首振りを平均化するためゆっくり追従させる
	FVector SmoothedMouthDir = FVector::ForwardVector;
	bool bMouthDirValid = false;

	// 方向平滑の追跡対象（魚が入れ替わったら方向を貼り直す）
	TWeakObjectPtr<AActor> TrackedFishForMouth;

	// 糸/針の生成済みフラグ（InitializeLine の冪等ガード）
	bool bLineInitialized = false;

	// 断線中フラグ（準備状態へ戻ったら復帰）
	bool bBroken = false;

	// 遅延取得した状態マネージャー
	TWeakObjectPtr<UFishingStateManagerComponent> CachedStateManager;
};
