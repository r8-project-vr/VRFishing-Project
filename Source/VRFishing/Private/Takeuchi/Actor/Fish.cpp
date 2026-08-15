// Fill out your copyright notice in the Description page of Project Settings.

#include "Takeuchi/Actor/Fish.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Kismet/GameplayStatics.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingCatchingStateComponent.h"
#include "Tanimura/Component/FishingResultStateComponent.h"

// 2026.07.27 Lee start
#include "Lee/component/HandHeightDetectorComponent.h"
// 2026.07.27 Lee end
// 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
#include "Lee/component/FishingStateHandUpDown.h"
// 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

AFish::AFish()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentState = EFishState::Circling;

	//RotatingMovementComponentを作成し、回転速度を設定
	RotatingMovementComp = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovementComp"));
	RotatingMovementComp->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
}

void AFish::BeginPlay()
{
	Super::BeginPlay();

	const FVector SpawnLocation = GetActorLocation();

	//魚のスポーン位置は動かさず、PivotTranslationが示す周回中心を固定保存する
	CenterLocation = SpawnLocation;

	if (RotatingMovementComp)
	{
		const FVector RotatingPivotOffset = GetActorRotation().RotateVector(RotatingMovementComp->PivotTranslation);
		CenterLocation += RotatingPivotOffset;
		RotatingMovementComp->Activate();
	}

	FVector PivotTranslation = FVector::ZeroVector;
	if (RotatingMovementComp)
	{
		PivotTranslation = RotatingMovementComp->PivotTranslation;
	}

	const FString SpawnText = SpawnLocation.ToCompactString();
	const FString CenterText = CenterLocation.ToCompactString();
	const FString PivotText = PivotTranslation.ToCompactString();
	const FString CenterDebugMessage = FString::Printf(
		TEXT("[Fish Debug] Spawn=%s | Center=%s | Pivot=%s"),
		*SpawnText,
		*CenterText,
		*PivotText
	);

	UE_LOG(LogTemp, Warning, TEXT("%s"), *CenterDebugMessage);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, CenterDebugMessage);
	}

	//赤い球が固定中心、緑の線がスポーン地点から中心までの距離を示す
	DrawDebugSphere(GetWorld(), CenterLocation, 12.0f, 16, FColor::Red, true);
	DrawDebugLine(GetWorld(), SpawnLocation, CenterLocation, FColor::Green, true, -1.0f, 0, 2.0f);

	// 2026.07.27 谷村　startーーーーーーーーーー
	////4秒後に中心へ移動する状態へ遷移する
	//GetWorldTimerManager().SetTimer(StateTimerHandle, this, &AFish::TransitionToMoveToCenter, 4.0f, false);
	// 2026.07.27 谷村　endーーーーーーーーーー

	//挙動確認用(現在無効化)
	/*APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		EnableInput(PC);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AFish::EscapeFish);
			InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &AFish::CatchFish);
		}
	}*/

	// 2026.07.27 谷村　startーーーーーーーーーー
	//VRPawnの既存イベントを、F1/F2と同じ魚の状態遷移へ接続する
	//AVRPawn* VRPawn = Cast<AVRPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	//if (VRPawn)
	//{
	//	// FishingStateManagerComponentの通知デリゲートへハンドラーを登録
	//	if (UFishingStateManagerComponent* StateManager = VRPawn->FindComponentByClass<UFishingStateManagerComponent>()) {
	//		StateManager->OnFishingStateChanged.AddUniqueDynamic(this, &AFish::OnFishingStateChanged);
	//	}

	//	/*if (UHandHeightDetectorComponent* HandHeightDetector = VRPawn->FindComponentByClass<UHandHeightDetectorComponent>())
	//	{
	//		HandHeightDetector->OnFishHit.AddUniqueDynamic(this, &AFish::StartStruggling);
	//	}

	//	if (UFishingReelStateComponent* ReelSimulator = VRPawn->FindComponentByClass<UFishingReelStateComponent>())
	//	{
	//		ReelSimulator->OnTargetRevolutionsReached.AddUniqueDynamic(this, &AFish::CatchFish);
	//	}*/
	//	//釣り上げステートの完了通知イベントをバインド
	//	if (UFishingCatchingStateComponent* CatchingState = VRPawn->FindComponentByClass<UFishingCatchingStateComponent>()) {
	//		CatchingState->OnFishingStateCompleted.AddUniqueDynamic(this, &AFish::OnCatchingStateCompleted);
	//	}
	//}
	// 2026.07.27 谷村　endーーーーーーーーーー
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		BoundStateManager = PlayerPawn->FindComponentByClass<UFishingStateManagerComponent>();

		if (BoundStateManager)
		{
			//FishingStateManagerComponentの通知デリゲートへハンドラーを登録
			BoundStateManager->OnFishingStateChanged.AddUniqueDynamic(
				this,
				&AFish::OnFishingStateChanged
			);

			//登録前に発生した現在ステートも一度適用する
			OnFishingStateChanged(BoundStateManager->CurrentState);
		}
	}
}

