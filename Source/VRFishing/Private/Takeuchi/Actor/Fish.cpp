// Fill out your copyright notice in the Description page of Project Settings.

#include "Takeuchi/Actor/Fish.h"
#include "Kismet/GameplayStatics.h"

AFish::AFish()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentState = EFishType::Circling;
}

void AFish::BeginPlay()
{
    Super::BeginPlay();

    CenterLocation = GetActorLocation();

    GetWorldTimerManager().SetTimer(StateTimerHandle, this, &AFish::TransitionToMoveToCenter, 5.0f, false);

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
        EnableInput(PC);
        if (InputComponent)
        {
            InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AFish::StartStruggling);
        }
    }
}

void AFish::TransitionToMoveToCenter()
{
    CurrentState = EFishType::MovingToCenter;
}

void AFish::DoPoke()
{
    if (CurrentState != EFishType::Poking) return;

	bApproaching = !bApproaching;

    if (bApproaching)
    {
        PokeTargetLocation = CenterLocation;
    }
    else
    {
        FVector Offset = FVector(FMath::RandRange(-80.0f, 80.0f), FMath::RandRange(-80.0f, 80.0f), 0.0f);
        PokeTargetLocation = CenterLocation + Offset;
    }

    float NextDelay = FMath::RandRange(3.0f, 5.0f);
    GetWorldTimerManager().SetTimer(PokeTimerHandle, this, &AFish::DoPoke, NextDelay, false);
}

void AFish::StartStruggling()
{
    if (CurrentState == EFishType::Struggling || CurrentState == EFishType::Caught) return;

    CurrentState = EFishType::Struggling;
    GetWorldTimerManager().ClearTimer(PokeTimerHandle);

    OnStartStruggling();
}

void AFish::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    RunningTime += DeltaTime;

    switch (CurrentState)
    {
    case EFishType::Circling:
    {
        float X = CenterLocation.X + FMath::Cos(RunningTime * CircleSpeed) * CircleRadius;
        float Y = CenterLocation.Y + FMath::Sin(RunningTime * CircleSpeed) * CircleRadius;
        SetActorLocation(FVector(X, Y, GetActorLocation().Z));
        break;
    }
    case EFishType::MovingToCenter:
    {
        FVector NewLoc = FMath::VInterpTo(GetActorLocation(), CenterLocation, DeltaTime, 2.0f);
        SetActorLocation(NewLoc);

        if (FVector::Dist(GetActorLocation(), CenterLocation) < 20.0f)
        {
            CurrentState = EFishType::Poking;
            DoPoke(); 
        }
        break;
    }
    case EFishType::Poking:
    {
        float InterpSpeed = bApproaching ? 4.0f : 1.5f; 
        FVector NewLoc = FMath::VInterpTo(GetActorLocation(), PokeTargetLocation, DeltaTime, InterpSpeed);
        SetActorLocation(NewLoc);
        break;
    }
    case EFishType::Struggling:
    {
        float FastSpeed = CircleSpeed * 4.0f;
        float X = CenterLocation.X + FMath::Cos(RunningTime * FastSpeed) * StruggleRadius;
        float Y = CenterLocation.Y + FMath::Sin(RunningTime * FastSpeed) * StruggleRadius;
        SetActorLocation(FVector(X, Y, GetActorLocation().Z));
        break;
    }
    case EFishType::Caught:
    {
        FVector TargetHeight = GetActorLocation() + FVector(0, 0, 300.0f);
        SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetHeight, DeltaTime, 3.0f));
        break;
    }
    }
}
