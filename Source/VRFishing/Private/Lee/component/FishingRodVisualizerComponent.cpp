// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/component/FishingRodVisualizerComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Lee/component/FishingLineComponent.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "MotionControllerComponent.h"
#include "Tanimura/Component/FishingCatchingStateComponent.h"
#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingResultStateComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "VRFishingLog.h"

UFishingRodVisualizerComponent::UFishingRodVisualizerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 既定の竿クラス（BP 側で上書き可能）
	RodActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_FishingRod.BP_FishingRod_C")));
}

void UFishingRodVisualizerComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// --- 実行時キャッシュの構築（カメラ / 状態マネージャ / 同期元コントローラ） ---
	CachedCamera = Owner->FindComponentByClass<UCameraComponent>();
	CachedStateManager = Owner->FindComponentByClass<UFishingStateManagerComponent>();
	CachedHandController = FindHandMotionController();

	if (!CachedHandController)
	{
		UE_LOG(LogFishing, Warning, TEXT("[RodView] RodHand=%d に一致する MotionControllerComponent が見つかりません（手追従は無効）"),
			static_cast<int32>(RodHand));
	}

	// --- 手のメッシュを隠して竿を視覚の主役にする ---
	if (bHideHandMeshes)
	{
		HideHandMeshes();
	}

	// --- 竿アクタのスポーン（カメラ正面・水平前向き） ---
	SpawnRodActor();
}

void UFishingRodVisualizerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Rod = RodActor.Get();
	if (!Rod)
	{
		return;
	}

	// --- 手メッシュ隠しの再適用（BP 側の表示切替対策・可視時のみ書き込む） ---
	if (bHideHandMeshes)
	{
		HideHandMeshes();
	}

	// --- 結果状態（釣上/逃走判定後）は竿を非表示にし、準備復帰で再表示する ---
	// 釣り糸は竿の子コンポーネントのため SetActorHiddenInGame で同時に隠れる
	const UFishingStateComponentBase* CurrentState = CachedStateManager ? CachedStateManager->GetCurrentState() : nullptr;
	if (!bRodHidden && CurrentState && CurrentState->IsA<UFishingResultStateComponent>())
	{
		SetRodHidden(true);
	}
	else if (bRodHidden && CurrentState && CurrentState->IsA<UFishingReadyStateComponent>())
	{
		SetRodHidden(false);
	}

	// --- 同期元が無い場合はスポーン姿勢のまま静止 ---
	if (!CachedHandController)
	{
		return;
	}

	// --- ピッチ追従: 手のワールド Z 変位 → 1 軸（ピッチ）のみ駆動 ---
	float TargetPitchDeg = 0.0f;
	if (bEnableHeightPitch && (!bPitchOnlyDuringFishing || IsFishingPhaseActive()))
	{
		bool bHeightValid = false;
		const float HandZ = GetRodHandHeight(bHeightValid);
		if (bHeightValid)
		{
			// 中立高さの自動校正（初回有効フレーム）
			if (!bNeutralResolved)
			{
				ResolvedNeutralHeightCm = (NeutralHeightCm > 0.0f) ? NeutralHeightCm : HandZ;
				bNeutralResolved = true;
				UE_LOG(LogFishing, Log, TEXT("[RodView] ピッチ中立高さを校正: %.1f cm"), ResolvedNeutralHeightCm);
			}
			TargetPitchDeg = FMath::Clamp((HandZ - ResolvedNeutralHeightCm) * PitchDegreesPerCm,
				-MaxPitchOffsetDegrees, MaxPitchOffsetDegrees);
		}
	}

	// --- 平滑化して基礎回転へピッチのみ合成（Location はスポーン位置に固定） ---
	CurrentPitchOffsetDeg = FMath::FInterpTo(CurrentPitchOffsetDeg, TargetPitchDeg, DeltaTime, PitchSmoothSpeed);
	const FRotator FinalRotation = CachedSpawnPose.GetRotation().Rotator() + FRotator(CurrentPitchOffsetDeg, 0.0f, 0.0f);

	Rod->SetActorLocationAndRotation(CachedSpawnPose.GetLocation(), FinalRotation);
}

