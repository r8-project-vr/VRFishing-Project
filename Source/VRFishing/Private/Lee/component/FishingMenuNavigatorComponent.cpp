// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/component/FishingMenuNavigatorComponent.h"
#include "Lee/subsystem/FishingLoadSettingsSubsystem.h"
#include "VRFishingLog.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/WidgetComponent.h"
#include "Components/Slider.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** 行内項目の追加用ヘルパー */
	void AddItem(FFishingMenuNavRow& Row, const TCHAR* WidgetName, EFishingTitleMenuAction Action)
	{
		FFishingMenuNavItem Item;
		Item.WidgetName = WidgetName;
		Item.Action = Action;
		Row.Items.Add(Item);
	}
}

UFishingMenuNavigatorComponent::UFishingMenuNavigatorComponent()
{
	// メニュー解決の再試行とスティック反復のために Tick を有効化
	PrimaryComponentTick.bCanEverTick = true;

	// WBP_TitleMenu の既定コントロール名でナビゲーション表を構築する
	FFishingMenuNavRow& MainRow = NavRows.AddDefaulted_GetRef();
	AddItem(MainRow, TEXT("Button_StartFishing"), EFishingTitleMenuAction::StartFishing);
	AddItem(MainRow, TEXT("Button_LoadSettings"), EFishingTitleMenuAction::LoadSettings);
	AddItem(MainRow, TEXT("Button_Back"), EFishingTitleMenuAction::Back);

	FFishingMenuNavRow& VerticalRow = NavRows.AddDefaulted_GetRef();
	AddItem(VerticalRow, TEXT("Button_VerticalLow"), EFishingTitleMenuAction::SetVerticalLow);
	AddItem(VerticalRow, TEXT("Button_VerticalMedium"), EFishingTitleMenuAction::SetVerticalMedium);
	AddItem(VerticalRow, TEXT("Button_VerticalHigh"), EFishingTitleMenuAction::SetVerticalHigh);

	FFishingMenuNavRow& RotationRow = NavRows.AddDefaulted_GetRef();
	AddItem(RotationRow, TEXT("Button_RotationLow"), EFishingTitleMenuAction::SetRotationLow);
	AddItem(RotationRow, TEXT("Button_RotationMedium"), EFishingTitleMenuAction::SetRotationMedium);
	AddItem(RotationRow, TEXT("Button_RotationHigh"), EFishingTitleMenuAction::SetRotationHigh);

	FFishingMenuNavRow& SliderRow = NavRows.AddDefaulted_GetRef();
	AddItem(SliderRow, TEXT("Slider_ExerciseTime"), EFishingTitleMenuAction::SliderRow);

	// 既定の入力アクションをアセットから読み込む（BP 側で上書き可能）
	static ConstructorHelpers::FObjectFinder<UInputAction> StickActionFinder(
		TEXT("/Game/Input/Actions/IA_DebugReelStick.IA_DebugReelStick"));
	if (StickActionFinder.Succeeded())
	{
		StickAction = StickActionFinder.Object;
	}
	else
	{
		UE_LOG(LogFishing, Warning, TEXT("[MenuNav] 既定のスティック入力アクションが読み込めません: /Game/Input/Actions/IA_DebugReelStick"));
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ConfirmActionFinder(
		TEXT("/Game/XRFramework/Input/Actions/IA_Menu_Interact_Left_Pressed.IA_Menu_Interact_Left_Pressed"));
	if (ConfirmActionFinder.Succeeded())
	{
		ConfirmAction = ConfirmActionFinder.Object;
	}
	else
	{
		UE_LOG(LogFishing, Warning, TEXT("[MenuNav] 既定の確定入力アクションが読み込めません: /Game/XRFramework/Input/Actions/IA_Menu_Interact_Left_Pressed"));
	}
}

void UFishingMenuNavigatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// メニュー解決と入力バインドが完了するまで再試行する
	if (!bInitialized)
	{
		TryInitialize();
		if (!bInitialized)
		{
			return;
		}
	}

	// スティックによるフォーカス移動（反復付き）
	HandleNavigation(DeltaTime);
}

