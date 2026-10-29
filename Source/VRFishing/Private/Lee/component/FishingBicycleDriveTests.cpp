// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

// ============================================================================
// 自転車デバイス駆動（完走周期イベント経路）の自動テスト。
//
// 対象:
//   - UFishingReelStateComponent::ProcessDeviceCycle（標定 RPM 直用・逆方向無視・
//     測定中は推進のみ・イベント実時刻で停止基準更新・完了時に即打ち切り）
//   - UBicycleBridgeComponent の純関数（方向判定・履歴再構築判定）
//
// 実行タイミング（2026.10.29 Lee(3)）:
//   - フィルタは EditorContext + EngineFilter（手動/コマンド起動専用）。
//     SmokeFilter を付けるとエディタ起動フェーズで自動実行され、TypedElementRegistry
//     が未初期化の状態でコンポーネント UObject を生成して断言落ちするため禁止。
//   - 起動方法: エディタ完全起動後に「Automation RunTests VRFishing.BicycleDrive」
//    （コマンドレット / 自動化テスト画面のどちらでも可。既定 TestTier=Engine で選択される）
//
// 対象外:
//   - SimulateReelByStick / SimulateReelByWheel / UpdateState は GetWorld() の
//     ゲーム時刻に依存するため、World 無しで実行できる本テスト群では対象外
//    （入力経路そのものは変更しておらず、実機/PIE での回帰確認を優先する）。
// ============================================================================

#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Lee/component/BicycleBridgeComponent.h"
// 2026.10.29 Lee(4): フレーム分類（ClassifyBicycleFrame 純関数）のテスト用。
// ワーカー ヘッダはプラグイン型を前方宣言のみで参照するため、この include で
// プラグイン依存（Windows 専用ヘッダ）がテストへ漏れることはない
#include "Lee/device/FishingSerialWorker.h"
// 2026.10.29 Lee(6): FBicycleCycleEventCore（GetRpm の二重防御）を直接実測するための include
#include "Lee/device/FishingWiredDeviceTypes.h"
// 2026.10.29 Lee(5) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
// デコーダ単体の挙動マトリクスと docs 実測 JSON 回放のテスト用
#include "Lee/device/FishingBicycleMotionDecoder.h"
#include "Lee/device/FishingAserialFrameParser.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <limits>
// 2026.10.29 Lee(5) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 2026.10.29 Lee: FAutomationTestBase 等は Core の Misc/ 配下にある（ルート直下ではない）
#include "Misc/AutomationTest.h"
// 2026.10.29 Lee: NewObject / GetTransientPackage は UObjectGlobals で宣言される
#include "UObject/UObjectGlobals.h"
// 2026.10.29 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
// UHT は cpp 内 UCLASS の generated.h を生成しないため、テスト用シンク
//（UTestRpmSink）はテスト専用ヘッダへ切り出した（generated.h はそちら側に置く）
#include "FishingBicycleDriveTestsHelper.h"
// 2026.10.29 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

namespace
{
	/**
	 * @brief テスト用の活性リールを生成する（World 非依存）。
	 * @note  RegisterComponent を行わず Activate() だけで IsActive を立てる。
	 *        ProcessDeviceCycle は GetWorld() に依存しないためこの形で検証できる。
	 *        RF_Transient を付けてシリアライズ/保存系の経路から切り離す。
	 *        本関数はエディタ完全起動後（手動/コマンド起動のテスト実行時）にのみ
	 *        呼ばれる前提であり、起動フェーズで自動実行されないことをフィルタ側で保証する。
	 */
	UFishingReelStateComponent* MakeActiveReel()
	{
		UFishingReelStateComponent* Reel = NewObject<UFishingReelStateComponent>(
			GetTransientPackage(), NAME_None, RF_Transient);
		Reel->Activate();
		return Reel;
	}

	/**
	 * @brief テスト用の RPM シンクを生成してリールへ接続する。
	 */
	UTestRpmSink* ConnectSink(UFishingReelStateComponent* Reel)
	{
		UTestRpmSink* Sink = NewObject<UTestRpmSink>(GetTransientPackage());
		Reel->OnRPMCalculated.AddDynamic(Sink, &UTestRpmSink::OnRpm);
		return Sink;
	}

	// 2026.10.29 Lee(5): 状態列を一定間隔でデコーダへ投入する（RpmMonitor.cs の FeedSeq 相当）
	void FeedSeq(FBicycleMotionDecoder& Decoder, double T0, double Dt, const int* States, int Count)
	{
		for (int i = 0; i < Count; ++i)
		{
			Decoder.Feed(T0 + Dt * i, States[i]);
		}
	}

	// 2026.10.29 Lee(6) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief デコーダ生イベント（FBicycleCycleEvent）の平均間隔の有効判定。
	 * @note  デコーダ生イベントには IsMeasuring() が無いため、Core 側（FBicycleCycleEventCore::
	 *        IsMeasuring）と同一の口径（厳密に正かつ有限）をこの共有式で判定する。
	 *        検証の弱化はしない（口径自体を直接比較する）。
	 */
	bool IsValidAvgInterval(double AvgIntervalSec)
	{
		return AvgIntervalSec > 0.0 && FMath::IsFinite(AvgIntervalSec);
	}

	/**
	 * @brief 平均間隔を Core 構造体へ転写して GetRpm の実値を実測する。
	 * @note  GetRpm（無効値の二重防御込み）そのものを検証対象とするため、
	 *        生イベントの平均間隔を Core へ渡して戻り値を比較する。
	 */
	double CoreRpmFrom(double AvgIntervalSec)
	{
		FBicycleCycleEventCore Core;
		Core.AvgIntervalSec = AvgIntervalSec;
		return Core.GetRpm();
	}
	// 2026.10.29 Lee(6) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// 2026.10.29 Lee(5) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 16 進 1 文字を値へ変換する（不正文字は -1） */
	int32 HexNibble(TCHAR C)
	{
		if (C >= TEXT('0') && C <= TEXT('9')) { return C - TEXT('0'); }
		if (C >= TEXT('A') && C <= TEXT('F')) { return C - TEXT('A') + 10; }
		if (C >= TEXT('a') && C <= TEXT('f')) { return C - TEXT('a') + 10; }
		return -1;
	}

