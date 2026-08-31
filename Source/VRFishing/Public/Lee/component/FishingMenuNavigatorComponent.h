// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "FishingMenuNavigatorComponent.generated.h"

class UInputAction;
class UWidget;
class UUserWidget;
class UWidgetComponent;
class AActor;

/**
 * @brief タイトルメニューの各項目が確定されたときの動作。
 */
UENUM(BlueprintType)
enum class EFishingTitleMenuAction : uint8
{
	SetVerticalLow    UMETA(DisplayName = "上下運動:低"),
	SetVerticalMedium UMETA(DisplayName = "上下運動:中"),
	SetVerticalHigh   UMETA(DisplayName = "上下運動:高"),
	SetRotationLow    UMETA(DisplayName = "リール:低"),
	SetRotationMedium UMETA(DisplayName = "リール:中"),
	SetRotationHigh   UMETA(DisplayName = "リール:高"),
	SliderRow         UMETA(DisplayName = "運動時間スライダー行"),
	StartFishing      UMETA(DisplayName = "釣り開始"),
	LoadSettings      UMETA(DisplayName = "負荷設定を開く"),
	Back              UMETA(DisplayName = "戻る")
};

/** @brief ナビゲーション表のひとつの項目（コントロール名と確定動作の対応） */
USTRUCT(BlueprintType)
struct FFishingMenuNavItem
{
	GENERATED_BODY()

	/** WBP_TitleMenu 上のコントロール名（WidgetTree->FindWidget で解決する） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	FName WidgetName;

	/** この項目を確定したときの動作 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	EFishingTitleMenuAction Action = EFishingTitleMenuAction::StartFishing;
};

/** @brief ナビゲーション表のひとつの行（左右入力で行内を移動する項目の集まり） */
USTRUCT(BlueprintType)
struct FFishingMenuNavRow
{
	GENERATED_BODY()

	/** 行内の項目（左から右の順） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	TArray<FFishingMenuNavItem> Items;
};

/** @brief メニュー動作の確定通知（StartFishing / LoadSettings / Back など BP 側で処理する動作用） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuAction, EFishingTitleMenuAction, Action);

/** @brief フォーカス移動の通知（BP 側で独自のフォーカス表示をする場合に使用） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFocusChanged, UWidget*, FocusedWidget, EFishingTitleMenuAction, FocusedAction);

/**
 * @brief タイトルメニューをスティック＋Aボタンで操作するナビゲーター（レーザーポインタ不要）。
 * @note BP_XRPawn に手動で追加して使用する。上下入力で行移動、左右入力で行内移動、
 *       Aボタン（IA_DebugTitleConfirm）で現在フォーカス中の項目を確定する。
 * @note Collapse 済みの項目には移動できないため、負荷設定パネルの開閉とナビゲーション範囲が自動で同期する。
 * @note メニュー Actor・入力コンポーネントは生成順序が保証されないため、Tick で準備完了を再試行する。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingMenuNavigatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFishingMenuNavigatorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==================== 設定 ====================

	/** ナビゲーション表（行＝上下移動、行内＝左右移動）。既定値は WBP_TitleMenu のコントロール名で構築済み */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	TArray<FFishingMenuNavRow> NavRows;

	/** メニュー Actor のクラス（BP_TitleMenu を指定。未指定の場合は WidgetComponent を持つ最初の Actor を使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	TSubclassOf<AActor> MenuActorClass;

	/** スティック入力（ナビゲーション用）。既定で IA_DebugReelStick を読み込む */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	TObjectPtr<UInputAction> StickAction;

	/** 確定入力（Aボタン）。既定で IA_DebugTitleConfirm を読み込む */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	TObjectPtr<UInputAction> ConfirmAction;

	/** スティックを倒し続けたときの移動の反復間隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav", meta = (ClampMin = "0.1"))
	float NavRepeatDelay = 0.35f;

	/** スティック入力を傾きとみなす閾値 (0.0〜1.0)。倒し込み開始の判定に使う */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StickThreshold = 0.6f;

	/**
	 * @brief ニュートラルへ戻ったとみなす閾値の比率（StickThreshold × この値）。
	 * @note 単一閾値で判定すると回中残差が閾値付近で振動した際に中性→倒し込みが
	 *       繰り返され、フォーカスが勝手に連続移動（漂い）する。ヒステリシスで防止する。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float StickReleaseThresholdRatio = 0.5f;

	/** 非フォーカス項目の RenderOpacity（フォーカス中は 1.0） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimmedOpacity = 0.5f;

	/** スライダー行の左右入力での運動時間のステップ幅（秒）。0 = Project Settings のステップ幅に自動追従 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav", meta = (ClampMin = "0.0"))
	float ExerciseTimeStepSeconds = 0.0f;

	/** メニュー解決の再試行を諦めるまでの時間（秒）。ゲーム本編などメニューがないレベルではここで探索を打ち切る */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav", meta = (ClampMin = "1.0"))
	float InitRetryTimeoutSeconds = 5.0f;

