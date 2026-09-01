// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/component/FishingLineComponent.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "Tanimura/Component/FishingResultStateComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "VRFishingLog.h"

// 魚側の基準点キー（BP_Fish 上のコンポーネント名/タグ/メッシュソケット名）。
// Actor の root 位置はメッシュの見た目とズレるため、視口で魚の口へ配置した基準点を優先して使う
static const FName FishMouthPointName(TEXT("FishMouth"));

// Basic Cone の形状定数（/Engine/BasicShapes/Cone: 高さ 100cm・原点は中心・先端はローカル +Z 50cm）
static constexpr float BasicConeHalfHeight = 50.0f;

UFishingLineComponent::UFishingLineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFishingLineComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeLine();
}

void UFishingLineComponent::InitializeLine()
{
	// --- 冪等ガード（BeginPlay と実行時自動生成の二重呼び出し対策） ---
	if (bLineInitialized)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		UE_LOG(LogFishing, Warning, TEXT("[LineView] オーナーまたはルートが無いため釣り糸を初期化できません"));
		return;
	}

	// --- 竿先の解決（明示指定 → 基準点マーカー → オフセット/自動推定。各層でログ出力） ---
	ResolveRodTip(ResolvedTipLocal);

	// --- 糸（直線メッシュ）を実行時生成: 竿ルートへ追従させ、竿先から伸ばす。
	//     CableComponent の物理シミュレーションは竿ピッチ操作でのムチ打ち（線が空へ舞う）が
	//     制御しきれないため廃止。毎フレーム「竿先→糸端」へ直線として張る ---
	LineMeshComp = NewObject<UStaticMeshComponent>(Owner, TEXT("FishingLineMesh"));
	LineMeshComp->SetupAttachment(Owner->GetRootComponent());
	LineMeshComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	LineMeshComp->SetRelativeLocation(ResolvedTipLocal);
	UStaticMesh* ResolvedLineMesh = LineMeshAsset ? LineMeshAsset.Get()
		: LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	LineMeshComp->SetStaticMesh(ResolvedLineMesh);
	LineMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LineMeshComp->SetCastShadow(false);
	if (LineMaterial)
	{
		LineMeshComp->SetMaterial(0, LineMaterial);
	}
	LineMeshComp->RegisterComponent();
	Owner->AddInstanceComponent(LineMeshComp);

	// --- 針（任意資産。未指定なら生成しない） ---
	if (HookMeshAsset)
	{
		HookMeshComp = NewObject<UStaticMeshComponent>(Owner, TEXT("FishingHookMesh"));
		HookMeshComp->SetupAttachment(Owner->GetRootComponent());
		HookMeshComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		HookMeshComp->SetStaticMesh(HookMeshAsset);
		HookMeshComp->SetWorldScale3D(HookScale);
		HookMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HookMeshComp->RegisterComponent();
		Owner->AddInstanceComponent(HookMeshComp);
	}

	UE_LOG(LogFishing, Log, TEXT("[LineView] 初期化完了: 竿先ローカル=(%.0f, %.0f, %.0f) 針=%d"),
		ResolvedTipLocal.X, ResolvedTipLocal.Y, ResolvedTipLocal.Z,
		HookMeshComp ? 1 : 0);

	bLineInitialized = true;
}

void UFishingLineComponent::ApplyAutoCreatedOverrides(UMaterialInterface* InLineMaterial, float InCableWidth, UStaticMesh* InHookMeshAsset, const FVector& InHookScale, const FRotator& InHookRotationOffset, FName InRodTipPointName)
{
	// 実行時自動生成されたインスタンスのみに流し込む代理設定（BP 手動配置時は呼ばれない）
	LineMaterial = InLineMaterial;
	CableWidth = InCableWidth;
	HookMeshAsset = InHookMeshAsset;
	HookScale = InHookScale;
	HookRotationOffset = InHookRotationOffset;
	RodTipPointName = InRodTipPointName;
}

void UFishingLineComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!LineMeshComp)
	{
		return;
	}

	// --- 状態取得（ポーリング専用。デリゲート購読は Escape 適用の同一フレーム競合を避けるため行わない） ---
	AFish* Fish = FindFish();
	UFishingStateManagerComponent* Manager = FindStateManager();
	UFishingStateComponentBase* Current = Manager ? Manager->GetCurrentState() : nullptr;

	// --- 断線判定: 魚が Escape 状態 or 結果状態が失敗（二重救済で全逃走経路を覆盖） ---
	if (!bBroken &&
		((Fish && Fish->CurrentState == EFishState::Escape) ||
			(Current && Current->IsA<UFishingResultStateComponent>() && !Current->IsSuccessState())))
	{
		BreakLine();
	}

	// --- 復帰判定: 準備状態へ戻ったら糸を復活（次セット） ---
	if (bBroken && Current && Current->IsA<UFishingReadyStateComponent>())
	{
		RestoreLine();
	}

	// --- 断線中は非表示のまま何もしない ---
	if (bBroken)
	{
		return;
	}

	// --- 接続先の決定: 咬み合わせ（HookedStates）中のみ魚の口先へ接続、それ以外は松線で垂らす ---
	// 竿先は竿ルート変換 × 解決済み竿先ローカルで確定的に求める（直線メッシュは中点に置かれるため
	// コンポーネント位置からは読めない。竿のピッチ回転にも自動的に追従する）
	const FVector TipWorld = GetOwner()->GetRootComponent()->GetComponentTransform().TransformPosition(ResolvedTipLocal);
	const bool bHooked = Fish && HookedStates.Contains(Fish->CurrentState);

	FVector TargetEndWorld;
	if (bHooked)
	{
		// --- 口先の解決（毎フレーム直読・平滑なし）:
		//     1) FishMouth 基準点（BP_Fish のビューポートで魚の口へ直接配置したマーカー。
		//        ユーザー指定の追跡点。ドラッグで微調整すればそのまま糸端に反映される）
		//     2) Cone 錐尖（基準点未設置の魚種用の代替） 3) ActorLocation。
		//     かつては Cable 物理のムチ打ち対策で方向平滑を入れていたが、直線描画化で
		//     追従しきれない物理はもう無く、平滑は「口先からずれる」原因でしかないため廃止 ---
		FVector MouthWorld;
		if (!FindNamedPointWorld(Fish, FishMouthPointName, MouthWorld) &&
			!GetFishConeTipLocation(Fish, MouthWorld))
		{
			static bool bWarnedNoFishMarker = false;
			if (!bWarnedNoFishMarker)
			{
				bWarnedNoFishMarker = true;
				UE_LOG(LogFishing, Warning, TEXT("[LineView] 魚の Cone/基準点「%s」が見つからないため ActorLocation へ代用"), *FishMouthPointName.ToString());
			}
			MouthWorld = Fish->GetActorLocation();
		}
		TargetEndWorld = MouthWorld;
	}
	else
	{
		// --- 松線: 竿先から前方オフセット＋真下へ垂らす（SlackForward=0 で竿先の真下） ---
		const FVector RodForward = GetOwner()->GetActorForwardVector();
		TargetEndWorld = TipWorld + RodForward * SlackForward + FVector::DownVector * SlackDrop;
	}

	// --- 糸端は毎フレーム目標へ直接貼り付ける（平滑は物理縄時代の遺物で、直線描画では
	//     口先からのズレの原因にしかならないため廃止。竿先と同じ扱い=剛性ピン止め） ---
	SmoothedEndWorld = TargetEndWorld;

	// --- デバッグ: 接続モードと糸端の整合確認用（0.5 秒間隔。整合確認後に削除可） ---
	static float LogAccumulator = 0.0f;
	LogAccumulator += DeltaTime;
	if (LogAccumulator >= 0.5f)
	{
		LogAccumulator = 0.0f;
		if (Fish)
		{
			const FVector FishLoc = Fish->GetActorLocation();
			UE_LOG(LogFishing, Log, TEXT("[LineView] %s 状態=%d 糸端=(%.0f,%.0f,%.0f) 魚=(%.0f,%.0f,%.0f)"),
				bHooked ? TEXT("接続") : TEXT("松線"), static_cast<int32>(Fish->CurrentState),
				SmoothedEndWorld.X, SmoothedEndWorld.Y, SmoothedEndWorld.Z,
				FishLoc.X, FishLoc.Y, FishLoc.Z);
		}
		else
		{
			UE_LOG(LogFishing, Log, TEXT("[LineView] 松線（魚なし） 糸端=(%.0f,%.0f,%.0f)"),
				SmoothedEndWorld.X, SmoothedEndWorld.Y, SmoothedEndWorld.Z);
		}
	}

	// --- 直線メッシュを竿先→糸端へ張る: 位置=中点、Z 軸を線方向へ、Z スケール=長さ。
	//     エンジン既定 Cylinder は直径/高さとも 100cm 基準のため 1/100 スケールで調整 ---
	const FVector LineDir = (SmoothedEndWorld - TipWorld).GetSafeNormal();
	const float LineLength = FVector::Dist(TipWorld, SmoothedEndWorld);
	if (!LineDir.IsNearlyZero())
	{
		LineMeshComp->SetWorldLocationAndRotation(TipWorld + LineDir * (LineLength * 0.5f),
			FRotationMatrix::MakeFromZ(LineDir).Rotator());
		LineMeshComp->SetWorldScale3D(FVector(CableWidth * 0.01f, CableWidth * 0.01f,
			FMath::Max(LineLength, 1.0f) * 0.01f));
	}

	// --- 針を糸の末端へ置く（向きは線の延伸方向 +X 基準） ---
	if (HookMeshComp)
	{
		HookMeshComp->SetWorldLocation(SmoothedEndWorld);
		const FVector LineDirection = (SmoothedEndWorld - TipWorld).GetSafeNormal();
		if (!LineDirection.IsNearlyZero())
		{
			HookMeshComp->SetWorldRotation(FRotationMatrix::MakeFromX(LineDirection).Rotator() + HookRotationOffset);
		}
	}
}