void UFishingMenuNavigatorComponent::TryInitialize()
{
	// ==================== 1. メニューの UserWidget を解決 ====================
	if (!MenuUserWidget.IsValid())
	{
		if (UWidgetComponent* WidgetComponent = FindMenuWidgetComponent())
		{
			MenuUserWidget = WidgetComponent->GetUserWidgetObject();
		}
		if (!MenuUserWidget.IsValid())
		{
			return;
		}
	}

	// ==================== 2. ナビゲーション表のコントロールを名前で解決 ====================
	if (ResolvedWidgets.IsEmpty())
	{
		ResolvedWidgets.SetNum(NavRows.Num());
		int32 ResolvedCount = 0;
		for (int32 RowIndex = 0; RowIndex < NavRows.Num(); ++RowIndex)
		{
			for (const FFishingMenuNavItem& Item : NavRows[RowIndex].Items)
			{
				UWidget* Widget = MenuUserWidget->WidgetTree->FindWidget(Item.WidgetName);
				ResolvedWidgets[RowIndex].Add(Widget);
				if (Widget)
				{
					++ResolvedCount;
				}
				else
				{
					UE_LOG(LogFishing, Warning, TEXT("[MenuNav] コントロールが見つかりません: %s"), *Item.WidgetName.ToString());
				}
			}
		}

		// ひとつも解決できなければ表と実態が合っていないため、次フレームで再試行する
		if (ResolvedCount == 0)
		{
			ResolvedWidgets.Reset();
			return;
		}
	}

	// ==================== 3. 拡張入力へバインド（ポーンの入力コンポーネントは遅れて生成される） ====================
	if (!bInputBound)
	{
		UEnhancedInputComponent* EnhancedInput =
			GetOwner() ? Cast<UEnhancedInputComponent>(GetOwner()->InputComponent) : nullptr;
		if (!EnhancedInput || !StickAction || !ConfirmAction)
		{
			return;
		}

		EnhancedInput->BindAction(StickAction, ETriggerEvent::Triggered, this, &UFishingMenuNavigatorComponent::HandleStickInput);
		EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &UFishingMenuNavigatorComponent::HandleConfirmInput);
		bInputBound = true;
	}

	// ==================== 4. 初期フォーカスを設定して完了 ====================
	bInitialized = true;
	CurrentRowIndex = 0;
	CurrentColumnIndex = 0;
	SnapToNavigableItem();
	UpdateFocusVisual();

	UE_LOG(LogFishing, Log, TEXT("[MenuNav] 初期化完了: メニュー=%s 行数=%d 解決コントロール=%d"),
		*MenuUserWidget->GetName(), NavRows.Num(), ResolvedWidgets.Num());
}

UWidgetComponent* UFishingMenuNavigatorComponent::FindMenuWidgetComponent() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// クラス指定がある場合はその Actor を優先する
	if (MenuActorClass)
	{
		AActor* MenuActor = UGameplayStatics::GetActorOfClass(World, MenuActorClass);
		if (MenuActor)
		{
			return MenuActor->FindComponentByClass<UWidgetComponent>();
		}
		return nullptr;
	}

	// 未指定の場合は WidgetComponent を持つ最初の Actor を使用する（タイトルレベル想定）
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UWidgetComponent* WidgetComponent = (*It)->FindComponentByClass<UWidgetComponent>())
		{
			return WidgetComponent;
		}
	}
	return nullptr;
}