void AFish::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//ステート変更通知のデリゲートを解除する
	if (BoundStateManager)
	{
		BoundStateManager->OnFishingStateChanged.RemoveDynamic(
			this,
			&AFish::OnFishingStateChanged
		);
	}

	//釣り上げ完了通知のデリゲートを解除する
	if (BoundCatchingState)
	{
		BoundCatchingState->OnFishingStateCompleted.RemoveDynamic(
			this,
			&AFish::OnCatchingStateCompleted
		);
	}

	BoundCatchingState = nullptr;
	BoundStateManager = nullptr;

	ClearStateTimers();

	Super::EndPlay(EndPlayReason);
}

// 2026.07.27 谷村　startーーーーーーーーーー
// 釣りモードのステート変更に応じて魚の挙動を制御するハンドラー
void AFish::OnFishingStateChanged(UFishingStateComponentBase* NewState)
{
	if (!NewState) {
		return;
	}

	// 準備モード (Ready): 周回運動 (Circling)
	if (NewState->IsA<UFishingReadyStateComponent>()) {
		if (CurrentState != EFishState::Circling) {
			CurrentState = EFishState::Circling;
			ClearStateTimers();
			if (RotatingMovementComp && !RotatingMovementComp->IsActive()) {
				RotatingMovementComp->Activate();
			}
		}
	}
	// 2026.07.27 Lee start
	// 高さ検知モード (HandHeight): 中心移動 (MovingToCenter) 経由で つつき (Poking) へ
	//else if (NewState->IsA<UHandHeightDetectorComponent>()) {←もともとのコードも消さない！
	//	TransitionToMoveToCenter();
	//}
	// 2026.07.27 Lee end
	// 2026.07.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// センサ(HandHeightDetector)はステートでなくなったため、上下運動プレイステートで判定する
	else if (NewState->IsA<UFishingStateHandUpDown>()) {
		TransitionToMoveToCenter();
	}
	// 2026.07.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// リール回転モード (Reel): 暴れる (Struggling)
	else if (NewState->IsA<UFishingReelStateComponent>()) {
		StartStruggling();
	}
	// 釣り上げモード (Catching): 完了イベントで処理するため開始時は処理なし
	else if (NewState->IsA<UFishingCatchingStateComponent>()) {
		//StartCatchDelay();
		// 2026.08.05 竹内 startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		if (BoundCatchingState && BoundCatchingState != NewState)
		{
			BoundCatchingState->OnFishingStateCompleted.RemoveDynamic(
				this,
				&AFish::OnCatchingStateCompleted
			);
		}
		BoundCatchingState = NewState;
		BoundCatchingState->OnFishingStateCompleted.AddUniqueDynamic(
			this,
			&AFish::OnCatchingStateCompleted
		);
		// 2026.08.05 竹内　endーーーーーーーーーー
	}

	// 2026.08.05 谷村 startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 釣り上げ結果モード (Result): 失敗時のみ魚が逃げる（成功時は既に Caught 状態）
	else if (NewState->IsA<UFishingResultStateComponent>()) {
		if (!NewState->IsSuccessState()) {
			EscapeFish();
		}
	}
	// 2026.08.05 谷村 endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
}
// 2026.07.27 谷村　endーーーーーーーーーー

void AFish::OnCatchingStateCompleted(bool bIsSuccess)
{
	if (bIsSuccess)
	{
		CatchFish();
	}
	else
	{
		EscapeFish();
	}
}
void AFish::TransitionToMoveToCenter()
{
	//通知を受けた時点で周回状態の場合のみ、中央へ移動する状態へ変更する
	if (CurrentState != EFishState::Circling)
	{
		return;
	}

	CurrentState = EFishState::MovingToCenter;

	if (RotatingMovementComp)
	{
		RotatingMovementComp->Deactivate();
	}
}