void UFishingRodVisualizerComponent::SpawnRodActor()
{
	UWorld* World = GetWorld();
	UClass* LoadedClass = RodActorClass.IsValid() ? RodActorClass.LoadSynchronous() : nullptr;
	if (!World || !LoadedClass)
	{
		UE_LOG(LogFishing, Warning, TEXT("[RodView] RodActorClass が無効のため竿をスポーンできません"));
		return;
	}

	// --- スポーン姿勢: カメラ正前方・水平（カメラ無し時はオーナー正面）＋左右/上下オフセット ---
	AActor* Owner = GetOwner();
	FVector CamLocation = Owner->GetActorLocation();
	FVector Forward = Owner->GetActorForwardVector();
	FVector Right = Owner->GetActorRightVector();
	if (CachedCamera)
	{
		CamLocation = CachedCamera->GetComponentLocation();
		Forward = CachedCamera->GetForwardVector();
		Right = CachedCamera->GetRightVector();
	}
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	Right.Z = 0.0f;
	Right.Normalize();

	const FVector SpawnLocation = CamLocation
		+ Forward * SpawnDistanceAhead
		+ Right * SpawnOffsetRight
		+ FVector::UpVector * SpawnOffsetUp;

	// --- 基礎回転: 正面 × 竿メッシュ軸補正（オフセットは竿ローカル軸で効く） ---
	const FQuat BaseQuat = FQuat(FRotationMatrix::MakeFromX(Forward).Rotator()) * FQuat(BaseRotationOffset);
	const FTransform SpawnTransform(BaseQuat, SpawnLocation);

	// --- 基礎姿勢を固定値としてキャッシュ（Tick はこの位置/基礎回転のみを書く。Location は動かさない） ---
	CachedSpawnPose = SpawnTransform;
	bSpawnPoseCached = true;

	// --- スポーン（衝突判定なし・オーナー Pawn をインスタンサーに指定） ---
	AActor* Rod = World->SpawnActorDeferred<AActor>(LoadedClass, SpawnTransform, Owner,
		nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Rod)
	{
		UE_LOG(LogFishing, Warning, TEXT("[RodView] 竿アクタのスポーンに失敗しました"));
		return;
	}
	Rod->FinishSpawning(SpawnTransform);
	RodActor = Rod;

	// --- 釣り糸コンポーネントの取得または自動生成 ---
	UFishingLineComponent* Line = GetOrAddLineComponent(Rod);
	UE_LOG(LogFishing, Log, TEXT("[RodView] 竿をスポーン: %s（釣り糸コンポーネント=%d）"),
		*Rod->GetName(), Line ? 1 : 0);
}

UFishingLineComponent* UFishingRodVisualizerComponent::GetOrAddLineComponent(AActor* Rod)
{
	UFishingLineComponent* Line = Rod ? Rod->FindComponentByClass<UFishingLineComponent>() : nullptr;
	if (Line || !Rod || !bAutoCreateLineComponent)
	{
		// BP_FishingRod 側へ手動配置済みならその設定を尊重する
		return Line;
	}

	// --- 実行時自動生成（BP 側の追加作業が不要な構成） ---
	Line = NewObject<UFishingLineComponent>(Rod, TEXT("FishingLineComponent"));
	Line->ApplyAutoCreatedOverrides(LineMaterial, CableWidth, HookMeshAsset, HookScale, HookRotationOffset, LineRodTipPointName);
	Line->RegisterComponent();
	Rod->AddInstanceComponent(Line);
	Line->SetComponentTickEnabled(true);
	// 登録フローより BeginPlay が遅れるケースに備えて明示呼び出し（冪等なので二重初期化にならない）
	Line->InitializeLine();
	return Line;
}

