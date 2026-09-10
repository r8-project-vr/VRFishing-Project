// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/subsystem/FishingSeControllerSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Lee/settings/FishingAudioSettings.h"
#include "Lee/widget/ReelRPMThresholdReader.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingResultStateComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "VRFishingLog.h"

namespace
{
	/** 世界再スキャンの間隔（秒）。破棄済み監視対象の掃除と新規対象の検出を兼ねる */
	constexpr float SeWorldRefreshInterval = 0.5f;

	/** 反射による RPM 閾値キャッシュの有効期間（秒） */
	constexpr float SeRpmThresholdCacheDuration = 0.25f;
}

// override: 専用サーバーでは生成しない
bool UFishingSeControllerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// 音響出力が存在しない環境では動かない
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->GetNetMode() != NM_DedicatedServer;
}

// override: 購読解除とボイス掃除
void UFishingSeControllerSubsystem::Deinitialize()
{
	UnbindAll();
	FishEntries.Empty();
	Super::Deinitialize();
}

// override: 監視を一定間隔で間引いて実行する
void UFishingSeControllerSubsystem::Tick(float DeltaTime)
{
	// --- 世界再スキャン（間引き） ---
	RefreshAccumulated += DeltaTime;
	if (RefreshAccumulated >= SeWorldRefreshInterval)
	{
		RefreshAccumulated = 0.0f;
		RefreshWorldWatch();
	}

	// --- 魚の状態ポーリングとボイス管理 ---
	UpdateFishVoices(DeltaTime);
}

// override
TStatId UFishingSeControllerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFishingSeControllerSubsystem, STATGROUP_Tickables);
}

void UFishingSeControllerSubsystem::RefreshWorldWatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// --- Pawn 走査: StateManager（本編）／MenuNavigator（タイトル・リザルト）を探索して購読する ---
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;

		// 本編 Pawn（StateManager を持つ。BP_XRPawn 系。公開コンポーネント探索のみで型に触れない）
		if (!BoundStateManager.IsValid() && Pawn->FindComponentByClass<UFishingStateManagerComponent>())
		{
			BindFishingPawn(Pawn);
		}

		// メニュー Pawn（Navigator を持つ。BP_MenuPawn 系）
		if (!BoundNavigator.IsValid())
		{
			if (UFishingMenuNavigatorComponent* Navigator = Pawn->FindComponentByClass<UFishingMenuNavigatorComponent>())
			{
				Navigator->OnFocusChanged.AddUniqueDynamic(this, &ThisClass::HandleFocusChanged);
				Navigator->OnMenuAction.AddUniqueDynamic(this, &ThisClass::HandleMenuAction);
				BoundNavigator = Navigator;
				UE_LOG(LogFishing, Log, TEXT("[SeController] MenuNavigator を購読しました"));
			}
		}

		if (BoundStateManager.IsValid() && BoundNavigator.IsValid())
		{
			break;
		}
	}

	// --- 魚走査: 新規魚を監視へ追加する ---
	for (TActorIterator<AFish> It(World); It; ++It)
	{
		AFish* Fish = *It;

		bool bAlreadyWatched = false;
		for (const FFishingSeFishEntry& Entry : FishEntries)
		{
			if (Entry.Fish.Get() == Fish)
			{
				bAlreadyWatched = true;
				break;
			}
		}

		if (!bAlreadyWatched)
		{
			FFishingSeFishEntry Entry;
			Entry.Fish = Fish;
			Entry.LastState = Fish->CurrentState;
			FishEntries.Add(Entry);
			UE_LOG(LogFishing, Verbose, TEXT("[SeController] 魚を監視対象へ追加（初期状態 %d）"), static_cast<int32>(Entry.LastState));
		}
	}

	// --- 破棄済み魚の掃除（ボイスは魚 Owner のため魚の破棄で自動停止済み） ---
	FishEntries.RemoveAll([](const FFishingSeFishEntry& Entry) { return !Entry.Fish.IsValid(); });
}

