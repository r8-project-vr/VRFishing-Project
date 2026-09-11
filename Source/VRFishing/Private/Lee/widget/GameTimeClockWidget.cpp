// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/GameTimeClockWidget.h"
#include "Tanimura/FishingGameModeBase.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "VRFishingLog.h"

namespace
{
	/**
	 * @brief 時計角度（度・0=真上・正=時計回り）からローカル空間の方向ベクトルへ変換する。
	 * @note 画面座標は Y が下向きのため -Cos を使う。
	 *       検算：0°→(0,-1)=真上、90°→(1,0)=右、180°→(0,1)=真下。
	 */
	// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// ユニティビルドで RpmGaugeWidget.cpp の同名ヘルパーと衝突するため改名
	FVector2f ClockAngleToDir(float AngleDeg)
	//FVector2f GaugeAngleToDir(float AngleDeg)
	// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	{
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		return FVector2f(FMath::Sin(AngleRad), -FMath::Cos(AngleRad));
	}
}

UGameTimeClockWidget::UGameTimeClockWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** @brief 生成時処理。時間の取得は Tick の定期更新に任せる */
void UGameTimeClockWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

/** @brief 毎フレーム処理。時間の定期取得 → 弧の長さの平滑追従 → 前景弧の再構築 → 中心テキスト → トラック円キャッシュ更新 */
void UGameTimeClockWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const double Now = FPlatformTime::Seconds();

	// ---- 時間の定期更新 ----
	// LoadApplier が Pawn BeginPlay で TotalGameTime を書き替えるため NativeConstruct 時点の
	// 1 回きり読み取りは禁止
	if (Now - LastTimeRefreshTime >= TimeRefreshInterval)
	{
		LastTimeRefreshTime = Now;
		RefreshTimeSource();
	}

	// ---- 弧の長さの平滑追従 ----
	// 初回取得時は補間せずスナップする（空から満円まで伸びる演出を防ぐ）
	if (bHasEverReceivedTime)
	{
		DisplayedRemainingSeconds = FMath::FInterpTo(DisplayedRemainingSeconds, TargetRemainingSeconds, InDeltaTime, ArcSmoothingSpeed);
		DisplayedRemainingSeconds = FMath::Clamp(DisplayedRemainingSeconds, 0.0f, FMath::Max(DisplayFullScaleSeconds, 0.0f));
	}
	else if (TargetRemainingSeconds >= 0.0f)
	{
		DisplayedRemainingSeconds = TargetRemainingSeconds;
		bHasEverReceivedTime = true;
	}

	// ---- 前景弧の再構築（長さが毎フレーム変わるためキャッシュしない） ----
	UpdateForegroundArc();

	// ---- 中心テキスト（残り時間 M:SS／ロスタイム中は LossTimeText。値が変わった時だけ SetText） ----
	if (Text_Remaining && bHasEverReceivedTime)
	{
		if (bLossTime)
		{
			// ロスタイムへ切り替わった瞬間だけ SetText する（毎フレームの SetText を避ける）
			if (!bLossTimeTextShown)
			{
				bLossTimeTextShown = true;
				LastDisplayedTotalSeconds = -1; // 通常表示へ戻った際に必ず再反映させる
				Text_Remaining->SetText(LossTimeText);
			}
		}
		else
		{
			bLossTimeTextShown = false;
			const int32 TotalSeconds = FMath::RoundToInt(DisplayedRemainingSeconds);
			if (TotalSeconds != LastDisplayedTotalSeconds)
			{
				LastDisplayedTotalSeconds = TotalSeconds;
				const int32 Minutes = TotalSeconds / 60;
				const int32 Seconds = TotalSeconds % 60;
				Text_Remaining->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Seconds)));
			}
		}
	}

	// ---- トラック円キャッシュの更新（サイズ／パラメータ変化時のみ再構築） ----
	const FVector2f LocalSize(MyGeometry.GetLocalSize());
	const bool bSizeChanged =
		(FMath::Abs(LocalSize.X - CachedGeometrySize.X) > 0.5f) ||
		(FMath::Abs(LocalSize.Y - CachedGeometrySize.Y) > 0.5f);
	const uint32 ParamsHash = ComputeDrawParamsHash();
	if (bDrawCacheDirty || bSizeChanged || ParamsHash != CachedDrawParamsHash)
	{
		CachedGeometrySize = LocalSize;
		CachedDrawParamsHash = ParamsHash;
		RebuildDrawCache();
	}
}

