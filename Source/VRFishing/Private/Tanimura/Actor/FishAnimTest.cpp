// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Tanimura/Actor/FishAnimTest.h"
#include "Components/SceneComponent.h"

AFishAnimTest::AFishAnimTest()
{
	PrimaryActorTick.bCanEverTick = true;

	//SceneComponentを作成し、RootComponentに設定
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;
}

void AFishAnimTest::BeginPlay()
{
	Super::BeginPlay();

	CollectSpineBones();
}

void AFishAnimTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;

	//魚の速度に応じた周波数（振り回数）を取得
	const float CurrentFrequency = Frequency + SwimSpeed * FrequencyPerSpeed;

	//ボーン配列末尾のインデックスを取得
	const float LastIndex = static_cast<float>(SpineBones.Num() - 1);

	for (int32 Index = 0; Index < SpineBones.Num(); ++Index) {
		//体のどのあたりかを0〜1で表す
		const float AlongBody = static_cast<float>(Index) / LastIndex;

		//尾に近いほど大きく振れる
		const float Amplitude = MaxAmplitude * FMath::Pow(AlongBody, AmplitudeExponent);

		//尾に近いほど位相を遅らせる
		const float Phase = UE_TWO_PI * (CurrentFrequency * ElapsedTime - WaveCount * AlongBody);

		//波の角度は初期回転へ加算し、BP側で付けた傾きを保つ
		const FRotator WaveRotation(0.0f, Amplitude * FMath::Sin(Phase), 0.0f);
		SpineBones[Index]->SetRelativeRotation(InitialRotations[Index] + WaveRotation);
	}
}

void AFishAnimTest::CollectSpineBones()
{
	SpineBones.Reset();
	InitialRotations.Reset();

	//BP側で追加されたSceneComponentを取得
	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);

	//Bone0から順に名前で検索し、頭→尾の順に格納
	for (int32 BoneIndex = 0; ; ++BoneIndex) {
		const FString BoneName = FString::Printf(TEXT("Bone%d"), BoneIndex);
		USceneComponent* FoundBone = nullptr;
		for (USceneComponent* Component : SceneComponents) {
			if (IsValid(Component) && Component->GetName() == BoneName) {
				FoundBone = Component;
				break;
			}
		}

		//次の番号が見つからない場合は末尾とみなす
		if (!FoundBone) {
			break;
		}

		SpineBones.Add(FoundBone);
	}

	//見つからない場合の警告
	if (SpineBones.Num() < 2) {
		UE_LOG(LogTemp, Warning, TEXT("%s: Bone0は2つ以上必要です"), *GetName());
		return;
	}

	//波を加算する基準として初期回転を記録しておく
	InitialRotations.Reserve(SpineBones.Num());
	for (USceneComponent* Bone : SpineBones) {
		InitialRotations.Add(Bone->GetRelativeRotation());
	}
}