void UFishingSeControllerSubsystem::BindFishingPawn(APawn* Pawn)
{
	UFishingStateManagerComponent* Manager = Pawn->FindComponentByClass<UFishingStateManagerComponent>();
	if (!Manager)
	{
		return;
	}

	// --- ステート切替の購読 ---
	Manager->OnFishingStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
	BoundStateManager = Manager;

	// --- リール RPM の購読 ---
	if (UFishingReelStateComponent* Reel = Pawn->FindComponentByClass<UFishingReelStateComponent>())
	{
		Reel->OnRPMCalculated.AddUniqueDynamic(this, &ThisClass::HandleRpmCalculated);
		BoundReelState = Reel;
	}

	// --- 完了通知はステートコンポーネント基準クラスで一括購読する ---
	// 動的デリゲートはハンドラ引数が厳密一致のため、完了元ステートの判別はバインド時の型判定で行う
	// （ResultState 完了だけはリザルト表示音へ分流させる）
	TArray<UFishingStateComponentBase*> StateComponents;
	Pawn->GetComponents<UFishingStateComponentBase>(StateComponents);
	for (UFishingStateComponentBase* State : StateComponents)
	{
		if (!State)
		{
			continue;
		}

		if (State->IsA<UFishingResultStateComponent>())
		{
			State->OnFishingStateCompleted.AddUniqueDynamic(this, &ThisClass::HandleResultCompleted);
		}
		else
		{
			State->OnFishingStateCompleted.AddUniqueDynamic(this, &ThisClass::HandleStateCompleted);
		}
		BoundStateComponents.Add(State);
	}

	UE_LOG(LogFishing, Log, TEXT("[SeController] 釣り Pawn のステートを購読しました（ステート %d 個）"), StateComponents.Num());
}

void UFishingSeControllerSubsystem::UnbindAll()
{
	if (UFishingStateManagerComponent* Manager = BoundStateManager.Get())
	{
		Manager->OnFishingStateChanged.RemoveAll(this);
	}
	BoundStateManager = nullptr;

	if (UFishingReelStateComponent* Reel = BoundReelState.Get())
	{
		Reel->OnRPMCalculated.RemoveAll(this);
	}
	BoundReelState = nullptr;

	for (const TWeakObjectPtr<UFishingStateComponentBase>& WeakState : BoundStateComponents)
	{
		if (UFishingStateComponentBase* State = WeakState.Get())
		{
			State->OnFishingStateCompleted.RemoveAll(this);
		}
	}
	BoundStateComponents.Reset();

	if (UFishingMenuNavigatorComponent* Navigator = BoundNavigator.Get())
	{
		Navigator->OnFocusChanged.RemoveAll(this);
		Navigator->OnMenuAction.RemoveAll(this);
	}
	BoundNavigator = nullptr;
}

void UFishingSeControllerSubsystem::HandleStateChanged(UFishingStateComponentBase* /*NewState*/)
{
	// フェーズが切り替わるたびに 1 回鳴らす（Pawn 初期化直後の初回 Ready は購読前のため鳴らない）
	PlayUiSfx(EFishingUiSfx::PhaseChange);
}

void UFishingSeControllerSubsystem::HandleStateCompleted(bool bIsSuccess)
{
	// 各ステート（Ready／上下／リール／つりあげ）の完了。成否で音を出し分ける
	PlayUiSfx(bIsSuccess ? EFishingUiSfx::PhaseSuccess : EFishingUiSfx::PhaseFail);
}

void UFishingSeControllerSubsystem::HandleResultCompleted(bool /*bIsSuccess*/)
{
	// ResultState 完了＝リザルト画面表示（GameMode.OnSetCompleted と同時機）
	PlayUiSfx(EFishingUiSfx::ResultAppear);
}

void UFishingSeControllerSubsystem::HandleFocusChanged(UWidget* /*FocusedWidget*/, EFishingTitleMenuAction /*FocusedAction*/)
{
	PlayUiSfx(EFishingUiSfx::MenuCursorMove);
}

void UFishingSeControllerSubsystem::HandleMenuAction(EFishingTitleMenuAction /*Action*/)
{
	PlayUiSfx(EFishingUiSfx::MenuConfirm);
}