void AFish::DoPoke()
{
	if (CurrentState != EFishState::Poking)
	{
		return;
	}

	//接近と退避を交互に切り替える
	bApproaching = !bApproaching;

	if (bApproaching)
	{
		//Actor原点ではなく口先が中央へ届くよう、現在いる側で手前に停止する
		FVector ApproachSide = GetActorLocation() - CenterLocation;
		ApproachSide.Z = 0.0f;

		if (ApproachSide.IsNearlyZero())
		{
			ApproachSide = -GetActorRightVector();
			ApproachSide.Z = 0.0f;
		}

		ApproachSide.Normalize();
		PokeTargetLocation = CenterLocation + ApproachSide * PokeContactOffset;
	}
	else
	{
		//中央から現在位置へ向かう方向を、後退の基準方向にする
		FVector RetreatDirection = GetActorLocation() - CenterLocation;
		RetreatDirection.Z = 0.0f;

		if (RetreatDirection.IsNearlyZero())
		{
			const float RandomBaseAngle = FMath::RandRange(0.0f, 360.0f);
			RetreatDirection = FVector::ForwardVector.RotateAngleAxis(RandomBaseAngle, FVector::UpVector);
		}
		else
		{
			RetreatDirection.Normalize();
		}

		//真後ろだけでなく、左右どちらかへランダムにずれながら離れる
		const float SideAngle = FMath::RandRange(-MaxRetreatSideAngle, MaxRetreatSideAngle);
		RetreatDirection = RetreatDirection.RotateAngleAxis(SideAngle, FVector::UpVector);

		const float MinDistance = FMath::Min(PokeRetreatDistanceMin, PokeRetreatDistanceMax);
		const float MaxDistance = FMath::Max(PokeRetreatDistanceMin, PokeRetreatDistanceMax);
		const float Distance = FMath::RandRange(MinDistance, MaxDistance);
		PokeTargetLocation = CenterLocation + RetreatDirection * Distance;
	}

	const float MinInterval = FMath::Min(PokeIntervalMin, PokeIntervalMax);
	const float MaxInterval = FMath::Max(PokeIntervalMin, PokeIntervalMax);
	const float NextDelay = FMath::RandRange(MinInterval, MaxInterval);
	GetWorldTimerManager().SetTimer(PokeTimerHandle, this, &AFish::DoPoke, NextDelay, false);
}

void AFish::StartStruggling()
{
	if (CurrentState == EFishState::Struggling || CurrentState == EFishState::Caught)
	{
		return;
	}

	CurrentState = EFishState::Struggling;
	ClearStateTimers();

	if (RotatingMovementComp)
	{
		RotatingMovementComp->Deactivate();
	}

	//現在位置から連続して円運動を始め、状態変更時の位置飛びを防ぐ
	const FVector CurrentOffset = GetActorLocation() - CenterLocation;
	StruggleStartAngle = FMath::Atan2(CurrentOffset.Y, CurrentOffset.X);
	StruggleCurrentRadius = CurrentOffset.Size2D();
	StruggleElapsedTime = 0.0f;

	OnStartStruggling();
}

void AFish::StartCatchDelay()
{
	if (CurrentState == EFishState::CatchDelay || CurrentState == EFishState::Caught)
	{
		return;
	}

	CurrentState = EFishState::CatchDelay;
	ClearStateTimers();

	if (RotatingMovementComp)
	{
		RotatingMovementComp->Deactivate();
	}

	if (PreCatchingWaitTime <= 0.0f)
	{
		CatchFish();
		return;
	}

	GetWorldTimerManager().SetTimer(
		PreCatchingTimerHandle,
		this,
		&AFish::CatchFish,
		PreCatchingWaitTime,
		false
	);
}

void AFish::CatchFish()
{
	if (CurrentState == EFishState::Caught)
	{
		return;
	}

	CurrentState = EFishState::Caught;
	ClearStateTimers();

	if (RotatingMovementComp)
	{
		RotatingMovementComp->Deactivate();
	}

	//捕獲位置のCaughtHeight分上を、移動中に変化しない吊り上げ先として固定する
	CaughtTargetLocation = GetActorLocation() + FVector(0.0f, 0.0f, CaughtHeight);

	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw, -90.0f));
}

void AFish::RotateTowardCenter(float DeltaTime, float InterpSpeed)
{
	const FVector DirectionToCenter = CenterLocation - GetActorLocation();
	if (DirectionToCenter.IsNearlyZero())
	{
		return;
	}

	FRotator TargetRotation = DirectionToCenter.Rotation();
	TargetRotation.Yaw -= 90.0f;

	const FRotator SmoothRotation = FMath::RInterpTo(
		GetActorRotation(),
		FRotator(0.0f, TargetRotation.Yaw, 0.0f),
		DeltaTime,
		InterpSpeed
	);
	SetActorRotation(SmoothRotation);
}

void AFish::ClearStateTimers()
{
	GetWorldTimerManager().ClearTimer(PokeTimerHandle);
	GetWorldTimerManager().ClearTimer(PreCatchingTimerHandle);
}

