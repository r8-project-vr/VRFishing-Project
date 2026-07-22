// Fill out your copyright notice in the Description page of Project Settings.

#include "Takeuchi/Actor/Fish.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"

AFish::AFish()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentState = EFishState::Circling;

	// RotatingMovementComponentを作成し、回転速度を設定
    RotatingMovementComp = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovementComp"));
    RotatingMovementComp->RotationRate = FRotator(0.0f, 90.0f, 0.0f); 
}

void AFish::BeginPlay()
{
    Super::BeginPlay();

    //スポーン位置を中心にする
    CenterLocation = GetActorLocation();

    //回転コンポーネントを有効化
    if (RotatingMovementComp)
    {
        RotatingMovementComp->Activate();
    }

	//中心へ移動する状態に遷移
    GetWorldTimerManager().SetTimer(StateTimerHandle, this, &AFish::TransitionToMoveToCenter, 4.0f, false);

	//仮　F1で暴れる
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
        EnableInput(PC);
        if (InputComponent)
        {
            InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AFish::StartStruggling);

            InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &AFish::CatchFish);
        }
    }
}

void AFish::TransitionToMoveToCenter()
{
    CurrentState = EFishState::MovingToCenter;

	//移動するため回転コンポーネントを無効化
    if (RotatingMovementComp)
    {
		RotatingMovementComp->Deactivate();
    }
}

void AFish::DoPoke()
{
    if (CurrentState != EFishState::Poking) return;

    //離れたり近づいたりする
	bApproaching = !bApproaching;

    if (bApproaching)
    {
        //中心を目指す
        PokeTargetLocation = CenterLocation;
    }
    else
    {
        //【パターンB: 中央を向いたまま、円形に横へ動く ＋ ほんの少し手前に引く】

        //1. 0度〜360度のランダムな角度（ラジアン）を決める
        float RandomAngle = FMath::RandRange(0.0f, PI * 2.0f);

        //2. 中心から離れる距離（半径）を小さく制限する（例: 15〜40単位程度）
        // ※ここを大きくしすぎると遠くに逃げてしまうため、小さな値にします
        float Distance = FMath::RandRange(15.0f, 40.0f);

        //3. 円運動の公式（CosとSin）を使って、中心のまわりの円周上の座標を計算！
        float OffsetX = FMath::Cos(RandomAngle) * Distance;
        float OffsetY = FMath::Sin(RandomAngle) * Distance;

        //4. 中心座標に足し合わせる
        PokeTargetLocation = CenterLocation + FVector(OffsetX, OffsetY, 0.0f);
    }

	//次の突くまでの時間をランダムに設定
    float NextDelay = FMath::RandRange(2.0f, 4.0f);
    GetWorldTimerManager().SetTimer(PokeTimerHandle, this, &AFish::DoPoke, NextDelay, false);
}

void AFish::StartStruggling()
{
    if (CurrentState == EFishState::Struggling || CurrentState == EFishState::Caught) return;

    CurrentState = EFishState::Struggling;
    GetWorldTimerManager().ClearTimer(PokeTimerHandle);

    if (RotatingMovementComp)
    {
        RotatingMovementComp->Deactivate();
    }

    OnStartStruggling();
}
void AFish::CatchFish()
{
    if (CurrentState == EFishState::Caught) return;

    CurrentState = EFishState::Caught;
    GetWorldTimerManager().ClearTimer(PokeTimerHandle);

    if (RotatingMovementComp)
    {
        RotatingMovementComp->Deactivate();
    }

    //釣られた瞬間の現在地から「Z軸方向（真上）に300単位」高い位置を最終目的地として一回だけ固定保存
    CaughtTargetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 300.0f);

    OnCaught(); //BPイベントの実行
}

void AFish::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    RunningTime += DeltaTime;

    switch (CurrentState)
    {
    case EFishState::Circling:
    {
        break;
    }
    case EFishState::MovingToCenter:
    {
		FVector CurrentLoc = GetActorLocation();
        FVector NewLoc = FMath::VInterpTo(GetActorLocation(), CenterLocation, DeltaTime, 2.0f);
        SetActorLocation(NewLoc);

        FRotator TargetRotation = (CenterLocation - CurrentLoc).Rotation();

        TargetRotation.Yaw -= 90.0f;

        FRotator CurrentRot = GetActorRotation();
        FRotator SmoothRot = FMath::RInterpTo(CurrentRot, FRotator(0.0f, TargetRotation.Yaw, 0.0f), DeltaTime, 5.0f);
        SetActorRotation(SmoothRot);

        if (FVector::Dist(GetActorLocation(), CenterLocation) < 20.0f)
        {
            CurrentState = EFishState::Poking;
            bApproaching = false;
            DoPoke(); 
        }
        break;
    }
    case EFishState::Poking:
    {
        float InterpSpeed = bApproaching ? 4.0f : 1.5f; 
        FVector CurrentLoc = GetActorLocation();
        FVector NewLoc = FMath::VInterpTo(GetActorLocation(), PokeTargetLocation, DeltaTime, InterpSpeed);
        SetActorLocation(NewLoc);

        FRotator TargetRotation = (CenterLocation - CurrentLoc).Rotation();
        TargetRotation.Yaw -= 90.0f;
        FRotator SmoothRot = FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, TargetRotation.Yaw, 0.0f), DeltaTime, 5.0f);
        SetActorRotation(SmoothRot);
        break;
    }
    case EFishState::Struggling:
    {
        float FastSpeed = CircleSpeed * 4.0f;
        float X = CenterLocation.X + FMath::Cos(RunningTime * FastSpeed) * StruggleRadius;
        float Y = CenterLocation.Y + FMath::Sin(RunningTime * FastSpeed) * StruggleRadius;
        SetActorLocation(FVector(X, Y, GetActorLocation().Z));

        FRotator TargetRotation = (CenterLocation - GetActorLocation()).Rotation();
        TargetRotation.Yaw -= 90.0f;

        FRotator SmoothRot = FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, TargetRotation.Yaw, 0.0f), DeltaTime, 10.0f);
        SetActorRotation(SmoothRot);
        break;
    }
    case EFishState::Caught:
    {
		FVector CurrentLoc = GetActorLocation();
		FVector NewLoc = FMath::VInterpTo(GetActorLocation(), CaughtTargetLocation, DeltaTime, 3.0f);
		SetActorLocation(NewLoc);

        break;
    }
    }
}