void UFishingLineComponent::ResolveRodTip(FVector& OutTipLocal)
{
	OutTipLocal = FVector::ZeroVector;
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Owner || !Root)
	{
		return;
	}

	// --- 第1層: 明示指定 ---
	if (RodTipOverride)
	{
		OutTipLocal = Root->GetComponentTransform().InverseTransformPosition(RodTipOverride->GetComponentLocation());
		UE_LOG(LogFishing, Log, TEXT("[LineView] 竿先を RodTipOverride から解決"));
		return;
	}

	// --- 第2層: 事前配置マーカー（タグ/コンポーネント名/メッシュソケットの共通キー） ---
	FVector NamedPointWorld;
	if (FindNamedPointWorld(Owner, RodTipPointName, NamedPointWorld))
	{
		OutTipLocal = Root->GetComponentTransform().InverseTransformPosition(NamedPointWorld);
		UE_LOG(LogFishing, Log, TEXT("[LineView] 竿先を基準点「%s」から解決"), *RodTipPointName.ToString());
		return;
	}

	// --- 第3層: 明示オフセット ---
	if (!FallbackTipOffset.IsZero())
	{
		OutTipLocal = FallbackTipOffset;
		UE_LOG(LogFishing, Log, TEXT("[LineView] 竿先を FallbackTipOffset から解決"));
		return;
	}

	// --- 第3層の自動推定: 竿メッシュのバウンディングから長軸方向の先端を求める ---
	TArray<UStaticMeshComponent*> MeshComponents;
	Owner->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (const UStaticMeshComponent* MeshComp : MeshComponents)
	{
		const UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}

		// --- バウンディングの最も長い軸を竿の向きとみなし、メッシュ空間の先端を求める（XYZ 全軸判定） ---
		const FBox MeshBox = Mesh->GetBoundingBox();
		const FVector Extent = MeshBox.GetExtent();
		FVector TipMeshLocal;
		if (Extent.X >= Extent.Y && Extent.X >= Extent.Z)
		{
			TipMeshLocal = FVector(MeshBox.Max.X, 0.0, 0.0);
		}
		else if (Extent.Y >= Extent.X && Extent.Y >= Extent.Z)
		{
			TipMeshLocal = FVector(0.0, MeshBox.Max.Y, 0.0);
		}
		else
		{
			TipMeshLocal = FVector(0.0, 0.0, MeshBox.Max.Z);
		}

		// --- メッシュ コンポーネントの回転/オフセット/スケール込みでルート相対へ変換 ---
		OutTipLocal = Root->GetComponentTransform().InverseTransformPosition(
			MeshComp->GetComponentTransform().TransformPosition(TipMeshLocal));

		UE_LOG(LogFishing, Log, TEXT("[LineView] 竿先をメッシュ境界から自動推定: (%.0f, %.0f, %.0f)"),
			OutTipLocal.X, OutTipLocal.Y, OutTipLocal.Z);
		return;
	}

	UE_LOG(LogFishing, Warning, TEXT("[LineView] 竿先を解決できませんでした（原点を竿先として扱います）"));
}