void UFishingMenuNavigatorComponent::HandleNavigation(float DeltaTime)
{
	if (!bNavigatorEnabled || !MenuUserWidget.IsValid())
	{
		return;
	}

	// パネル閉鎖などで現在フォーカス中の項目が操作不能になった場合、表示中の項目へ戻す
	if (!IsItemNavigable(CurrentRowIndex, CurrentColumnIndex))
	{
		SnapToNavigableItem();
	}

	if (LastStickValue.Size() < StickThreshold)
	{
		// ニュートラルへ戻ったら反復を解除する
		bStickWasNeutral = true;
		RepeatTimeAccumulated = 0.0f;
		return;
	}

	// 初回の倒し込み、または反復間隔の経過で移動する
	if (bStickWasNeutral || RepeatTimeAccumulated >= NavRepeatDelay)
	{
		if (FMath::Abs(LastStickValue.Y) >= FMath::Abs(LastStickValue.X))
		{
			// 上下方向の倒し込みが強い場合は行移動（スティック上 = Y 正 = 前の行）
			MoveRow(LastStickValue.Y < 0.0f);
		}
		else
		{
			// 左右方向の場合は列移動（スライダー行では運動時間のステップ増減）
			MoveColumn(LastStickValue.X > 0.0f);
		}
		bStickWasNeutral = false;
		RepeatTimeAccumulated = 0.0f;
	}
	else
	{
		RepeatTimeAccumulated += DeltaTime;
	}
}

void UFishingMenuNavigatorComponent::MoveRow(bool bNext)
{
	const int32 NumRows = NavRows.Num();
	if (NumRows <= 0)
	{
		return;
	}

	for (int32 Step = 1; Step <= NumRows; ++Step)
	{
		const int32 CandidateRow = ((CurrentRowIndex + (bNext ? Step : -Step)) % NumRows + NumRows) % NumRows;
		const int32 PreferredColumn = FMath::Clamp(CurrentColumnIndex, 0, NavRows[CandidateRow].Items.Num() - 1);
		const int32 CandidateColumn = FindNavigableColumnInRow(CandidateRow, PreferredColumn);
		if (CandidateColumn != INDEX_NONE)
		{
			if (CandidateRow != CurrentRowIndex || CandidateColumn != CurrentColumnIndex)
			{
				CurrentRowIndex = CandidateRow;
				CurrentColumnIndex = CandidateColumn;
				UpdateFocusVisual();
			}
			return;
		}
	}
}

void UFishingMenuNavigatorComponent::MoveColumn(bool bNext)
{
	if (!NavRows.IsValidIndex(CurrentRowIndex) || !NavRows[CurrentRowIndex].Items.IsValidIndex(CurrentColumnIndex))
	{
		return;
	}

	const FFishingMenuNavItem& CurrentItem = NavRows[CurrentRowIndex].Items[CurrentColumnIndex];

	// スライダー行は左右入力で運動時間をステップ増減する（フォーカス移動なし）
	if (CurrentItem.Action == EFishingTitleMenuAction::SliderRow)
	{
		if (const UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UFishingLoadSettingsSubsystem* Subsystem = GameInstance->GetSubsystem<UFishingLoadSettingsSubsystem>())
				{
					Subsystem->StepExerciseTime(bNext ? ExerciseTimeStepSeconds : -ExerciseTimeStepSeconds);
					// スライダー表示も直接更新する（BP 側のバインド不要）
					UpdateExerciseTimeSlider();
				}
			}
		}
		return;
	}

	const int32 NumColumns = NavRows[CurrentRowIndex].Items.Num();
	if (NumColumns <= 1)
	{
		return;
	}

	for (int32 Step = 1; Step < NumColumns; ++Step)
	{
		const int32 CandidateColumn = ((CurrentColumnIndex + (bNext ? Step : -Step)) % NumColumns + NumColumns) % NumColumns;
		if (IsItemNavigable(CurrentRowIndex, CandidateColumn))
		{
			CurrentColumnIndex = CandidateColumn;
			UpdateFocusVisual();
			return;
		}
	}
}

