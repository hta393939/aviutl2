
#include <windows.h>

struct SEQSEP {
	TCHAR* pstart;
	TCHAR* phead;
	TCHAR* ptail;
	TCHAR* pdext;
	int count;
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


int _parseSeqSep(TCHAR* file, SEQSEP* dst) {
	dst->pstart = file;
	dst->phead = nullptr;
	dst->ptail = nullptr;
	dst->pdext = nullptr;
	dst->count = 0;
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
		if (!dst->pdext) {
			if (val == '.') {
				dst->pdext = dst->pstart + offset;
			}
		}
		else if (!dst->ptail) {
			if (0x30 <= val && val <= 0x39) {
				dst->ptail = dst->pstart + offset;
				dst->phead = dst->ptail;
			}
		}
		else {
			if (0x30 <= val && val <= 0x39) {
				dst->phead = dst->pstart + offset;
			}
			else {
				break;
			}
		}
	}
	if (dst->phead) {
		dst->count = ((int)dst->ptail) - ((int)dst->phead) + 1;
		int adjust = dst->count - 9;
		if (adjust > 0) {
			dst->phead += adjust;
			dst->count = 9;
		}
		for (int i = 0; i < dst->count; ++i) {
			dst->begin = dst->begin * 10 + (dst->phead[i] - 0x30);
		}
	}

	return 1;
}


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

