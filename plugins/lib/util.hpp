
#include <windows.h>

/// <summary>
/// 連番ファイル群の先頭ファイルをパースした結果
/// </summary>
struct SEQSEP {
	// 先頭ポインタ(参照のみ)
	TCHAR* pstart;
	// パース用のオフセット(長すぎる場合は補正後)
	int head;
	// パース用のオフセット
	int tail;
	// 後ろから探して最初のドット
	int dext;
	// 桁数
	int digit;
	// カウンタの開始数
	int begin;
};

typedef unsigned short _u16;

// 指数テーブル
float _gk2[32];

void _makeTable() {
	float k = 64.0f; // 2^6
	for (int i = 0; i < 32; ++i) {
		_gk2[31 - i] = k;
		k *= 0.5f;
	}
	_gk2[0] = _gk2[1];
}

/// <summary>
/// _makeTable 実行が必要
/// </summary>
/// <param name="u16"></param>
/// <returns></returns>
float _u16tof(unsigned short u16) {
	if (u16 == 0) {
		return 0.0f;
	}
	int signBit = (u16 & 0x8000) ? 0x80000000 : 0;
	int biased = (int)((u16 >> 10) & 0x1f);
	int bits = (int)(u16 & 0x3ff); // 10bit
	if (biased == 0) { // ケチ表現
		return (signBit ? -1.0f : 1.0f) * _gk2[0] * (float)bits;
	}

	if (biased == 31) { // 無限大またはNaN. 8bit exp
		unsigned int buf = 0x7f800000 | signBit | (bits << 13);
		float* p = (float*)&buf;
		return (*p);
	}

	float ret = _gk2[biased] * (float)(bits | 0x400);
	if (signBit) {
		ret = -ret;
	}
	return ret;
}

unsigned short _ftob16(float v) {
	int b32 = 0;
	CopyMemory(&b32, &v, 4);
	bool isSign = (b32 < 0);
	int biased = (b32 >> 23) & 0xff; // 0-255
	int exp2 = biased - 127;
	int bias16 = exp2 + 15;
	int frac = b32 & 0x7fffff; // 23bit

	_u16 ret = (isSign) ? 0x8000 : 0x0000;
	if (biased == 255) {
		ret |= (_u16)0x7c00;
		if (frac == 0) {
			return ret;
		}
		frac >>= 13;
		if (frac == 0) {
			frac = 1;
		}
		ret |= (_u16)frac; // NaN
		return ret;
	}

	// -15(halfケチ), -14, 0, +14, +15, +16(halfでは無限大かNaN)
	if (exp2 > 15) {
		ret |= (30 << 10) | 0x03ff; // half16の最大値
		return ret;
	}
	if (exp2 < -14) {
		frac |= 0x800000; // 23個より1つ上
		frac >>= -1 - exp2;
		if (frac == 0) {
			frac = 1; // 最小に切り上げ
		}
		ret |= frac;
		return ret;
	}

	frac >>= 13; // 10bitだけ残す
	ret |= (_u16)frac;
	ret |= (_u16)(bias16 << 10);
	return ret;
}

/// <summary>
/// 連番でなくても count は1以上の可能性がある。
/// </summary>
/// <param name="file"></param>
/// <param name="dst"></param>
/// <returns></returns>
int _parseSeqSep(TCHAR* file, SEQSEP* dst) {
	if (!file || !dst) {
		return -1;
	}
	dst->pstart = file;
	dst->head = -1;
	dst->tail = -1;
	dst->dext = -1;
	dst->digit = 0;
	dst->begin = 0;
	
	int len = 0;
	for (int i = 0; i < 4096; ++i) {
		if (file[i] == 0) {
			len = i;
			break;
		}
	}
	int offset = len - 1;
	for (; offset >= 0; --offset) {
		auto val = dst->pstart[offset];
		if (val == '/' || val == '\\') {
			break;
		}
		if (dst->dext < 0) {
			if (val == '.') {
				dst->dext = offset;
			}
		}
		else if (dst->tail < 0) {
			if (0x30 <= val && val <= 0x39) {
				dst->tail = offset;
				dst->head = dst->tail;
			}
		}
		else {
			if (0x30 <= val && val <= 0x39) {
				dst->head = offset;
			}
			else {
				break;
			}
		}
	}
	if (dst->head >= 0) {
		dst->digit = dst->tail - dst->head + 1;
		int adjust = dst->digit - 9;
		if (adjust > 0) {
			dst->head += adjust;
			dst->digit = 9;
		}
		for (int i = 0; i < dst->digit; ++i) {
			dst->begin = dst->begin * 10 + (dst->pstart[dst->head + i] - 0x30);
		}
	}

	return 1;
}


/// <summary>
/// hModuleから解決するファイルを含むフォルダ名を得る
/// </summary>
/// <param name="hModule"></param>
/// <param name="dst">書き出し先</param>
/// <param name="maxNum">最大個数</param>
/// <returns>nullを含まない個数</returns>
int _getModuleDir(HMODULE hModule, TCHAR* dst, int maxNum) {
	// null を含まない個数が返る
	auto num = GetModuleFileName(hModule, dst, maxNum);
	for (int i = num - 1; i >= 0; --i) {
		auto val = dst[i];
		if (val == '/' || val == '\\') {
			dst[i] = 0;
			return i;
		}
	}
	return -1;
}