	/**
	 * @brief 回放 JSON（"ms":数値 と "hex":"D0-..-.." の対の列挙）を走査して
	 *        フレーム配列（時刻[秒]とバイト列）を組み立てる。
	 * @note  同一 ms 内の複数フレームは同一時刻になる（同時刻周期の自然な再現にもなる）。
	 */
	void ScanDocsFrames(const FString& Json, TArray<double>& OutTimes, TArray<TArray<uint8>>& OutByteBlocks)
	{
		int Cursor = 0;
		double CurMs = 0.0;
		while (true)
		{
			const int MsIdx = Json.Find(TEXT("\"ms\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			const int HexIdx = Json.Find(TEXT("\"hex\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
			if (MsIdx == INDEX_NONE && HexIdx == INDEX_NONE)
			{
				break;
			}

			if (HexIdx != INDEX_NONE && (MsIdx == INDEX_NONE || HexIdx < MsIdx))
			{
				// "hex" : "D0-01-.." の文字列部を取り出す
				const int Quote1 = Json.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, HexIdx + 5);
				if (Quote1 == INDEX_NONE) { break; }
				const int Quote2 = Json.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Quote1 + 1);
				if (Quote2 == INDEX_NONE) { break; }

				TArray<uint8> Bytes;
				TArray<FString> Parts;
				Json.Mid(Quote1 + 1, Quote2 - Quote1 - 1).ParseIntoArray(Parts, TEXT("-"));
				for (const FString& Part : Parts)
				{
					if (Part.Len() < 2) { continue; }
					const int32 Hi = HexNibble(Part[0]);
					const int32 Lo = HexNibble(Part[1]);
					if (Hi >= 0 && Lo >= 0)
					{
						Bytes.Add(static_cast<uint8>(Hi * 16 + Lo));
					}
				}
				OutTimes.Add(CurMs * 0.001);
				OutByteBlocks.Add(MoveTemp(Bytes));
				Cursor = Quote2 + 1;
			}
			else
			{
				// "ms" : 123.45 の数値部を取り出す
				const int Colon = Json.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, MsIdx + 4);
				if (Colon == INDEX_NONE) { break; }
				int Start = Colon + 1;
				while (Start < Json.Len() && FChar::IsWhitespace(Json[Start])) { ++Start; }
				int End = Start;
				while (End < Json.Len() &&
					(FChar::IsDigit(Json[End]) ||
					 Json[End] == TEXT('.') || Json[End] == TEXT('-') ||
					 Json[End] == TEXT('+') || Json[End] == TEXT('e') || Json[End] == TEXT('E')))
				{
					++End;
				}
				CurMs = (End > Start) ? FCString::Atod(*Json.Mid(Start, End - Start)) : 0.0;
				Cursor = End;
			}
		}
	}

	/**
	 * @brief 回放 1 件を実行して净边沿・イベント数と期待値を照合する。
	 * @note  ASerial の実 wire を直接扱うため、実測バイト列を FAserialFrameParser で
	 *        フレームへ分解してから実 C++ デコーダへ投入する。各バイト块（設備 1 回分の
	 *        粘着批次）を 3 バイト子块に分割して投入し、拆包の両面を再現する。
	 */
	void ReplayOneDoc(FAutomationTestBase& Test, const FString& Json, int64 ExpectedNet, const FString& Label)
	{
		TArray<double> Times;
		TArray<TArray<uint8>> ByteBlocks;
		ScanDocsFrames(Json, Times, ByteBlocks);
		Test.AddInfo(FString::Printf(TEXT("[%s] hex 块数=%d"), *Label, ByteBlocks.Num()));
		Test.TestTrue(TEXT("回放にフレームが存在する"), ByteBlocks.Num() > 0);
		if (ByteBlocks.Num() == 0)
		{
			return;
		}

		FBicycleMotionDecoder Decoder;
		struct FReplayContext
		{
			FBicycleMotionDecoder* Decoder = nullptr;
			double TimeSec = 0.0;
			int32 StateFrames = 0;
			int32 InfoFrames = 0;
			int32 OtherFrames = 0;
		} Ctx;
		Ctx.Decoder = &Decoder;

		// 完成フレームの分類（有効情報は生存扱いにせず運動にも取込まない）
		FAserialFrameParser::FFrameHandler Handler = [](void* UserData, int Length, const uint8_t* Payload)
		{
			FReplayContext* C = static_cast<FReplayContext*>(UserData);
			if (Length == 1 && Payload[0] <= 3)
			{
				++C->StateFrames;
				C->Decoder->Feed(C->TimeSec, Payload[0]);
			}
			else if (Length == 4)
			{
				++C->InfoFrames;
			}
			else
			{
				++C->OtherFrames;
			}
		};

		FAserialFrameParser Parser(Handler, &Ctx);
		for (int i = 0; i < ByteBlocks.Num(); ++i)
		{
			Ctx.TimeSec = Times[i];
			const TArray<uint8>& Bytes = ByteBlocks[i];
			// 拆包再現: 3 バイト子块に分割して投入（ブロック自体は粘着批次）
			for (int Offset = 0; Offset < Bytes.Num(); Offset += 3)
			{
				const int Chunk = FMath::Min(3, Bytes.Num() - Offset);
				Parser.FeedBytes(Bytes.GetData() + Offset, Chunk);
			}
		}

		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(0.0);
		FBicycleCycleEvent Events[256];
		const int NumEvents = Decoder.DrainCycleEvents(Events, 256);

		Test.AddInfo(FString::Printf(TEXT("[%s] 状態帧=%d 情報帧=%d 他帧=%d 净边沿=%lld 圈 +=%lld -=%lld 完走イベント=%d"),
			*Label, Ctx.StateFrames, Ctx.InfoFrames, Ctx.OtherFrames,
			Snap.NetEdges, Snap.ForwardCycles, Snap.ReverseCycles, NumEvents));

		// 期待値核对（docs 実測の净边沿）
		Test.TestEqual(FString::Printf(TEXT("[%s] 净边沿"), *Label), Snap.NetEdges, ExpectedNet);
		// 完走周期イベントは 1 周期につき必ず 1 件（イベントと圈数の一致）
		Test.TestEqual(FString::Printf(TEXT("[%s] イベント数=圈数合計"), *Label),
			static_cast<int64>(NumEvents), Snap.ForwardCycles + Snap.ReverseCycles);
	}
	// 2026.10.29 Lee(5) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
}

// ---------------------------------------------------------------------------
// 1. 標定 RPM の直接使用（判定・OnRPMCalculated 通知）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventCalibratedRpmDirect,
	"VRFishing.BicycleDrive.CalibratedRpmDirect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventCalibratedRpmDirect::RunTest(const FString& Parameters)
{
	UFishingReelStateComponent* Reel = MakeActiveReel();
	UTestRpmSink* Sink = ConnectSink(Reel);

	// 標定 RPM=50（許容帯内: 既定 10〜60）の前進周期を 3 件
	for (int32 i = 0; i < 3; ++i)
	{
		TestTrue(TEXT("未完了で継続"), Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0 + i));
	}

	// 通知値は標定値そのもの（角度積分による再計算値ではない）
	TestEqual(TEXT("通知回数"), Sink->BroadcastCount, 3);
	TestEqual(TEXT("通知値=標定RPM"), Sink->LastRpm, 50.0f);
	// 周期ごとに完走圈が進む
	TestEqual(TEXT("完走圈数"), Reel->GetCurrentRevolutionCount(), 3);
	return true;
}

