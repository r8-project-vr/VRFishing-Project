// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Takeuchi/Actor/Fish.h"
#include "Lee/component/FishingMenuNavigatorComponent.h"
#include "FishingSeControllerSubsystem.generated.h"

class AFish;
class UAudioComponent;
class USoundBase;
class UWidget;
class UFishingStateComponentBase;
class UFishingStateManagerComponent;
class UFishingReelStateComponent;

/**
 * @brief UI 効果音の種類。Project Settings「Fishing Audio Settings」の UI 音スロットと 1:1 対応する。
 * @note 効果音を追加するときは EFishingUiSfx に列挙値追加 ＋ 設定クラスにスロット追加 ＋
 *       UFishingSeControllerSubsystem の switch に case 追加の 3 ステップで行う（詳細は
 *       UFishingAudioSettings のクラスコメント参照）。魚の 3D 効果音（世界音）は本列挙に含まない。
 */
UENUM(BlueprintType)
enum class EFishingUiSfx : uint8
{
	MenuCursorMove UMETA(DisplayName = "メニュー移動音"),        /**< フォーカス移動のたび */
	MenuConfirm    UMETA(DisplayName = "メニュー決定音"),        /**< A ボタン・レーザークリック共通 */
	PhaseChange    UMETA(DisplayName = "フェーズ切替音"),        /**< よーい→うで→リール→つりあげ→リザルト */
	PhaseSuccess   UMETA(DisplayName = "フェーズ成功音"),        /**< 各ステート成功完了時 */
	PhaseFail      UMETA(DisplayName = "フェーズ失敗音"),        /**< 過速・過遅・停止タイムアウトなどのミス時 */
	ResultAppear   UMETA(DisplayName = "リザルト表示音"),        /**< ResultState 完了＝リザルト画面表示のタイミング */
	RpmJudgeChange UMETA(DisplayName = "RPM判定変化音")          /**< 遅すぎ／適正／速すぎ の判定切替時のみ */
};

/**
 * @brief ゲーム状態を単独で監視し、効果音（SE）を再生する WorldSubsystem（SE 制御の単一窓口）。
 * @note 増分機能として設計されており、既存クラス（AFish／VRPawn／GameMode／各 Widget／Navigator）には
 *       一切変更を加えない。検出は「公開デリゲートの購読」と「public UPROPERTY のポーリング」のみ。
 * @note 検出対象と再生の対応:
 *       - 魚の EFishState をポーリング → 暴れループ水音／咬みつき水音／逃走音／釣り上げ音（3D・魚に追従）
 *       - StateManager の OnFishingStateChanged → フェーズ切替音
 *       - 各ステートの OnFishingStateCompleted → 成功音／失敗音（ResultState 完了はリザルト表示音へ分流）
 *       - MenuNavigator の OnFocusChanged／OnMenuAction → メニュー移動音／決定音
 *       - ReelState の OnRPMCalculated ＋ LeeReelRpm 閾値 → RPM 判定変化音
 * @note 音源スロットは Project Settings「Fishing Audio Settings (釣りオーディオ設定)」で割り当てる。
 *       未設定（None）の音は完全な無音（エラー・警告なし）。素材後入れで動く。
 * @note BGM はレベル遷移をまたぐため二期で GameInstance 層の専用チャンネルとして追加する。
 *       本サブシステムは World 層なので BGM を扱わない（設計メモ: UFishingAudioSettings クラスコメント参照）。
 */
UCLASS()
class VRFISHING_API UFishingSeControllerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief UI 効果音（2D・非空間化）を再生する。
	 * @param SfxId 再生する効果音の種類
	 * @param VolumeMultiplier 個別倍率（設定の UiSfxVolume に乗算される）
	 * @note 音源未設定時は無音で戻る。BP から手動テストにも使える。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|Audio")
	void PlayUiSfx(EFishingUiSfx SfxId, float VolumeMultiplier = 1.0f);

	virtual void Deinitialize() override;

protected:
	/** @brief 専用サーバーでは生成しない（音響出力が存在しないため） */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** @brief 毎フレームの監視エントリ（内部で一定間隔に間引き） */
	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override;