/** @brief 自前描画。トラック円（満円の軌跡）→ 残り時間の緑弧 の順でレイヤを積む */
int32 UGameTimeClockWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// 文字盤テクスチャがある／自前描画オフの場合は何も描かない（子 Image は親の OnPaint より
	// 後に描かれるため、文字盤テクスチャ設置＝全面差し替え）
	if (!bSelfDrawDial || Image_DialFace || CachedRadius <= 0.0f)
	{
		return BaseLayer;
	}

	int32 CurrentLayer = BaseLayer;

	// ---- トラック円（満円の軌跡。時間が無くなっても満円の位置が分かるように下に敷く） ----
	if (bDrawTrack && TrackArcPoints.Num() >= 2)
	{
		++CurrentLayer;
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), TrackArcPoints, ESlateDrawEffect::None, TrackColor, true, CachedBandThickness);
	}

	// ---- 残り時間の前景弧（ロスタイム中は満円の黄になる。色はキャッシュ外のため Paint で直読） ----
	if (ForegroundArcPoints.Num() >= 2)
	{
		++CurrentLayer;
		FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayer, AllottedGeometry.ToPaintGeometry(), ForegroundArcPoints, ESlateDrawEffect::None, bLossTime ? LossTimeColor : RingColor, true, CachedBandThickness);
	}

	return CurrentLayer;
}

// ==================== 内部処理 ====================

void UGameTimeClockWidget::RefreshTimeSource()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CachedGameMode = World->GetAuthGameMode<AFishingGameModeBase>();
	if (!CachedGameMode)
	{
		// GameMode の無いレベル（タイトル／リザルト）では静かに諦める（現在の表示を保持）
		bTimeSourceResolved = false;
		return;
	}

	bTimeSourceResolved = true;

	// 満量程は公開プロパティ 2 つの和で毎回算出する。GetRemainingTime が Max(0) でクランプするため
	// 時間切れ時（CurrentGameTime == TotalGameTime）も等式が成立する。キャッシュは不要
	DisplayFullScaleSeconds = FMath::Max(CachedGameMode->CurrentGameTime + CachedGameMode->RemainingTime, 0.0f);
	TargetRemainingSeconds = CachedGameMode->RemainingTime;
	// ロスタイム判定（時間切れ後も最終セット完了までは続行する期間。公開プロパティのため直読み）
	bLossTime = CachedGameMode->bIsTimeUp;

	// ---- 値が動いた瞬間だけログ（0.25 秒ごとに流さない。HMD 内でも確認できる検証手段） ----
	if (!FMath::IsNearlyEqual(DisplayFullScaleSeconds, LastLoggedFullScale, 0.05f)
		|| bLossTime != LastLoggedLossTime)
	{
		LastLoggedFullScale = DisplayFullScaleSeconds;
		LastLoggedLossTime = bLossTime;
		UE_LOG(LogFishing, Log, TEXT("[GameTimeClock] 満量程=%.1f 残り=%.1f 経過=%.1f ロスタイム=%d"),
			DisplayFullScaleSeconds, CachedGameMode->RemainingTime, CachedGameMode->CurrentGameTime, bLossTime ? 1 : 0);
	}
}