// ---------------------------------------------------------------------------
// 2. 正反混合（逆方向は推進も停止基準更新もしない）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventMixedDirections,
	"VRFishing.BicycleDrive.MixedDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventMixedDirections::RunTest(const FString& Parameters)
{
	UFishingReelStateComponent* Reel = MakeActiveReel();
	UTestRpmSink* Sink = ConnectSink(Reel);

	// 正(50)→逆(30)→正(50)→逆(30)。逆方向のイベントは何も変えない
	TestTrue(TEXT("正1"), Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0));
	TestTrue(TEXT("逆1"), Reel->ProcessDeviceCycle(false, 30.0f, false, 101.0));
	TestTrue(TEXT("正2"), Reel->ProcessDeviceCycle(true, 50.0f, false, 102.0));
	TestTrue(TEXT("逆2"), Reel->ProcessDeviceCycle(false, 30.0f, false, 103.0));

	// 正方向のみ推進・通知される
	TestEqual(TEXT("完走圈数"), Reel->GetCurrentRevolutionCount(), 2);
	TestEqual(TEXT("通知回数"), Sink->BroadcastCount, 2);
	// 停止基準は最後の「正方向周期の実時刻」（逆イベントの時刻 103 で上書きされない）
	TestEqual(TEXT("停止基準=最後の正方向周期時刻"), Reel->GetLastRevolutionTime(), 102.0);
	return true;
}

// ---------------------------------------------------------------------------
// 3. 反転設定の方向判定（純関数）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventInvertFlagDirection,
	"VRFishing.BicycleDrive.InvertFlagDirection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventInvertFlagDirection::RunTest(const FString& Parameters)
{
	// 反転なし: 物理正方向がゲーム内前進
	TestTrue(TEXT("非反転+1"), UBicycleBridgeComponent::IsGameForwardEvent(false, +1));
	TestFalse(TEXT("非反転-1"), UBicycleBridgeComponent::IsGameForwardEvent(false, -1));
	// 反転あり: 物理逆方向がゲーム内前進
	TestFalse(TEXT("反転+1"), UBicycleBridgeComponent::IsGameForwardEvent(true, +1));
	TestTrue(TEXT("反転-1"), UBicycleBridgeComponent::IsGameForwardEvent(true, -1));
	// 未確定方向（0）は前進扱いにしない
	TestFalse(TEXT("方向0"), UBicycleBridgeComponent::IsGameForwardEvent(false, 0));
	return true;
}

// ---------------------------------------------------------------------------
// 4. 長フレーム多圈（判定漏れ無し・完了時に即打ち切り）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventLongFrameMultiCycle,
	"VRFishing.BicycleDrive.LongFrameMultiCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventLongFrameMultiCycle::RunTest(const FString& Parameters)
{
	// --- 4a. 許容帯内 5 周期を一括処理（低フレームレート相当）でも圈と判定が欠損しない ---
	{
		UFishingReelStateComponent* Reel = MakeActiveReel();
		UTestRpmSink* Sink = ConnectSink(Reel);

		bool bKeepGoing = true;
		for (int32 i = 0; i < 5 && bKeepGoing; ++i)
		{
			bKeepGoing = Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0 + i);
		}
		TestTrue(TEXT("許容帯内は失敗しない"), bKeepGoing);
		TestEqual(TEXT("完走圈数"), Reel->GetCurrentRevolutionCount(), 5);
		TestEqual(TEXT("通知回数（周期ごと 1 回）"), Sink->BroadcastCount, 5);
	}

	// --- 4b. 超速: 1 周期 4 判定 × 連続許容 3 周期分（既定 12 判定）で失敗し、残イベントは処理しない ---
	{
		UFishingReelStateComponent* Reel = MakeActiveReel();
		UTestRpmSink* Sink = ConnectSink(Reel);

		// 2026.10.29 Lee(3) startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// JudgeRPM のミスログ（ShowErrorLog → LogTemp Error）は本テストの期待動作:
		// 3 周期 × 1 周期 4 判定 = 12 通が必ず出る。自動化フレームワークへ事前登録し、
		// 「出過ぎ/足りない/文言違い」は従来どおり検知する（警告の抑制はしない）。
		// 件数は周期ごとの連続カウンタ（1〜12 回連続）で文言が変わるため、
		// 周期種別の固定部分（速すぎミス）を Contains で照合する。
		// 2026.10.29 Lee(4): AddExpectedMessage は既定で正規評価され [FishingReel] の
		// [] が文字クラスとして解釈され全件不一致になったため、正規評価を行わない
		// AddExpectedMessagePlain（素の部分文字列照合）へ変更する。
		AddExpectedMessagePlain(
			TEXT("[FishingReel] 速すぎミス"),
			ELogVerbosity::Error,
			EAutomationExpectedMessageFlags::Contains,
			12);
		// 2026.10.29 Lee(3) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

		// RPM=80 は既定の StickMaxAllowedRPM(60) 超過。5 周期分のイベントを 1 回で投入する
		bool bKeepGoing = true;
		int32 Processed = 0;
		for (int32 i = 0; i < 5 && bKeepGoing; ++i)
		{
			bKeepGoing = Reel->ProcessDeviceCycle(true, 80.0f, false, 100.0 + i);
			++Processed;
		}
		TestFalse(TEXT("3 周期分の連続超速で失敗"), bKeepGoing);
		TestEqual(TEXT("処理打ち切りの周期数"), Processed, 3);
		// 推進→判定の順のため、失敗した周期自体は圈数へ反映済み
		TestEqual(TEXT("完走圈数（失敗周期まで）"), Reel->GetCurrentRevolutionCount(), 3);
		TestEqual(TEXT("通知回数"), Sink->BroadcastCount, 3);

		// 完了後のイベントは無視される（状態も変わらない）
		TestFalse(TEXT("完了後は継続不可"), Reel->ProcessDeviceCycle(true, 80.0f, false, 200.0));
		TestEqual(TEXT("完了後の圈数不変"), Reel->GetCurrentRevolutionCount(), 3);
	}
	return true;
}