private:
	/** @brief 魚 1 匹ぶんの監視エントリ（前回状態と鳴き中ボイスを保持する） */
	struct FFishingSeFishEntry
	{
		/** @brief 監視対象の魚（無効になったらエントリごと除去する） */
		TWeakObjectPtr<AFish> Fish;

		/** @brief 前回観測した EFishState（遷移検出用） */
		EFishState LastState = EFishState::Circling;

		/** @brief 鳴き中のボイス（魚に NewObject で追従させる AudioComponent。魚の破棄で自動停止） */
		TWeakObjectPtr<UAudioComponent> VoiceComponent;

		/** @brief ボイスがループ音（Struggling）か。自然終了時に再生し直す */
		bool bVoiceLoop = false;

		/** @brief Poking 開始からの経過累計（咬みつき音の周期再生用。魚ごとに独立） */
		float PokeSfxElapsed = 0.0f;
	};

	/** @brief RPM の判定分類（音の変化検出専用。ゲーム判定には使わない内部値） */
	enum class EFishingSeRpmJudge : uint8
	{
		None,
		TooSlow,
		Good,
		TooFast
	};

	// ==================== 監視・検出 ====================

	/** @brief 世界を再スキャンし、魚・Pawn・コンポーネントの監視対象とデリゲート購読を最新化する */
	void RefreshWorldWatch();

	/** @brief 釣り本編 Pawn（StateManager を持つ Pawn）のステート関連デリゲートを購読する */
	void BindFishingPawn(APawn* Pawn);

	/** @brief デリゲート購読をすべて解除する（Deinitialize 時） */
	void UnbindAll();

	/** @brief StateManager のステート変更通知（フェーズ切替音） */
	UFUNCTION()
	void HandleStateChanged(UFishingStateComponentBase* NewState);

	/**
	 * @brief 各ステートの完了通知（成功音／失敗音）。
	 * @note 動的デリゲートはハンドラ引数が厳密一致のため単一 bool のシグネチャとし、ResultState 完了は
	 *       バインド時の型判定で専用ハンドラ（HandleResultCompleted）へ振り分ける。
	 */
	UFUNCTION()
	void HandleStateCompleted(bool bIsSuccess);

	/** @brief ResultState 完了通知（リザルト表示音。GameMode.OnSetCompleted と同時機） */
	UFUNCTION()
	void HandleResultCompleted(bool bIsSuccess);

	/** @brief メニューのフォーカス移動通知（メニュー移動音） */
	UFUNCTION()
	void HandleFocusChanged(UWidget* FocusedWidget, EFishingTitleMenuAction FocusedAction);

	/** @brief メニューの確定通知（メニュー決定音） */
	UFUNCTION()
	void HandleMenuAction(EFishingTitleMenuAction Action);

	/** @brief リール 1 回転ごとの RPM 通知（判定が変わった瞬間のみ RPM 判定変化音） */
	UFUNCTION()
	void HandleRpmCalculated(float NewRPM);

	// ==================== 魚音チャンネル（3D・世界音） ====================

	/** @brief 監視中の全魚の状態遷移を処理し、ボイスの開始／停止／ループ維持を行う */
	void UpdateFishVoices(float DeltaTime);

	/** @brief 魚に追従する AudioComponent を生成して再生する（魚が Owner のため破棄時に自動停止） */
	void StartFishVoice(FFishingSeFishEntry& Entry, USoundBase* Sound, bool bLoop);

	/** @brief ボイスを停止して破棄する */
	void StopFishVoice(FFishingSeFishEntry& Entry);

	// ==================== 音源解決 ====================

	/** @brief UI 効果音の音源を設定から解決してロードする（未設定時 nullptr） */
	USoundBase* ResolveUiSfx(EFishingUiSfx SfxId) const;

	// ==================== 状態 ====================

	/** @brief 監視中の魚エントリ一覧 */
	TArray<FFishingSeFishEntry> FishEntries;

	/** @brief 購読済みの StateManager（本編 Pawn は 1 つという前提の単一保持） */
	TWeakObjectPtr<UFishingStateManagerComponent> BoundStateManager;

	/** @brief 購読済みの全ステートコンポーネント（完了通知の解除に使用） */
	TArray<TWeakObjectPtr<UFishingStateComponentBase>> BoundStateComponents;

	/** @brief 購読済みの RPM 算出元リールステート */
	TWeakObjectPtr<UFishingReelStateComponent> BoundReelState;

	/** @brief 購読済みのメニューナビゲーター */
	TWeakObjectPtr<UFishingMenuNavigatorComponent> BoundNavigator;

	/** @brief 世界再スキャンの間隔（秒）。破棄済み監視対象の掃除と新規対象の検出を兼ねる */
	float RefreshAccumulated = 0.0f;

	/** @brief Poking 中の咬みつき音の再生間隔（秒）。魚の PokeIntervalMin〜Max(2〜4 秒) の中間値 */
	float PokeSfxInterval = 3.0f;

	/** @brief 前回の RPM 判定（変化検出用） */
	EFishingSeRpmJudge LastRpmJudge = EFishingSeRpmJudge::None;

	/** @brief 反射による RPM 閾値の取得結果キャッシュ（取得時刻とセットで 0.25 秒間隔で更新）。
	 *  上限は「判定が実際に使った値」を毎回直読するため、ここでは未入力時の予測用に 2 本保持する */
	float CachedMinRPM = 0.0f;
	float CachedWheelMaxRPM = 0.0f;
	float CachedStickMaxRPM = 0.0f;
	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief ホイール用の下限 RPM（CachedMinRPM はスティック／ASerial 用。未入力時の予測に使用） */
	float CachedWheelMinRPM = 0.0f;
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	float RpmThresholdCacheTime = -1.0f;
};