	/** ナビゲーターの有効フラグ（ゲーム本編へ遷移後などに無効化する用途） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	bool bNavigatorEnabled = true;

	/** StartFishing 確定時に遷移するレベル名（パッケージ名） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|MenuNav")
	FName LevelToLoad = TEXT("LV_MainGame");

	// ==================== 通知 ====================

	/** StartFishing / LoadSettings / Back の確定通知（BP 側でレベル遷移などを処理する） */
	UPROPERTY(BlueprintAssignable, Category = "Fishing|MenuNav")
	FOnMenuAction OnMenuAction;

	/** フォーカス移動の通知（BP 側で独自のフォーカス表示をする場合に使用） */
	UPROPERTY(BlueprintAssignable, Category = "Fishing|MenuNav")
	FOnFocusChanged OnFocusChanged;

private:
	/** 解決済みメニュー Widget（Tick での再試行に備えて弱参照で保持） */
	TWeakObjectPtr<UUserWidget> MenuUserWidget;

	/** NavRows に対応する解決済みコントロール（ResolvedWidgets[行][列]） */
	TArray<TArray<TWeakObjectPtr<UWidget>>> ResolvedWidgets;

	/** 現在フォーカス中の行インデックス */
	int32 CurrentRowIndex = 0;

	/** 現在フォーカス中の列インデックス */
	int32 CurrentColumnIndex = 0;

	/** メニュー解決と入力バインドが完了済みか */
	bool bInitialized = false;

	/** 拡張入力のバインド済みフラグ */
	bool bInputBound = false;

	/** メニュー解決の再試行の累積時間（秒） */
	float InitRetryTimeAccumulated = 0.0f;

	/** マウス/レーザー用ボタンの OnClicked バインド済みフラグ */
	bool bMouseActionsBound = false;

	/** スティックがニュートラルへ戻ったか（反復制御用） */
	bool bStickWasNeutral = true;

	/**
	 * @brief 倒し込みシーケンス中に固定された移動（0=未確定、1=前行、2=次行、3=左、4=右）。
	 * @note リピート毎に軸を再判定すると斜め入力で行／列が交互に切り替わり、
	 *       フォーカスが意図しない方向へ漂うため、ニュートラルへ戻るまで固定する。
	 */
	int8 LockedMove = 0;

	/** スティック反復の累積時間（秒） */
	float RepeatTimeAccumulated = 0.0f;

	/** 最新のスティック入力値 */
	FVector2D LastStickValue = FVector2D::ZeroVector;

	/** @brief メニュー解決・入力バインドを試みる（未完了なら次フレームで再試行） */
	void TryInitialize();

	/** @brief メニューの WidgetComponent を探索する */
	UWidgetComponent* FindMenuWidgetComponent() const;

	/** @brief スティック入力からフォーカス移動とスライダー値の増減を処理する */
	void HandleNavigation(float DeltaTime);

	/** @brief 行を移動する（bNext=true で次の行、false で前の行） */
	void MoveRow(bool bNext);

	/** @brief 列を移動する（bNext=true で右、false で左）。スライダー行は運動時間のステップ増減を行う */
	void MoveColumn(bool bNext);

	/** @brief 現在フォーカス中の項目を確定する */
	void ExecuteCurrentAction();

	/** @brief フォーカス表示を更新する（RenderOpacity と OnFocusChanged の通知） */
	void UpdateFocusVisual();

	/** @brief 指定項目がナビゲーション可能（表示中）か */
	bool IsItemNavigable(int32 RowIndex, int32 ColumnIndex) const;

	/** @brief 指定動作の項目へフォーカスを移す（表示中の項目のみ。移動できたら true） */
	bool FocusAction(EFishingTitleMenuAction Action);

	/** @brief 現在のフォーカスが不可視になった場合、表示中の項目へフォーカスを移し直す */
	void SnapToNavigableItem();

	/** @brief 指定行内の PreferredColumn に最も近い表示中の列を探す */
	int32 FindNavigableColumnInRow(int32 RowIndex, int32 PreferredColumn) const;

	/** @brief スティック入力のハンドラー（値を保持して Tick 側で処理する） */
	void HandleStickInput(const FInputActionValue& Value);

	/** @brief スティック入力の終了ハンドラー（保持値をゼロに戻して反復移動を止める） */
	void HandleStickInputEnd(const FInputActionValue& Value);

	/** @brief 負荷設定パネルを開き、主メニューを非表示にする */
	void OpenLoadSettingsPanel();

	/** @brief 負荷設定パネルを閉じて主メニューを表示する */
	void CloseLoadSettingsPanel();

	/** @brief スライダーへ現在の運動時間の表示位置を反映する */
	void UpdateExerciseTimeSlider();

	/** @brief 指定動作を実行する（フォーカス位置に依らず呼べる抽出版） */
	void ExecuteAction(EFishingTitleMenuAction Action);

	/** @brief マウス/レーザーで直接クリックできるよう、WBP 側に OnClicked バインドがないボタン（StartFishing / Back）へのみバインドする */
	void BindMouseOnlyActions();

	/** @brief 確定入力（Aボタン）のハンドラー */
	void HandleConfirmInput(const FInputActionValue& Value);

	/** @brief StartFishing ボタンの OnClicked ハンドラー（マウス/レーザーで直接クリックされたとき） */
	UFUNCTION()
	void HandleStartFishingClicked();

	/** @brief Back ボタンの OnClicked ハンドラー（マウス/レーザーで直接クリックされたとき） */
	UFUNCTION()
	void HandleBackClicked();
};