// ---------------------------------------------------------------------------
// 5. イベント実時刻と停止基準（消費遅延で現在時刻を使わない）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventTimeMapping,
	"VRFishing.BicycleDrive.EventTimeMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventTimeMapping::RunTest(const FString& Parameters)
{
	UFishingReelStateComponent* Reel = MakeActiveReel();

	// 停止基準はイベントの完了時刻（写像後）そのもので更新される
	TestTrue(TEXT("周期1"), Reel->ProcessDeviceCycle(true, 50.0f, false, 1000.0));
	TestEqual(TEXT("停止基準=イベント時刻1"), Reel->GetLastRevolutionTime(), 1000.0);

	// 次の周期（4 秒後完了想定）で基準が進む
	TestTrue(TEXT("周期2"), Reel->ProcessDeviceCycle(true, 50.0f, false, 1004.0));
	TestEqual(TEXT("停止基準=イベント時刻2"), Reel->GetLastRevolutionTime(), 1004.0);

	// 逆方向のイベント（時刻 1006）は基準を更新しない
	TestTrue(TEXT("逆方向"), Reel->ProcessDeviceCycle(false, 50.0f, false, 1006.0));
	TestEqual(TEXT("逆方向で基準不変"), Reel->GetLastRevolutionTime(), 1004.0);

	// 測定中の初回周期（時刻 1008）も基準を更新する（推進は行う）
	TestTrue(TEXT("測定中周期"), Reel->ProcessDeviceCycle(true, 0.0f, true, 1008.0));
	TestEqual(TEXT("測定中でも基準更新"), Reel->GetLastRevolutionTime(), 1008.0);
	return true;
}

// ---------------------------------------------------------------------------
// 6. 初圈と測定中（推進のみで判定・通知しない）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventMeasuringFirstCycle,
	"VRFishing.BicycleDrive.MeasuringFirstCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventMeasuringFirstCycle::RunTest(const FString& Parameters)
{
	UFishingReelStateComponent* Reel = MakeActiveReel();
	UTestRpmSink* Sink = ConnectSink(Reel);

	// 初回周期（測定中）: 推進と停止基準更新のみ。RPM 未確定のため通知・判定しない
	TestTrue(TEXT("初回周期は継続"), Reel->ProcessDeviceCycle(true, 0.0f, true, 100.0));
	TestEqual(TEXT("初圈で圈数 1"), Reel->GetCurrentRevolutionCount(), 1);
	TestEqual(TEXT("初圈で基準更新"), Reel->GetLastRevolutionTime(), 100.0);
	TestEqual(TEXT("初圈は通知しない"), Sink->BroadcastCount, 0);

	// 測定中フラグ付きなら値が来ても無視する（虚構の判定をしない）
	TestTrue(TEXT("測定中 2 周期"), Reel->ProcessDeviceCycle(true, 999.0f, true, 101.0));
	TestEqual(TEXT("測定中は通知しない"), Sink->BroadcastCount, 0);
	TestEqual(TEXT("測定中でも推進する"), Reel->GetCurrentRevolutionCount(), 2);
	TestTrue(TEXT("測定中でも失敗しない（999RPM を無視）"), Reel->GetCurrentRevolutionCount() == 2);
	return true;
}

// ---------------------------------------------------------------------------
// 7. 履歴再構築判定（フェーズ入替 / 再接続 / 方向設定変更）（純関数）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventRebuildBaseline,
	"VRFishing.BicycleDrive.RebuildBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventRebuildBaseline::RunTest(const FString& Parameters)
{
	// リール非アクティブ → 常に再構築
	TestTrue(TEXT("非アクティブ"),
		UBicycleBridgeComponent::ShouldRebuildBaseline(false, true, 1, 1, false, false));

	// 再接続（接続代次の変化）→ 再構築
	TestTrue(TEXT("代次変化"),
		UBicycleBridgeComponent::ShouldRebuildBaseline(true, true, 2, 1, false, false));

	// 方向反転設定の変更 → 再構築
	TestTrue(TEXT("方向設定変更"),
		UBicycleBridgeComponent::ShouldRebuildBaseline(true, true, 1, 1, true, false));

	// フェーズ入替（IsActive の立ち上がり）→ 再構築
	TestTrue(TEXT("立ち上がり"),
		UBicycleBridgeComponent::ShouldRebuildBaseline(true, false, 1, 1, false, false));

	// 正常継続中 → 再構築しない
	TestFalse(TEXT("正常継続"),
		UBicycleBridgeComponent::ShouldRebuildBaseline(true, true, 1, 1, false, false));

	// 2026.10.29 Lee(7) startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 再構築時の批次消費方針:
	// 再接続（代次変化）+ 活性 → 新代次イベントを消費（握手直後の首圈を守る）。
	// 2026.10.29 Lee(8) 改訂: 消費を許すのは「純粋な再接続」（活性継続・方向不変）のみ
	TestTrue(TEXT("純粋な再接続は新代次批次を消費"),
		UBicycleBridgeComponent::ShouldConsumeEventsOnRebuild(true, true, true, false));
	// 階段の立ち上がりが同帧で絡む → 履歴と新データの境界を証明できないため全破棄
	TestFalse(TEXT("階段立ち上がりとの同帧混合は全破棄"),
		UBicycleBridgeComponent::ShouldConsumeEventsOnRebuild(true, false, true, false));
	// 方向設定変更が同帧で絡む → 旧方向語义の圈を新方向へ補算する危険があるため全破棄
	TestFalse(TEXT("方向変更との同帧混合は全破棄"),
		UBicycleBridgeComponent::ShouldConsumeEventsOnRebuild(true, true, true, true));
	// フェーズ入替（代次不変）→ 非アクティブ期間の積压は履歴として全破棄
	TestFalse(TEXT("フェーズ入替は全破棄"),
		UBicycleBridgeComponent::ShouldConsumeEventsOnRebuild(true, true, false, false));
	// 非アクティブ → 全破棄
	TestFalse(TEXT("非アクティブは全破棄"),
		UBicycleBridgeComponent::ShouldConsumeEventsOnRebuild(false, true, true, false));
	// 2026.10.29 Lee(7) endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	return true;
}