UMotionControllerComponent* UFishingRodVisualizerComponent::FindHandMotionController() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// --- 最優先: センサ（HandHeightDetector）の HandRef を使う（進行バーと同一データ経路） ---
	// BP_XRPawn では HandRef に MotionControllerRightGrip が設定されている実績あり
	if (const UHandHeightDetectorComponent* Detector = Owner->FindComponentByClass<UHandHeightDetectorComponent>())
	{
		if (UMotionControllerComponent* Hand = Cast<UMotionControllerComponent>(Detector->HandRef.Get()))
		{
			return Hand;
		}
	}

	// --- フォールバック: TrackingSource（従来の Hand 指定）と MotionSource（XR ソース名）の両方で照合 ---
	const FName ExpectedSourceName = (RodHand == EControllerHand::Left) ? TEXT("Left") : TEXT("Right");

	TArray<UMotionControllerComponent*> Controllers;
	Owner->GetComponents<UMotionControllerComponent>(Controllers);

	for (UMotionControllerComponent* Controller : Controllers)
	{
		if (Controller->GetTrackingSource() == RodHand || Controller->GetTrackingMotionSource() == ExpectedSourceName)
		{
			return Controller;
		}
	}
	return nullptr;
}

void UFishingRodVisualizerComponent::HideHandMeshes()
{
	// --- 収集は一度だけ（MotionController 配下を優先、無ければ全スケルタルメッシュ） ---
	if (!bHandMeshesResolved)
	{
		bHandMeshesResolved = true;

		const AActor* Owner = GetOwner();
		if (Owner)
		{
			TArray<USkeletalMeshComponent*> SkeletalMeshes;
			Owner->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);

			for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
			{
				// アタッチ親チェーンを辿り MotionController 配下かを判定
				bool bUnderController = false;
				const USceneComponent* Parent = Mesh->GetAttachParent();
				while (Parent)
				{
					if (Parent->IsA<UMotionControllerComponent>())
					{
						bUnderController = true;
						break;
					}
					Parent = Parent->GetAttachParent();
				}

				if (bUnderController)
				{
					HiddenHandMeshes.Add(Mesh);
				}
			}

			// フォールバック: コントローラ配下が見つからなければ全部隠す
			if (HiddenHandMeshes.Num() == 0)
			{
				for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
				{
					HiddenHandMeshes.Add(Mesh);
				}
			}
		}
	}

	// --- 可視のものだけ非表示へ戻す（Tick 毎の BP 側表示切替対策） ---
	for (const TWeakObjectPtr<USkeletalMeshComponent>& MeshWeak : HiddenHandMeshes)
	{
		USkeletalMeshComponent* Mesh = MeshWeak.Get();
		if (Mesh && Mesh->IsVisible())
		{
			Mesh->SetVisibility(false);
		}
	}
}

bool UFishingRodVisualizerComponent::IsFishingPhaseActive() const
{
	if (!CachedStateManager)
	{
		return false;
	}

	const UFishingStateComponentBase* Current = CachedStateManager->GetCurrentState();
	return Current && (Current->IsA<UFishingStateHandUpDown>() ||
		Current->IsA<UFishingReelStateComponent>() ||
		Current->IsA<UFishingCatchingStateComponent>());
}

void UFishingRodVisualizerComponent::SetRodHidden(bool bHidden)
{
	bRodHidden = bHidden;

	// --- SetActorHiddenInGame は子コンポーネント（釣り糸・針）も同時に隠す ---
	AActor* Rod = RodActor.Get();
	if (Rod)
	{
		Rod->SetActorHiddenInGame(bHidden);
	}
}

float UFishingRodVisualizerComponent::GetRodHandHeight(bool& bOutValid) const
{
	bOutValid = CachedHandController != nullptr;
	if (!CachedHandController)
	{
		return 0.0f;
	}
	// ワールド Z をそのまま使う（HMD 座標系に依存しない素直な高さ）
	return CachedHandController->GetComponentLocation().Z;
}