bool UFishingLineComponent::FindNamedPointWorld(const AActor* Actor, FName PointName, FVector& OutWorld)
{
	OutWorld = FVector::ZeroVector;
	if (!Actor || PointName.IsNone())
	{
		return false;
	}

	// --- 候補収集: 自アクターの全 SceneComponent ＋ ChildActor 内部（GetComponents は ChildActor 内部を含まない） ---
	TArray<USceneComponent*> Components;
	Actor->GetComponents<USceneComponent>(Components);
	TArray<UChildActorComponent*> ChildActorComponents;
	Actor->GetComponents<UChildActorComponent>(ChildActorComponents);
	for (const UChildActorComponent* Child : ChildActorComponents)
	{
		const AActor* ChildActor = Child ? Child->GetChildActor() : nullptr;
		if (ChildActor)
		{
			ChildActor->GetComponents<USceneComponent>(Components);
		}
	}

	// --- 1) タグ / コンポーネント名一致を最優先、2) メッシュソケットを次点で採用 ---
	USceneComponent* SocketOwner = nullptr;
	for (USceneComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}
		if (Component->ComponentTags.Contains(PointName) || Component->GetFName() == PointName)
		{
			OutWorld = Component->GetComponentLocation();
			return true;
		}
		if (!SocketOwner && Component->DoesSocketExist(PointName))
		{
			SocketOwner = Component;
		}
	}
	if (SocketOwner)
	{
		OutWorld = SocketOwner->GetSocketLocation(PointName);
		return true;
	}
	return false;
}

AFish* UFishingLineComponent::FindFish() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// セット毎に魚は 1 匹のみスポーンされるため、最初の生存魚を採用する
	for (TActorIterator<AFish> It(World); It; ++It)
	{
		AFish* Fish = *It;
		if (Fish && !Fish->IsPendingKillPending())
		{
			return Fish;
		}
	}
	return nullptr;
}

bool UFishingLineComponent::GetFishConeTipLocation(const AFish* Fish, FVector& OutTipWorld)
{
	if (!Fish)
	{
		return false;
	}

	// --- BP_Fish の口先は Cone 基本体の錐尖。コンポーネントの世界変換にメッシュ局所の
	//     先端点（+Z 50cm）を通すことで、親チェーンの回転/縮小をまとめて解決する。
	//     （暴れ期の高速な振れは呼び出し側の VInterpTo 平滑で吸受する） ---
	TArray<UStaticMeshComponent*> MeshComponents;
	Fish->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		const UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
		if (Mesh && Mesh->GetName().Contains(TEXT("Cone")))
		{
			OutTipWorld = MeshComp->GetComponentTransform().TransformPosition(FVector(0.0f, 0.0f, BasicConeHalfHeight));
			return true;
		}
	}
	return false;
}

UFishingStateManagerComponent* UFishingLineComponent::FindStateManager()
{
	if (!CachedStateManager.IsValid())
	{
		if (const UWorld* World = GetWorld())
		{
			if (const APlayerController* PC = World->GetFirstPlayerController())
			{
				if (const APawn* Pawn = PC->GetPawn())
				{
					CachedStateManager = Pawn->FindComponentByClass<UFishingStateManagerComponent>();
				}
			}
		}
	}
	return CachedStateManager.Get();
}

void UFishingLineComponent::BreakLine()
{
	bBroken = true;
	if (LineMeshComp)
	{
		LineMeshComp->SetVisibility(false);
	}
	if (HookMeshComp)
	{
		HookMeshComp->SetVisibility(false);
	}
	UE_LOG(LogFishing, Log, TEXT("[LineView] 釣り糸切断: 糸と針を非表示にしました"));
}

void UFishingLineComponent::RestoreLine()
{
	bBroken = false;
	bEndSnapped = false; // 復帰直後の糸端は目標へ即座に貼り付ける（隠れていた間の旧位置から補間しない）
	if (LineMeshComp)
	{
		LineMeshComp->SetVisibility(true);
	}
	if (HookMeshComp)
	{
		HookMeshComp->SetVisibility(true);
	}
	UE_LOG(LogFishing, Log, TEXT("[LineView] 釣り糸復帰: 準備状態へ戻ったため再表示しました"));
}