// ---------------------------------------------------------------------------
// 8. 受信フレーム分類（有効情報のみ生存根拠。ID/Ver 不一致・運動系の他形態は非該当）
// ---------------------------------------------------------------------------
// ※ ProcessBicycleFrame は本純関数の分類で分岐し、分類 0（有効情報）のときだけ
//   LastInfoResponseSec（心跳生存基準）と InfoFrameCount を更新する。
//   分類 2 へ落ちた 4 バイト（ID/Ver 不一致など）は生存判定へ到達しない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventFrameClassification,
	"VRFishing.BicycleDrive.FrameClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventFrameClassification::RunTest(const FString& Parameters)
{
	constexpr uint8 BikeId = 0x03;
	constexpr uint8 BikeVer = 0x02;

	// 有効情報フレーム: 長さ 4 + ID/Ver 一致 → 生存根拠（分類 0）
	TestEqual(TEXT("有効情報"),
		FFishingSerialWorker::ClassifyBicycleFrame(4, BikeId, BikeVer, BikeId, BikeVer), 0);

	// ID 不一致 / Ver 不一致 / 両方不一致の 4 バイト → 他フレーム（生存根拠にしない）
	TestEqual(TEXT("ID不一致"),
		FFishingSerialWorker::ClassifyBicycleFrame(4, 0x02, BikeVer, BikeId, BikeVer), 2);
	TestEqual(TEXT("Ver不一致"),
		FFishingSerialWorker::ClassifyBicycleFrame(4, BikeId, 0x01, BikeId, BikeVer), 2);
	TestEqual(TEXT("両方不一致"),
		FFishingSerialWorker::ClassifyBicycleFrame(4, 0x01, 0x01, BikeId, BikeVer), 2);

	// 状態フレーム: 長さ 1 + 0..3 → 運動取込み（分類 1。生存判定には使われない）
	TestEqual(TEXT("状態0"),
		FFishingSerialWorker::ClassifyBicycleFrame(1, 0, 0, BikeId, BikeVer), 1);
	TestEqual(TEXT("状態3"),
		FFishingSerialWorker::ClassifyBicycleFrame(1, 3, 0, BikeId, BikeVer), 1);

	// 值域外の 1 バイト → 他フレーム
	TestEqual(TEXT("状態4"),
		FFishingSerialWorker::ClassifyBicycleFrame(1, 4, 0, BikeId, BikeVer), 2);

	// その他の長さ → すべて他フレーム
	TestEqual(TEXT("長さ0"),
		FFishingSerialWorker::ClassifyBicycleFrame(0, 0, 0, BikeId, BikeVer), 2);
	TestEqual(TEXT("長さ2"),
		FFishingSerialWorker::ClassifyBicycleFrame(2, BikeId, BikeVer, BikeId, BikeVer), 2);
	TestEqual(TEXT("長さ3"),
		FFishingSerialWorker::ClassifyBicycleFrame(3, BikeId, BikeVer, BikeId, BikeVer), 2);
	TestEqual(TEXT("長さ5"),
		FFishingSerialWorker::ClassifyBicycleFrame(5, BikeId, BikeVer, BikeId, BikeVer), 2);
	return true;
}