void UFishingMenuNavigatorComponent::ExecuteCurrentAction()
{
	if (!bInitialized || !NavRows.IsValidIndex(CurrentRowIndex) || !NavRows[CurrentRowIndex].Items.IsValidIndex(CurrentColumnIndex))
	{
		return;
	}

	const EFishingTitleMenuAction Action = NavRows[CurrentRowIndex].Items[CurrentColumnIndex].Action;

	// 負荷プリセットはここで直接サブシステムへ反映する
	UFishingLoadSettingsSubsystem* Subsystem = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			Subsystem = GameInstance->GetSubsystem<UFishingLoadSettingsSubsystem>();
		}
	}

	switch (Action)
	{
	case EFishingTitleMenuAction::SetVerticalLow:
		if (Subsystem) { Subsystem->SetVerticalLoad(EFishingLoadPreset::Low); }
		break;
	case EFishingTitleMenuAction::SetVerticalMedium:
		if (Subsystem) { Subsystem->SetVerticalLoad(EFishingLoadPreset::Medium); }
		break;
	case EFishingTitleMenuAction::SetVerticalHigh:
		if (Subsystem) { Subsystem->SetVerticalLoad(EFishingLoadPreset::High); }
		break;
	case EFishingTitleMenuAction::SetRotationLow:
		if (Subsystem) { Subsystem->SetRotationLoad(EFishingLoadPreset::Low); }
		break;
	case EFishingTitleMenuAction::SetRotationMedium:
		if (Subsystem) { Subsystem->SetRotationLoad(EFishingLoadPreset::Medium); }
		break;
	case EFishingTitleMenuAction::SetRotationHigh:
		if (Subsystem) { Subsystem->SetRotationLoad(EFishingLoadPreset::High); }
		break;
	case EFishingTitleMenuAction::SliderRow:
		// 運動時間は左右入力で調整するため、確定では何もしない
		break;
	case EFishingTitleMenuAction::StartFishing:
		// レベル遷移は C++ 側で実行する（WBP 側のバインド不要）
		if (const UWorld* World = GetWorld())
		{
			UGameplayStatics::OpenLevel(World, LevelToLoad);
		}
		OnMenuAction.Broadcast(Action);
		break;
	case EFishingTitleMenuAction::LoadSettings:
		// 負荷設定パネルを開く（BP 側バインド不要のため C++ で直接操作）
		OpenLoadSettingsPanel();
		OnMenuAction.Broadcast(Action);
		break;
	case EFishingTitleMenuAction::Back:
		// パネルを閉じて主メニューへ戻る
		CloseLoadSettingsPanel();
		OnMenuAction.Broadcast(Action);
		break;
	}
}

void UFishingMenuNavigatorComponent::UpdateFocusVisual()
{
	for (int32 RowIndex = 0; RowIndex < ResolvedWidgets.Num(); ++RowIndex)
	{
		for (int32 ColumnIndex = 0; ColumnIndex < ResolvedWidgets[RowIndex].Num(); ++ColumnIndex)
		{
			if (UWidget* Widget = ResolvedWidgets[RowIndex][ColumnIndex].Get())
			{
				const bool bFocused = (RowIndex == CurrentRowIndex && ColumnIndex == CurrentColumnIndex);
				Widget->SetRenderOpacity(bFocused ? 1.0f : DimmedOpacity);
			}
		}
	}

	// 独自のフォーカス表示を行いたい BP へ現在のフォーカスを通知する
	if (NavRows.IsValidIndex(CurrentRowIndex) && NavRows[CurrentRowIndex].Items.IsValidIndex(CurrentColumnIndex))
	{
		if (UWidget* FocusedWidget = ResolvedWidgets[CurrentRowIndex][CurrentColumnIndex].Get())
		{
			OnFocusChanged.Broadcast(FocusedWidget, NavRows[CurrentRowIndex].Items[CurrentColumnIndex].Action);
		}
	}
}

bool UFishingMenuNavigatorComponent::IsItemNavigable(int32 RowIndex, int32 ColumnIndex) const
{
	if (!NavRows.IsValidIndex(RowIndex) || !NavRows[RowIndex].Items.IsValidIndex(ColumnIndex))
	{
		return false;
	}

	const UWidget* Widget =
		(ResolvedWidgets.IsValidIndex(RowIndex) && ResolvedWidgets[RowIndex].IsValidIndex(ColumnIndex))
		? ResolvedWidgets[RowIndex][ColumnIndex].Get()
		: nullptr;
	if (!Widget)
	{
		return false;
	}

	// 非表示のコントロールには移動できない（パネル閉鎖中はその行が自動的に到達不能になる）
	const ESlateVisibility Visibility = Widget->GetVisibility();
	return Visibility != ESlateVisibility::Collapsed && Visibility != ESlateVisibility::Hidden;
}