void AFish::EscapeFish()
{
	if (CurrentState == EFishState::Escape || CurrentState == EFishState::Caught)
	{
		return;
	}

	CurrentState = EFishState::Escape;
	ClearStateTimers();

	if (RotatingMovementComp)
	{
		RotatingMovementComp->Deactivate();
	}

	//中心から現在位置へ向かう外向きのベクトルを算出
	FVector EscapeDirection = GetActorLocation() - CenterLocation;
	EscapeDirection.Z = 0.0f;

	if (EscapeDirection.IsNearlyZero())
	{
		EscapeDirection = -GetActorRightVector();
		EscapeDirection.Z = 0.0f;
	}
	EscapeDirection.Normalize();

	//開始位置、外側の目標位置、初期スケールを保存
	EscapeStartLocation = GetActorLocation();
	EscapeTargetLocation = EscapeStartLocation + EscapeDirection * EscapeDistance;
	EscapeStartScale = GetActorScale3D();

	//頭を外側に向ける
	FRotator TargetRotation = EscapeDirection.Rotation();
	TargetRotation.Yaw -= 90.0f;
	SetActorRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));
}

void AFish::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (CurrentState)
	{
	case EFishState::Circling:
	{
		//周回中の移動はRotatingMovementComponentが担当する
		break;
	}
	case EFishState::MovingToCenter:
	{
		//初回接近でもActor原点ではなく、魚の口先が中央へ届く位置を目標にする
		FVector ApproachSide = GetActorLocation() - CenterLocation;
		ApproachSide.Z = 0.0f;

		if (ApproachSide.IsNearlyZero())
		{
			ApproachSide = -GetActorRightVector();
			ApproachSide.Z = 0.0f;
		}

		ApproachSide.Normalize();
		const FVector InitialPokeTarget = CenterLocation + ApproachSide * PokeContactOffset;
		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), InitialPokeTarget, DeltaTime, MoveToCenterSpeed);
		SetActorLocation(NewLocation);
		RotateTowardCenter(DeltaTime, 5.0f);

		if (FVector::Dist(GetActorLocation(), InitialPokeTarget) < 1.0f)
		{
			CurrentState = EFishState::Poking;
			bApproaching = false;
			DoPoke();
		}
		break;
	}
	case EFishState::Poking:
	{
		float InterpSpeed = PokeRetreatSpeed;
		if (bApproaching)
		{
			InterpSpeed = PokeApproachSpeed;
		}

		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), PokeTargetLocation, DeltaTime, InterpSpeed);
		SetActorLocation(NewLocation);
		RotateTowardCenter(DeltaTime, 5.0f);
		break;
	}
	case EFishState::Struggling:
	{
		StruggleElapsedTime += DeltaTime;
		StruggleCurrentRadius = FMath::FInterpTo(StruggleCurrentRadius, StruggleRadius, DeltaTime, 3.0f);
		const float FastSpeed = CircleSpeed * StruggleSpeedMultiplier;
		const float CurrentAngle = StruggleStartAngle + StruggleElapsedTime * FastSpeed;
		const float X = CenterLocation.X + FMath::Cos(CurrentAngle) * StruggleCurrentRadius;
		const float Y = CenterLocation.Y + FMath::Sin(CurrentAngle) * StruggleCurrentRadius;
		SetActorLocation(FVector(X, Y, GetActorLocation().Z));
		RotateTowardCenter(DeltaTime, 10.0f);
		break;
	}
	case EFishState::CatchDelay:
	{
		break;
	}
	case EFishState::Caught:
	{
		if (FVector::DistSquared(GetActorLocation(), CaughtTargetLocation) <= FMath::Square(1.0f))
		{
			SetActorLocation(CaughtTargetLocation);
			break;
		}

		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), CaughtTargetLocation, DeltaTime, CaughtMoveSpeed);
		SetActorLocation(NewLocation);
		break;
	}
	case EFishState::Escape:
	{
		//外側の目標位置へ移動
		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), EscapeTargetLocation, DeltaTime, EscapeSpeed);
		SetActorLocation(NewLocation);

		//頭を常に外側へ向け続ける
		FVector EscapeDirection = EscapeTargetLocation - EscapeStartLocation;
		EscapeDirection.Z = 0.0f;
		if (!EscapeDirection.IsNearlyZero())
		{
			FRotator TargetRotation = EscapeDirection.Rotation();
			TargetRotation.Yaw -= 90.0f; //頭を外側に向ける補正

			const FRotator SmoothRotation = FMath::RInterpTo(
				GetActorRotation(),
				FRotator(0.0f, TargetRotation.Yaw, 0.0f),
				DeltaTime,
				10.0f
			);
			SetActorRotation(SmoothRotation);
		}

		//残り距離に応じてスケールを滑らかに小さくする
		const float TotalDistance = FVector::Dist(EscapeStartLocation, EscapeTargetLocation);
		const float RemainingDistance = FVector::Dist(GetActorLocation(), EscapeTargetLocation);

		if (TotalDistance > 0.0f)
		{
			const float Alpha = FMath::Clamp(RemainingDistance / TotalDistance, 0.0f, 1.0f);
			SetActorScale3D(EscapeStartScale * Alpha);
		}

		//目標地点に到達したらアクターを破棄する
		if (RemainingDistance <= 10.0f)
		{
			Destroy();
		}
		break;
	}
	}
}