// ---------------------------------------------------------------------------
// 9. デコーダ挙動マトリクス（正反 / 抖動 / 首状態 / 非法跳変 / 反向清窓 /
//    停転再始動 / 同一時刻 / 非有限入力 / 每圈計数 / イベント-スナップショット一致）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventDecoderMatrix,
	"VRFishing.BicycleDrive.DecoderMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventDecoderMatrix::RunTest(const FString& Parameters)
{
	// --- 9a. 正方向 2 周期: 初回は測定中、第 2 周期は間隔 2 秒 → RPM=30。イベントと快照が一致 ---
	{
		FBicycleMotionDecoder Decoder;
		const int States[] = { 0, 1, 3, 2, 0, 1, 3, 2, 0 };
		FeedSeq(Decoder, 0.0, 0.5, States, 9);

		FBicycleCycleEvent Events[16];
		const int NumEvents = Decoder.DrainCycleEvents(Events, 16);
		TestEqual(TEXT("正方向: 完走イベント数"), NumEvents, 2);
		if (TestTrue(TEXT("正方向: イベント 2 件以上"), NumEvents >= 2))
		{
			// 生イベントの有効性は Core と同一口径（厳密に正かつ有限）で判定する
			TestTrue(TEXT("正方向: 初回周期は測定中"), !IsValidAvgInterval(Events[0].AvgIntervalSec));
			TestFalse(TEXT("正方向: 第 2 周期は速度確定"), !IsValidAvgInterval(Events[1].AvgIntervalSec));
			TestEqual(TEXT("正方向: 第 2 周期の平均間隔"), Events[1].AvgIntervalSec, 2.0);
			// GetRpm（無効値の二重防御込み）は Core 転写経由で実測する
			TestEqual(TEXT("正方向: 第 2 周期の実効 RPM"), CoreRpmFrom(Events[1].AvgIntervalSec), 30.0);
			TestEqual(TEXT("正方向: 方向 +1"), Events[1].Dir, 1);
		}

		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(4.2);
		TestEqual(TEXT("正方向: 快照净边沿 +8"), Snap.NetEdges, int64(8));
		TestEqual(TEXT("正方向: 快照圈数 +2"), Snap.ForwardCycles, int64(2));
		// イベントとスナップショットの有効平均間隔は常に一致する（同一の有効口径）
		TestEqual(TEXT("正方向: イベントと快照の平均間隔一致"), Snap.AvgIntervalSec, Events[1].AvgIntervalSec);
	}

	// --- 9b. 逆方向 2 周期: 0→2→3→1→0、净边沿 -8 ---
	{
		FBicycleMotionDecoder Decoder;
		const int States[] = { 0, 2, 3, 1, 0, 2, 3, 1, 0 };
		FeedSeq(Decoder, 0.0, 0.5, States, 9);

		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(4.2);
		TestEqual(TEXT("逆方向: 快照净边沿 -8"), Snap.NetEdges, int64(-8));
		TestEqual(TEXT("逆方向: 逆圈 2"), Snap.ReverseCycles, int64(2));
		TestEqual(TEXT("逆方向: 正圈 0"), Snap.ForwardCycles, int64(0));
	}

	// --- 9c. 抖動のみ（0→1→0 連発）: 完走周期なし・净边沿 0 ---
	{
		FBicycleMotionDecoder Decoder;
		const int States[] = { 0, 1, 0, 1, 0, 1, 0 };
		FeedSeq(Decoder, 0.0, 0.3, States, 7);

		FBicycleCycleEvent Events[16];
		TestEqual(TEXT("抖動: 完走イベント 0"), Decoder.DrainCycleEvents(Events, 16), 0);
		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(10.0);
		TestEqual(TEXT("抖動: 净边沿 0"), Snap.NetEdges, int64(0));
		TestEqual(TEXT("抖動: 圈数 0"), Snap.ForwardCycles + Snap.ReverseCycles, int64(0));
	}

	// --- 9d. 首状態は基準のみ（辺を捏造しない） ---
	{
		FBicycleMotionDecoder Decoder;
		Decoder.Feed(0.0, 2);
		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(1.0);
		TestEqual(TEXT("首状態: 净边沿 0"), Snap.NetEdges, int64(0));
		TestTrue(TEXT("首状態: 基準=2"), Snap.bHasBaseline && Snap.Baseline == 2);
	}

	// --- 9e. 非法跳変（0→3 は対角）: 計数せず基準のみ再同期、その後 3→2 は正方向 +1 ---
	{
		FBicycleMotionDecoder Decoder;
		Decoder.Feed(0.0, 0);
		Decoder.Feed(0.5, 3);
		TestEqual(TEXT("非法跳変: 計数しない"), Decoder.MakeSnapshot(2.0).NetEdges, int64(0));
		const FBicycleMotionDecoderSnapshot Snap1 = Decoder.MakeSnapshot(2.0);
		TestEqual(TEXT("非法跳変: 検出 1 回"), Snap1.InvalidTransitions, int64(1));
		Decoder.Feed(1.0, 2);
		TestEqual(TEXT("再同期後 3→2 は +1"), Decoder.MakeSnapshot(3.0).NetEdges, int64(1));
	}

	// --- 9f. 反向清窓: 正方向の速度窓が方向反転でクリアされ、逆の初回周期は測定中 ---
	{
		FBicycleMotionDecoder Decoder;
		const int Fwd[] = { 0, 1, 3, 2, 0, 1, 3, 2, 0 };
		FeedSeq(Decoder, 0.0, 0.5, Fwd, 9); // 速度窓 = [2.0]
		const int Rev[] = { 0, 2, 3, 1, 0 };
		FeedSeq(Decoder, 2.5, 0.5, Rev, 5); // 逆 1 周期（方向反転で窓クリア）

		FBicycleCycleEvent Events[16];
		const int NumEvents = Decoder.DrainCycleEvents(Events, 16);
		TestEqual(TEXT("反向清窓: 完走イベント 3"), NumEvents, 3);
		if (TestTrue(TEXT("反向清窓: イベント 3 件以上"), NumEvents >= 3))
		{
			TestTrue(TEXT("反向清窓: 逆の初回周期は測定中（窓クリア済み）"),
				!IsValidAvgInterval(Events[2].AvgIntervalSec));
			TestEqual(TEXT("反向清窓: 逆の初回周期 RPM は 0"),
				CoreRpmFrom(Events[2].AvgIntervalSec), 0.0);
		}
	}

	// --- 9g. 停転再始動: 停転間隔を速度窓へ混入させず、累計は保持する ---
	{
		FBicycleMotionDecoder Decoder;
		const int Fwd[] = { 0, 1, 3, 2, 0, 1, 3, 2, 0 };
		FeedSeq(Decoder, 0.0, 0.5, Fwd, 9); // 周期 t=2, 4（窓=[2.0]）
		const int Seq[] = { 1, 3, 2, 0 };
		FeedSeq(Decoder, 30.5, 0.5, Seq, 4); // 停転 28 秒後の第 1 新周期 t=32

		FBicycleCycleEvent Events[16];
		const int NumEvents = Decoder.DrainCycleEvents(Events, 16);
		TestEqual(TEXT("停転再始動: 完走イベント 3"), NumEvents, 3);
		if (TestTrue(TEXT("停転再始動: イベント 3 件以上"), NumEvents >= 3))
		{
			// gap=28 は窓へ入らず旧窓ごと破棄 → 第 1 新周期は測定中
			TestTrue(TEXT("停転再始動: 新周期は測定中"),
				!IsValidAvgInterval(Events[2].AvgIntervalSec));
		}
		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(32.0);
		TestEqual(TEXT("停転再始動: 累計保持 net=+12"), Snap.NetEdges, int64(12));
		TestEqual(TEXT("停転再始動: 累計保持 圈+3"), Snap.ForwardCycles, int64(3));
	}

	// --- 9h. 同一時刻の複数周期: 平均間隔 0 は無効 → 測定中扱い。RPM は 0（無限大/NaN を出さない） ---
	{
		FBicycleMotionDecoder Decoder;
		const int States[] = { 0, 1, 3, 2, 0, 1, 3, 2, 0 };
		FeedSeq(Decoder, 5.0, 0.0, States, 9); // 全辺沿が同一時刻 t=5.0

		FBicycleCycleEvent Events[16];
		const int NumEvents = Decoder.DrainCycleEvents(Events, 16);
		TestEqual(TEXT("同一時刻: 完走イベント 2"), NumEvents, 2);
		for (int i = 0; i < NumEvents; ++i)
		{
			TestTrue(FString::Printf(TEXT("同一時刻: イベント %d は測定中扱い"), i),
				!IsValidAvgInterval(Events[i].AvgIntervalSec));
			// GetRpm（無効値の二重防御込み）は Core 転写経由で実測する
			TestEqual(FString::Printf(TEXT("同一時刻: イベント %d の RPM は 0"), i),
				CoreRpmFrom(Events[i].AvgIntervalSec), 0.0);
			TestTrue(FString::Printf(TEXT("同一時刻: イベント %d の平均間隔は有限"), i),
				FMath::IsFinite(Events[i].AvgIntervalSec));
		}

		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(6.0);
		TestEqual(TEXT("同一時刻: 快照 RPM は 0"), Snap.Rpm, 0.0);
		TestTrue(TEXT("同一時刻: 快照は測定中または停止"),
			Snap.Phase == EBicycleMotionPhase::Measuring || Snap.Phase == EBicycleMotionPhase::Stopped);
	}

	// --- 9i. 非有限入力: Feed(NaN) は無視され、以降の復号も汚さない ---
	{
		FBicycleMotionDecoder Decoder;
		Decoder.Feed(0.0, 0);
		Decoder.Feed(std::numeric_limits<double>::quiet_NaN(), 1);
		const FBicycleMotionDecoderSnapshot Snap1 = Decoder.MakeSnapshot(1.0);
		TestEqual(TEXT("非有限: 基準変化なし"), Snap1.Baseline, 0);
		Decoder.Feed(1.0, 1); // NaN を挟んでも通常の辺として処理される
		const FBicycleMotionDecoderSnapshot Snap2 = Decoder.MakeSnapshot(2.0);
		TestEqual(TEXT("非有限: 無視後も辺を計上"), Snap2.NetEdges, int64(1));
		TestTrue(TEXT("非有限: 快照 RPM は有限"), FMath::IsFinite(Snap2.Rpm));
	}

	// --- 9j. 每圈計数: 正方向 10 圈 = 40 边沿 ---
	{
		FBicycleMotionDecoder Decoder;
		// 2026.10.29 Lee(6): 初回状態は基準確定にのみ使われる（辺を捏造しない）ため、
		// ループ前に基準 0 を投入しないと 40 辺のうち最初の 1 個が基準に吸収され 39 辺 9 圈になる
		Decoder.Feed(9.5, 0);
		const int Cycle[] = { 1, 3, 2, 0 };
		for (int CycleIndex = 0; CycleIndex < 10; ++CycleIndex)
		{
			FeedSeq(Decoder, 10.0 + CycleIndex * 2.0, 0.5, Cycle, 4);
		}
		const FBicycleMotionDecoderSnapshot Snap = Decoder.MakeSnapshot(40.0);
		TestEqual(TEXT("10 圈: 净边沿 +40"), Snap.NetEdges, int64(40));
		TestEqual(TEXT("10 圈: 正圈 10"), Snap.ForwardCycles, int64(10));
		TestEqual(TEXT("10 圈: 逆圈 0"), Snap.ReverseCycles, int64(0));

		FBicycleCycleEvent Events[16];
		int NumEvents = Decoder.DrainCycleEvents(Events, 16);
		int TotalEvents = NumEvents;
		while (NumEvents == 16)
		{
			NumEvents = Decoder.DrainCycleEvents(Events, 16);
			TotalEvents += NumEvents;
		}
		TestEqual(TEXT("10 圈: 完走イベント 10"), TotalEvents, 10);
	}
	return true;
}