void UFishingSeControllerSubsystem::HandleRpmCalculated(float NewRPM)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// --- 閾値キャッシュの更新（反射読み取りのため 0.25 秒間隔で再取得。RpmGaugeWidget と同一方針） ---
	if ((World->GetTimeSeconds() - RpmThresholdCacheTime) > SeRpmThresholdCacheDuration)
	{
		if (UFishingReelStateComponent* Reel = BoundReelState.Get())
		{
			float MinRpm = 0.0f;
			float WheelMaxRpm = 0.0f;
			float StickMaxRpm = 0.0f;
			if (LeeReelRpm::ReadReelRPMThresholds(Reel, MinRpm, WheelMaxRpm, StickMaxRpm))
			{
				CachedMinRPM = MinRpm;
				// 上限は未入力時の予測用に両デバイス分を保持する（解決は下で毎回行う）
				CachedWheelMaxRPM = WheelMaxRpm;
				CachedStickMaxRPM = StickMaxRpm;
				RpmThresholdCacheTime = World->GetTimeSeconds();
			}
		}
	}

	// --- 閾値を一度も取得できていない間は判定しない（無音） ---
	if (RpmThresholdCacheTime < 0.0f)
	{
		return;
	}

	// --- 判定分類と変化検出（判定が切り替わった瞬間のみ鳴らす） ---
	// 上限は 0.25 秒キャッシュを介さず判定側の実値を毎回参照する。キャッシュ経由だと入力デバイスが
	// 切り替わった直後（例：非 VR 実行で自転車デバイスを使い始めた直後）に予測値で誤判定し、
	// 誤った判定変化音が鳴る。直読みはメンバ参照のみでコストが無い。
	const float MaxRpm = LeeReelRpm::ResolveJudgedMaxAllowedRPM(BoundReelState.Get(), CachedWheelMaxRPM, CachedStickMaxRPM);

	// 分類は共通実装へ委譲し、ここでは音用の内部列挙へ名前を付け替えるだけにする
	const EHandSpeedState RpmState = LeeReelRpm::ClassifyRPM(CachedMinRPM, MaxRpm, NewRPM);
	const EFishingSeRpmJudge Judge =
		(RpmState == EHandSpeedState::TooSlow) ? EFishingSeRpmJudge::TooSlow
		: (RpmState == EHandSpeedState::TooFast) ? EFishingSeRpmJudge::TooFast
		: EFishingSeRpmJudge::Good;

	if (Judge != LastRpmJudge)
	{
		LastRpmJudge = Judge;
		PlayUiSfx(EFishingUiSfx::RpmJudgeChange);
	}
}

void UFishingSeControllerSubsystem::UpdateFishVoices(float DeltaTime)
{
	const UFishingAudioSettings* Settings = GetDefault<UFishingAudioSettings>();

	for (FFishingSeFishEntry& Entry : FishEntries)
	{
		AFish* Fish = Entry.Fish.Get();
		if (!Fish)
		{
			continue;
		}

		// --- 状態遷移検出 ---
		const EFishState NowState = Fish->CurrentState;
		if (NowState != Entry.LastState)
		{
			// 前状態のボイスは必ず止める（状態と音を 1:1 に保つ）
			StopFishVoice(Entry);

			switch (NowState)
			{
			case EFishState::Struggling:
				// 暴れ水音（ループ。SoundWave 側の Looping 推奨。未設定の場合は下段の継続再生でつなぐ）
				if (USoundBase* Sound = Settings->StruggleLoopSfx.LoadSynchronous())
				{
					StartFishVoice(Entry, Sound, true);
				}
				break;

			case EFishState::Poking:
				// 咬みつき音（Poking 中は一定間隔で繰り返し。魚のつつき動作への同期的検出は不可能なための代替）
				if (USoundBase* Sound = Settings->PokeSfx.LoadSynchronous())
				{
					StartFishVoice(Entry, Sound, false);
				}
				Entry.PokeSfxElapsed = 0.0f;
				break;

			case EFishState::Escape:
				// 逃走音（魚に追従し、遠ざかるにつれ減衰する）
				if (USoundBase* Sound = Settings->EscapeSfx.LoadSynchronous())
				{
					StartFishVoice(Entry, Sound, false);
				}
				break;

			case EFishState::Caught:
				// 釣り上げ音
				if (USoundBase* Sound = Settings->CaughtSfx.LoadSynchronous())
				{
					StartFishVoice(Entry, Sound, false);
				}
				break;

			default:
				// Circling / MovingToCenter / CatchDelay は無音
				break;
			}

			Entry.LastState = NowState;
		}

		// --- Poking 中の咬みつき音の周期再生 ---
		if (NowState == EFishState::Poking)
		{
			Entry.PokeSfxElapsed += DeltaTime;
			if (Entry.PokeSfxElapsed >= PokeSfxInterval)
			{
				Entry.PokeSfxElapsed = 0.0f;
				if (USoundBase* Sound = Settings->PokeSfx.LoadSynchronous())
				{
					StartFishVoice(Entry, Sound, false);
				}
			}
		}

		// --- ループボイスの継続と、鳴き終えた単発ボイスの掃除（Stop 時は状態側で bVoiceLoop=false 済み） ---
		if (UAudioComponent* Voice = Entry.VoiceComponent.Get())
		{
			if (!Voice->IsPlaying())
			{
				if (Entry.bVoiceLoop)
				{
					// Looping 未設定の音が自然終了したら再生し直す
					Voice->Play();
				}
				else
				{
					// 単発音は鳴り終えたら破棄する（OneShot ボイスの残留を防ぐ）
					Voice->DestroyComponent();
					Entry.VoiceComponent = nullptr;
				}
			}
		}
	}
}