void UFishingMenuNavigatorComponent::SnapToNavigableItem()
{
	for (int32 RowIndex = 0; RowIndex < NavRows.Num(); ++RowIndex)
	{
		const int32 NavigableColumn = FindNavigableColumnInRow(RowIndex, 0);
		if (NavigableColumn != INDEX_NONE)
		{
			CurrentRowIndex = RowIndex;
			CurrentColumnIndex = NavigableColumn;
			return;
		}
	}
}

int32 UFishingMenuNavigatorComponent::FindNavigableColumnInRow(int32 RowIndex, int32 PreferredColumn) const
{
	if (!NavRows.IsValidIndex(RowIndex))
	{
		return INDEX_NONE;
	}

	const int32 NumColumns = NavRows[RowIndex].Items.Num();
	const int32 StartColumn = FMath::Clamp(PreferredColumn, 0, NumColumns - 1);

	// 希望列から近い順に（右方向・左方向へ交互に）表示中の列を探す
	for (int32 Offset = 0; Offset < NumColumns; ++Offset)
	{
		const int32 Signs[2] = { 1, -1 };
		for (const int32 Sign : Signs)
		{
			if (Offset == 0 && Sign < 0)
			{
				continue;
			}
			const int32 CandidateColumn = ((StartColumn + Sign * Offset) % NumColumns + NumColumns) % NumColumns;
			if (IsItemNavigable(RowIndex, CandidateColumn))
			{
				return CandidateColumn;
			}
		}
	}
	return INDEX_NONE;
}

void UFishingMenuNavigatorComponent::HandleStickInput(const FInputActionValue& Value)
{
	// 入力値は Tick 側で反復制御つきで処理するためここでは保持のみ
	LastStickValue = Value.Get<FVector2D>();
}

void UFishingMenuNavigatorComponent::HandleConfirmInput(const FInputActionValue& Value)
{
	if (bNavigatorEnabled)
	{
		ExecuteCurrentAction();
	}
}

void UFishingMenuNavigatorComponent::OpenLoadSettingsPanel()
{
	if (!MenuUserWidget.IsValid())
	{
		return;
	}

	// 負荷設定パネルを表示し、主メニューを非表示にする
	if (UWidget* Panel = MenuUserWidget->WidgetTree->FindWidget(TEXT("Border_LoadSettings")))
	{
		Panel->SetVisibility(ESlateVisibility::Visible);
	}
	if (UWidget* Menu = MenuUserWidget->WidgetTree->FindWidget(TEXT("VerticalBox_TitleMenu")))
	{
		Menu->SetVisibility(ESlateVisibility::Collapsed);
	}

	// パネル内の最初の操作可能項目へフォーカスを移す
	SnapToNavigableItem();
}

void UFishingMenuNavigatorComponent::CloseLoadSettingsPanel()
{
	if (!MenuUserWidget.IsValid())
	{
		return;
	}

	// 主メニューを表示し、負荷設定パネルを非表示にする
	if (UWidget* Menu = MenuUserWidget->WidgetTree->FindWidget(TEXT("VerticalBox_TitleMenu")))
	{
		Menu->SetVisibility(ESlateVisibility::Visible);
	}
	if (UWidget* Panel = MenuUserWidget->WidgetTree->FindWidget(TEXT("Border_LoadSettings")))
	{
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 主メニュー（LoadSettings ボタン）へフォーカスを戻す
	SnapToNavigableItem();
}

void UFishingMenuNavigatorComponent::UpdateExerciseTimeSlider()
{
	if (!MenuUserWidget.IsValid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFishingLoadSettingsSubsystem* Subsystem =
		GameInstance ? GameInstance->GetSubsystem<UFishingLoadSettingsSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	if (USlider* Slider = Cast<USlider>(MenuUserWidget->WidgetTree->FindWidget(TEXT("Slider_ExerciseTime"))))
	{
		Slider->SetValue(Subsystem->GetExerciseSliderValue());
	}
}