// ---------------------------------------------------------------------------
// 10. docs 実測 JSON の回放（07/09/05。拆包・粘着を含む実 wire 逐フレーム投入）
// ---------------------------------------------------------------------------
// ※ 対象ファイルは docs のローカル運用資産（.gitignore 対象）のため、
//   存在しない環境では警告を出してスキップする（失敗にはしない）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventDocsReplay,
	"VRFishing.BicycleDrive.DocsReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventDocsReplay::RunTest(const FString& Parameters)
{
	// 実配置は docs/自行车设备排查与RPM标定资料_2026-10-28/文档 配下（旧 docs ルート想定は禁止）
	const FString DocDir = FPaths::ProjectDir() /
		TEXT("docs/自行车设备排查与RPM标定资料_2026-10-28/文档");

	struct FReplayCase
	{
		const TCHAR* FileName;
		int64 ExpectedNet;
	};
	const FReplayCase Cases[] = {
		{ TEXT("bicycle-raw-ten-turns-thirty-seconds-2026-10-28-07.json"), 40 },
		{ TEXT("bicycle-raw-ten-turns-thirty-seconds-2026-10-28-09.json"), 39 },
		{ TEXT("bicycle-raw-reverse-ten-seconds-2026-10-28-05.json"), -33 },
	};

	for (const FReplayCase& Case : Cases)
	{
		const FString Path = FPaths::Combine(DocDir, Case.FileName);
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			// docs はローカル運用ファイル。無い環境ではスキップ（警告をログへ残す）
			AddWarning(FString::Printf(TEXT("回放ファイルが存在しないためスキップ: %s"), *Path));
			continue;
		}
		ReplayOneDoc(*this, Json, Case.ExpectedNet, Case.FileName);
	}
	return true;
}

// ---------------------------------------------------------------------------
// 11. 停止検知の先典（積压イベントで失敗を复活させない）と境界
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventStopTimeoutFirstCheck,
	"VRFishing.BicycleDrive.StopTimeoutFirstCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventStopTimeoutFirstCheck::RunTest(const FString& Parameters)
{
	// --- 11a. 未超 5 秒（4.9 秒差）: 通常どおり推進し続行する ---
	{
		UFishingReelStateComponent* Reel = MakeActiveReel();
		TestTrue(TEXT("初圈（比較対象なしで受理）"), Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0));
		TestTrue(TEXT("4.9 秒差は継続"), Reel->ProcessDeviceCycle(true, 50.0f, false, 104.9));
		TestEqual(TEXT("未超: 圈数 2"), Reel->GetCurrentRevolutionCount(), 2);
	}

	// --- 11a2. 恰好 5.0 秒差: UpdateState と同一の「>=」で失敗に揃える（Lee(8)） ---
	{
		UFishingReelStateComponent* Reel = MakeActiveReel();
		TestTrue(TEXT("初圈を受理"), Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0));
		TestFalse(TEXT("5.0 秒差は既存規則どおり失敗（>=）"), Reel->ProcessDeviceCycle(true, 50.0f, false, 105.0));
		TestEqual(TEXT("恰好 5 秒: 圈数は初圈のまま"), Reel->GetCurrentRevolutionCount(), 1);
		TestEqual(TEXT("恰好 5 秒: 基準は复活しない"), Reel->GetLastRevolutionTime(), 100.0);
	}

	// --- 11b. 超 5 秒（6 秒差）: 基準更新の前に失敗が確定し、基準は复活しない ---
	{
		UFishingReelStateComponent* Reel = MakeActiveReel();
		TestTrue(TEXT("初圈を受理"), Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0));
		TestEqual(TEXT("初圈で基準=100"), Reel->GetLastRevolutionTime(), 100.0);

		// 前回完走圈から 6 秒差のイベント: 停止=失敗を先に確定させる
		TestFalse(TEXT("6 秒差は停止失敗"), Reel->ProcessDeviceCycle(true, 50.0f, false, 106.0));
		// 失敗確定のため基準（LastRevolutionTime）は初圈時刻のまま（106 へ進んで复活しない）
		TestEqual(TEXT("失敗時に基準を進めない"), Reel->GetLastRevolutionTime(), 100.0);
		TestEqual(TEXT("失敗周期は圈数へ反映しない"), Reel->GetCurrentRevolutionCount(), 1);

		// 完了後の入力は無視される
		TestFalse(TEXT("完了後は継続不可"), Reel->ProcessDeviceCycle(true, 50.0f, false, 107.0));
		TestEqual(TEXT("完了後の圈数不変"), Reel->GetCurrentRevolutionCount(), 1);
	}

	// --- 11c. 多事件同帧: 100→101→102→108。境界内 3 圈の後、6 秒差の 4 件目で失敗 ---
	{
		UFishingReelStateComponent* Reel = MakeActiveReel();
		TestTrue(TEXT("事件1"), Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0));
		TestTrue(TEXT("事件2"), Reel->ProcessDeviceCycle(true, 50.0f, false, 101.0));
		TestTrue(TEXT("事件3"), Reel->ProcessDeviceCycle(true, 50.0f, false, 102.0));
		TestFalse(TEXT("事件4（6 秒差）で失敗"), Reel->ProcessDeviceCycle(true, 50.0f, false, 108.0));
		TestEqual(TEXT("多事件: 圈数は失敗前まで"), Reel->GetCurrentRevolutionCount(), 3);
		TestEqual(TEXT("多事件: 基準は最後の有効圈"), Reel->GetLastRevolutionTime(), 102.0);
	}

	// --- 11d. 逆方向は停止判定にも基準更新にも関与しない ---
	{
		UFishingReelStateComponent* Reel = MakeActiveReel();
		TestTrue(TEXT("初圈"), Reel->ProcessDeviceCycle(true, 50.0f, false, 100.0));
		// 100 秒差の逆方向イベントでも失敗しない（逆方向は判定・更新の両方から除外）
		TestTrue(TEXT("逆方向は失敗させない"), Reel->ProcessDeviceCycle(false, 50.0f, false, 200.0));
		TestEqual(TEXT("逆方向で基準不変"), Reel->GetLastRevolutionTime(), 100.0);
		TestEqual(TEXT("逆方向で圈数不変"), Reel->GetCurrentRevolutionCount(), 1);
	}
	return true;
}