void UFishingSeControllerSubsystem::StartFishVoice(FFishingSeFishEntry& Entry, USoundBase* Sound, bool bLoop)
{
	AFish* Fish = Entry.Fish.Get();
	if (!Fish || !Sound)
	{
		return;
	}

	// --- 既存ボイスがあれば先に止める（1 魚 = 1 ボイス。Quest の同時発音数対策） ---
	StopFishVoice(Entry);

	// --- 魚を Owner として NewObject する（魚の移動へ自動追従、魚の破棄で自動停止。AFish 側は無変更） ---
	UAudioComponent* Voice = NewObject<UAudioComponent>(Fish, TEXT("SeFishVoice"));
	Voice->SetSound(Sound);
	Voice->bAutoActivate = false;
	Voice->bStopWhenOwnerDestroyed = true;
	Voice->VolumeMultiplier = GetDefault<UFishingAudioSettings>()->WorldSfxVolume;

	// --- 減衰設定アセットが無いケース向けのコード既定減衰（Falloff は EscapeDistance=500cm を十分カバー） ---
	Voice->bOverrideAttenuation = true;
	FSoundAttenuationSettings& Attenuation = Voice->AttenuationOverrides;
	Attenuation.bAttenuate = true;
	Attenuation.bSpatialize = true;
	Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	// Sphere の X が半径（1m までは減衰なし）
	Attenuation.AttenuationShapeExtents = FVector(100.0f, 0.0f, 0.0f);
	Attenuation.FalloffDistance = 1500.0f;
	Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

	Voice->RegisterComponent();
	Voice->Play();

	Entry.VoiceComponent = Voice;
	Entry.bVoiceLoop = bLoop;
}

void UFishingSeControllerSubsystem::StopFishVoice(FFishingSeFishEntry& Entry)
{
	// ループ継続フラグを先に落とす（Stop 完了後の誤った再再生を防ぐ）
	Entry.bVoiceLoop = false;

	if (UAudioComponent* Voice = Entry.VoiceComponent.Get())
	{
		Voice->Stop();
		Voice->DestroyComponent();
	}
	Entry.VoiceComponent = nullptr;
}

void UFishingSeControllerSubsystem::PlayUiSfx(EFishingUiSfx SfxId, float VolumeMultiplier)
{
	// --- 音源未設定時は無音で戻る（エラーなし。素材後入れ前提） ---
	USoundBase* Sound = ResolveUiSfx(SfxId);
	if (!Sound)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float Volume = GetDefault<UFishingAudioSettings>()->UiSfxVolume * VolumeMultiplier;
		UGameplayStatics::PlaySound2D(World, Sound, Volume, 1.0f);
	}
}

USoundBase* UFishingSeControllerSubsystem::ResolveUiSfx(EFishingUiSfx SfxId) const
{
	const UFishingAudioSettings* Settings = GetDefault<UFishingAudioSettings>();

	switch (SfxId)
	{
	case EFishingUiSfx::MenuCursorMove: return Settings->MenuCursorMoveSfx.LoadSynchronous();
	case EFishingUiSfx::MenuConfirm:    return Settings->MenuConfirmSfx.LoadSynchronous();
	case EFishingUiSfx::PhaseChange:    return Settings->PhaseChangeSfx.LoadSynchronous();
	case EFishingUiSfx::PhaseSuccess:   return Settings->PhaseSuccessSfx.LoadSynchronous();
	case EFishingUiSfx::PhaseFail:      return Settings->PhaseFailSfx.LoadSynchronous();
	case EFishingUiSfx::ResultAppear:   return Settings->ResultAppearSfx.LoadSynchronous();
	case EFishingUiSfx::RpmJudgeChange: return Settings->RpmJudgeChangeSfx.LoadSynchronous();
	default:
		// 将来の列挙追加漏れ検出用
		UE_LOG(LogFishing, Warning, TEXT("[SeController] 未対応の UI 効果音 ID: %d"), static_cast<int32>(SfxId));
		return nullptr;
	}
}