float UGameTimeClockWidget::RemainingToRatio(float RemainingSeconds) const
{
	// 残り時間比 1.0（満タン）→ 全円、0.0（時間切れ）→ 空 の線形写像
	return FMath::Clamp(RemainingSeconds / FMath::Max(DisplayFullScaleSeconds, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
}

void UGameTimeClockWidget::UpdateForegroundArc()
{
	// 帯の中心円はリング半径より内側に置く（帯が外に溢れないように厚みの半分だけ下げる）。
	// 初フレームなどキャッシュ未構築（半径 0）の間は弧を作らない
	ForegroundArcPoints.Reset();
	if (CachedRadius <= 0.0f)
	{
		return;
	}

	const float BandRadius = CachedRadius * (1.0f - RingThicknessRatio * 0.5f);
	const int32 Segments = FMath::Max(ArcSegmentCount, 2);

	// ロスタイム中は残り時間が 0 のままため、満円の黄弧を表示する（状態の可視化が目的）
	const float Ratio = bLossTime ? 1.0f : RemainingToRatio(DisplayedRemainingSeconds);
	if (Ratio <= 0.001f)
	{
		return; // 時間切れ／未取得は弧なし（トラック円のみ）
	}

	const float SweepDeg = 360.0f * Ratio;
	for (int32 k = 0; k <= Segments; ++k)
	{
		const float ArcT = static_cast<float>(k) / static_cast<float>(Segments);
		// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
		ForegroundArcPoints.Add(CachedCenter + ClockAngleToDir(RingStartAngleDeg + SweepDeg * ArcT) * BandRadius);
		//ForegroundArcPoints.Add(CachedCenter + GaugeAngleToDir(RingStartAngleDeg + SweepDeg * ArcT) * BandRadius);
		// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
	}
}

void UGameTimeClockWidget::RebuildDrawCache()
{
	CachedRadius = FMath::Max(FMath::Min(CachedGeometrySize.X, CachedGeometrySize.Y) * GaugeRadiusRatio, 1.0f);
	CachedCenter = FVector2f(static_cast<float>(CachedGeometrySize.X * GaugeCenterRatio.X), static_cast<float>(CachedGeometrySize.Y * GaugeCenterRatio.Y));
	CachedBandThickness = CachedRadius * RingThicknessRatio;

	// ---- トラック円（満円・閉曲線。区間が空でも必ず頂点を捨てる） ----
	TrackArcPoints.Reset();
	if (bDrawTrack)
	{
		const float BandRadius = CachedRadius * (1.0f - RingThicknessRatio * 0.5f);
		const int32 Segments = FMath::Max(ArcSegmentCount, 2);
		for (int32 k = 0; k <= Segments; ++k)
		{
			const float ArcT = static_cast<float>(k) / static_cast<float>(Segments);
			// 2026.09.11 Tanimura startーーーーーーーーーーーーーーーーーーーーーーーーーーー
			TrackArcPoints.Add(CachedCenter + ClockAngleToDir(RingStartAngleDeg + 360.0f * ArcT) * BandRadius);
			//TrackArcPoints.Add(CachedCenter + GaugeAngleToDir(RingStartAngleDeg + 360.0f * ArcT) * BandRadius);
			// 2026.09.11 Tanimura endーーーーーーーーーーーーーーーーーーーーーーーーーーー
		}
	}

	bDrawCacheDirty = false;
}

uint32 UGameTimeClockWidget::ComputeDrawParamsHash() const
{
	// 色は Paint で直読するため混ぜない。前景弧の頂点は毎フレーム再構築のため満量程も不要
	uint32 Hash = GetTypeHash(ArcSegmentCount);
	Hash = HashCombine(Hash, GetTypeHash(bDrawTrack));
	Hash = HashCombine(Hash, GetTypeHash(RingThicknessRatio));
	Hash = HashCombine(Hash, GetTypeHash(RingStartAngleDeg));
	Hash = HashCombine(Hash, GetTypeHash(GaugeCenterRatio.X));
	Hash = HashCombine(Hash, GetTypeHash(GaugeCenterRatio.Y));
	Hash = HashCombine(Hash, GetTypeHash(GaugeRadiusRatio));
	return Hash;
}