// ---------------------------------------------------------------------------
// 12. 通信異常の再接続判定と drain 継続判定（2026.10.29 追加の熱抜去対応純関数）
// ---------------------------------------------------------------------------
// ※ ワーカーが拔去（ハード IO エラー）と解析ノイズ（ソフトエラー）で閾値を分ける
//   方針、および受信 drain ループの「予算/停止要求での打ち切り」条件を固定する。
//   ShouldContinueDrain の条件緩和（予算無し・停止無視）は抜去時の PIE 終了
//   固死の再発に直結するため、本テストで規約として固定する
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventCommErrorPolicy,
	"VRFishing.BicycleDrive.CommErrorPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventCommErrorPolicy::RunTest(const FString& Parameters)
{
	// --- ハード IO エラー（available/read/write 失敗 = 抜去の直接的証拠）: 2 連続で再接続 ---
	TestFalse(TEXT("ハード1回目は一過性として許容"),
		FFishingSerialWorker::ShouldReconnectOnCommError(true, 1));
	TestTrue(TEXT("ハード2連続で再接続"),
		FFishingSerialWorker::ShouldReconnectOnCommError(true, 2));
	TestTrue(TEXT("ハード3連続以上も再接続"),
		FFishingSerialWorker::ShouldReconnectOnCommError(true, 3));

	// --- ソフト解析エラー（校験不一致などの 1 byte ノイズ）: 従来どおり 20 連続で再接続 ---
	TestFalse(TEXT("ソフト19連続は継続"),
		FFishingSerialWorker::ShouldReconnectOnCommError(false, 19));
	TestTrue(TEXT("ソフト20連続で再接続"),
		FFishingSerialWorker::ShouldReconnectOnCommError(false, 20));

	// --- drain 継続判定: バイト有り・予算未消化・停止要求なし の 3 条件が揃った時だけ継続 ---
	TestTrue(TEXT("滞留有り・予算内・停止なしは継続"),
		FFishingSerialWorker::ShouldContinueDrain(5, 0, false));
	TestFalse(TEXT("滞留ゼロは打ち切り"),
		FFishingSerialWorker::ShouldContinueDrain(0, 0, false));
	TestFalse(TEXT("予算消化済みは打ち切り（残りは次回ポーリングへ持ち越し）"),
		FFishingSerialWorker::ShouldContinueDrain(5, FFishingSerialWorker::MaxDrainBytesPerPoll, false));
	TestFalse(TEXT("停止要求済みは打ち切り（終了ブロック防止）"),
		FFishingSerialWorker::ShouldContinueDrain(5, 0, true));
	// 予算境界の直前（最大値-1）はまだ継続する
	TestTrue(TEXT("予算境界直前は継続"),
		FFishingSerialWorker::ShouldContinueDrain(5, FFishingSerialWorker::MaxDrainBytesPerPoll - 1, false));
	// 負値（available() 失敗）は継続しない（ハードエラー経路へ分岐させる前提）
	TestFalse(TEXT("滞留負値（ポート異常）は打ち切り"),
		FFishingSerialWorker::ShouldContinueDrain(-1, 0, false));
	return true;
}

// ---------------------------------------------------------------------------
// 13. シリアル 1 バイト読取の戻り値規約（2026.10.29 独立審査指摘の回帰固定）
// ---------------------------------------------------------------------------
// ※ WindowsSerial::read の旧実装（int8_t 受信）では 0x80..0xFF の正規バイトが
//    static_cast<int> の符号拡張で負値になり、0xFF が失敗値 -1 と同値だった
//    （フレームヘッダ D0・エスケープ AD まで失敗扱いされる回帰の発端）。
//    ByteToReturnValue（純関数・read() 本体が通る変換点）で
//    「成功値 = 0..255 / 失敗 = -1」の規約を固定し、00/7F/80/AD/D0/FF を実測する。
//    実機 ReadFile 経路の失敗分岐（ReadFile 失敗/部分読取 → -1）は自動化対象外で
//    実機验收で確認する
#if PLATFORM_WINDOWS
#include "WindowsSerial/WindowsSerial.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCycleEventSerialByteContract,
	"VRFishing.BicycleDrive.SerialByteContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCycleEventSerialByteContract::RunTest(const FString& Parameters)
{
	// --- 成功値: 6 バイトを実測（事故っていた 0x80 以上の領域と両境界を含む） ---
	const uint8_t Bytes[] = { 0x00, 0x7F, 0x80, 0xAD, 0xD0, 0xFF };
	for (const uint8_t Byte : Bytes)
	{
		const int ReturnValue = WindowsSerial::ByteToReturnValue(Byte);
		TestEqual(FString::Printf(TEXT("byte 0x%02X の戻り値は入力値そのもの"), Byte),
			ReturnValue, static_cast<int>(Byte));
		TestTrue(FString::Printf(TEXT("byte 0x%02X は非負（成功値）"), Byte),
			ReturnValue >= 0);
	}

	// --- 失敗値 -1 との混同防止の明示（D0=フレームヘッダ / AD=エスケープ / FF=正規データ） ---
	TestFalse(TEXT("0xD0 は失敗値ではない"), WindowsSerial::ByteToReturnValue(0xD0) == -1);
	TestFalse(TEXT("0xAD は失敗値ではない"), WindowsSerial::ByteToReturnValue(0xAD) == -1);
	TestFalse(TEXT("0xFF は失敗値ではない"), WindowsSerial::ByteToReturnValue(0xFF) == -1);
	return true;
}
#endif // PLATFORM_WINDOWS
