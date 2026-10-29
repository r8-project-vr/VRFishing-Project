// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/device/FishingAserialFrameParser.h"

// ============================================================================
// ASerial フレームパーサの実装（RpmMonitor.cs の AserialFrameParser と同一規則）。
// Doxygen 記法の関数説明はヘッダ側に記載済みのため、ここでは手順コメントのみ。
// ============================================================================

FAserialFrameParser::FAserialFrameParser(FFrameHandler InHandler, void* InUserData)
	: Handler(InHandler)
	, UserData(InUserData)
{
}

void FAserialFrameParser::Reset()
{
	InFrame = false;
	AddFlag = false;
	ErrorPending = false;
	Step = 0;
	DataLen = 0;
	ReadCount = 0;
	CheckSum = 0;
	CkHi = 0;
}

void FAserialFrameParser::FeedBytes(const uint8_t* Buf, int Count)
{
	for (int i = 0; i < Count; ++i)
	{
		FeedByte(Buf[i]);
	}
}

void FAserialFrameParser::FeedByte(uint8_t b)
{
	// フレーム先頭フラグは常に最優先（フレーム読み取り中の裸 D0 = 再同期）
	if (b == StartFlag)
	{
		if (InFrame)
		{
			// 前フレームは途切れたため破棄（ASerial: WARNING_READ_SKIP 相当）
			++ResyncCount;
		}
		InFrame = true;
		AddFlag = false;
		ErrorPending = false;
		Step = 0;
		ReadCount = 0;
		CheckSum = 0;
		CkHi = 0;
		return;
	}

	if (ErrorPending)
	{
		// エラー後は次の D0 まで全破棄
		return;
	}

	if (b == AddFlagByte)
	{
		if (AddFlag)
		{
			// AD の連続は規格外（ERR_ADDFLAG_CASCADE 相当）
			++BadFrameCount;
			ErrorPending = true;
			InFrame = false;
			return;
		}
		AddFlag = true;
		return;
	}

	uint8_t v = b;
	if (AddFlag)
	{
		// エスケープ解除（次バイト +1）
		v = static_cast<uint8_t>(b + 1);
		AddFlag = false;
	}

	if (!InFrame)
	{
		// フレーム外の単独バイトはノイズとして無視
		return;
	}

	switch (Step)
	{
	case 0: // 長さバイト
		DataLen = v;
		if (DataLen > MaxDataNum)
		{
			// 長さ超過は明示エラー（次の D0 まで破棄）
			++BadFrameCount;
			ErrorPending = true;
			InFrame = false;
			return;
		}
		Step = (DataLen == 0) ? 2 : 1;
		break;

	case 1: // payload 蓄積
		PayloadBuf[ReadCount] = v;
		CheckSum = (CheckSum + v) & 0xFFFF;
		++ReadCount;
		if (ReadCount >= DataLen)
		{
			Step = 2;
		}
		break;

	case 2: // チェックサム Hi
		CkHi = v;
		Step = 3;
		break;

	case 3: // チェックサム Lo（ここで確定判定）
		{
			const int Expect = (CkHi << 8) | v;
			InFrame = false;
			if (Expect != CheckSum)
			{
				// チェックサム不一致: 破棄して計数（運動へは取込まない）
				++BadChecksumCount;
			}
			else if (Handler != nullptr)
			{
				Handler(UserData, DataLen, PayloadBuf);
			}
		}
		break;

	default:
		// 到達しない（Step は 0..3 のみ）
		Reset();
		break;
	}
}
