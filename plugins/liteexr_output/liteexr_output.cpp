
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/output2.h"
#include "../lib/util.hpp"
#include "liteexr_output.h"

#define STRBUF (4096)
#define BUFBYTE (1024)

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

/*
// 指数テーブル
float gk2[32];

void makeTable() {
	float k = 64.0f; // 2^6
	for (int i = 0; i < 32; ++i) {
		gk2[31 - i] = k;
		k *= 0.5f;
	}
	gk2[0] = gk2[1];
}

float u16tof(unsigned short u16) {
	if (u16 == 0) {
		return 0.0f;
	}
	int signBit = (u16 & 0x8000) ? 0x80000000 : 0;
	int biased = (int)((u16 >> 10) & 0x1f);
	int bits = (int)(u16 & 0x3ff); // 10bit
	if (biased == 0) { // ケチ表現
		return (signBit ? -1.0f : 1.0f) * gk2[0] * (float)bits;
	}

	if (biased == 31) { // 無限大またはNaN. 8bit exp
		unsigned int buf = 0x7f800000 | signBit | (bits << 13);
		float* p = (float*)&buf;
		return (*p);
	}

	float ret = gk2[biased] * (float)(bits | 0x400);
	if (signBit) {
		ret = -ret;
	}
	return ret;
}

unsigned short ftob16(float v) {
	int b32 = 0;
	CopyMemory(&b32, &v, 4);
	bool isSign = (b32 < 0);
	int biased = (b32 >> 23) & 0xff; // 0-255
	int exp2 = biased - 127;
	int bias16 = exp2 + 15;
	int frac = b32 & 0x7fffff; // 23bit

	u16 ret = (isSign) ? 0x8000 : 0x0000;
	if (biased == 255) {
		ret |= (u16)0x7c00;
		if (frac == 0) {
			return ret;
		}
		frac >>= 13;
		if (frac == 0) {
			frac = 1;
		}
		ret |= (u16)frac; // NaN
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
	ret |= (u16)frac;
	ret |= (u16)(bias16 << 10);
	return ret;
}
*/


//---------------------------------------------------------------------
//		出力プラグイン内部変数
//---------------------------------------------------------------------
typedef struct CONFIG_ {
	TCHAR name[260];
	int straighten;
} CONFIG;
static CONFIG config = {
	TEXT("_%05d"),
	0,
};

/// <summary>
/// 長さ無し。null-terminate な ascii を null つきで書き出す
/// </summary>
/// <param name="fh"></param>
/// <param name="str"></param>
/// <returns></returns>
int wnts(HANDLE fh, const char* str) {
	auto len = strnlen_s(str, 260);
	WriteFile(fh, str, len, NULL, NULL);
	char term[4] = { 0, 0, 0, 0 };
	WriteFile(fh, term, 1, NULL, NULL);
	return (int)(len + 1);
}

/// <summary>
/// バイト数有り
/// </summary>
/// <param name="fh"></param>
/// <param name="pv"></param>
/// <param name="num"></param>
/// <returns></returns>
int wfs(HANDLE fh, float* pv, int num) {
	auto len = num * 4;
	WriteFile(fh, &len, 4, NULL, NULL);
	WriteFile(fh, pv, len, NULL, NULL);
	return (4 + len);
}

/// <summary>
/// バイト数有り
/// </summary>
/// <param name="fh"></param>
/// <param name="pv"></param>
/// <param name="num"></param>
/// <returns></returns>
int wints(HANDLE fh, int* pv, int num) {
	auto len = num * 4;
	WriteFile(fh, &len, 4, NULL, NULL);
	WriteFile(fh, pv, len, NULL, NULL);
	return (4 + len);
}

/// <summary>
/// バイト数有り
/// </summary>
/// <param name="fh"></param>
/// <param name="pv"></param>
/// <param name="num"></param>
/// <returns></returns>
int wu8s(HANDLE fh, u8* pv, int num) {
	auto len = num;
	WriteFile(fh, &len, 4, NULL, NULL);
	WriteFile(fh, pv, len, NULL, NULL);
	return (4 + len);
}

/// <summary>
/// float 版
/// </summary>
/// <param name="pSrc"></param>
/// <param name="width"></param>
/// <param name="height"></param>
/// <param name="name"></param>
/// <param name="straighten"></param>
/// <returns></returns>
int makeExr(
	const unsigned short* pSrc,
	int width,
	int height,
	TCHAR* name,
	int straighten,
	int elemSize) {

	auto fh = CreateFile(
		name,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (fh == INVALID_HANDLE_VALUE) {
		return 0;
	}

	u8 buf[BUFBYTE];
	float fs[64];
	int ints[64];

	{ // magic
		buf[0] = 'v';
		buf[1] = '/';
		buf[2] = '1';
		buf[3] = 0x01;
		buf[4] = 0x02;
		buf[5] = 0x00;
		buf[6] = 0x00;
		buf[7] = 0x00;
		WriteFile(fh, buf, 8, NULL, NULL);
	}

	{ // dict
		{
			wnts(fh, "channels");
			wnts(fh, "chlist");

			ints[0] = 0x49;
			WriteFile(fh, ints, 4, NULL, NULL);

			buf[1] = 0x00;
			for (int i = 0; i < 4; ++i) {
				switch (i)
				{
				case 0:
					buf[0] = 'A';
					break;
				case 1:
					buf[0] = 'B';
					break;
				case 2:
					buf[0] = 'G';
					break;
				case 3:
					buf[0] = 'R';
					break;
				}
				wnts(fh, (char*)buf);

				// この位置に16バイト長さ表示はいらない
				ints[0] = (elemSize == 4) ? 2 : 1; // 2: float, 1: half
				ints[1] = 0;
				ints[2] = 1;
				ints[3] = 1;
				WriteFile(fh, ints, 16, NULL, NULL);
			}
			buf[0] = 0;
			wnts(fh, (char*)buf);
		}
		{ // 1バイトの0 圧縮無し
			wnts(fh, "compression");
			wnts(fh, "compression");
			buf[0] = 0;
			wu8s(fh, buf, 1);
		}
		{
			wnts(fh, "dataWindow");
			wnts(fh, "box2i");
			ints[0] = 0;
			ints[1] = 0;
			ints[2] = width - 1;
			ints[3] = height - 1;
			wints(fh, ints, 4);
		}
		{
			wnts(fh, "displayWindow");
			wnts(fh, "box2i");
			wints(fh, ints, 4);
		}
		{ // 1バイトの0 Yは下へ向かう
			wnts(fh, "lineOrder");
			wnts(fh, "lineOrder");
			buf[0] = 0;
			wu8s(fh, buf, 1);
		}
		{
			wnts(fh, "pixelAspectRatio");
			wnts(fh, "float");
			fs[0] = 1.0f;
			wfs(fh, fs, 1);
		}
		{
			wnts(fh, "screenWindowCenter");
			wnts(fh, "v2f");
			fs[0] = 0.0f;
			fs[1] = 0.0f;
			wfs(fh, fs, 2);
		}
		{
			wnts(fh, "screenWindowWidth");
			wnts(fh, "float");
			fs[0] = 1.0f;
			wfs(fh, fs, 1);
		}
		{ // 止め
			buf[0] = 0;
			wnts(fh, (char*)buf);
		}
	}

	// オフセットを取得する
	u64 offset = 0L;
	{
		LARGE_INTEGER fsize;
		GetFileSizeEx(fh, &fsize);
		offset = fsize.QuadPart;
		offset += (u64)(8 * height);
	}

	u32 byteNum = width * 4 * elemSize;
	for (int y = 0; y < height; ++y) {
		WriteFile(fh, &offset, 8, NULL, NULL);
		offset += (u64)(8 + byteNum);
	}

	int toIndex[4] = { 3, 2, 1, 0 };
	auto pTop = pSrc;
	for (int y = 0; y < height; ++y) {
		WriteFile(fh, &y, 4, NULL, NULL);
		WriteFile(fh, &byteNum, 4, NULL, NULL);

		if (elemSize == 4) {
			{ // A
				auto p = pTop;
				for (int x = 0; x < width; ++x) {
					float a = _u16tof(p[3]);
					WriteFile(fh, &a, elemSize, NULL, NULL);
					p += 4;
				}
			}
			for (int j = 1; j < 4; ++j) { // BGR
				int index = toIndex[j];
				auto p = pTop;
				for (int x = 0; x < width; ++x) {
					float a = _u16tof(p[3]);
					float k = (straighten && a != 0.0f) ? 1.0f / a : 1.0f;
					float val = _u16tof(p[index]) * k;
					WriteFile(fh, &val, elemSize, NULL, NULL);

					p += 4;
				}
			}
		}
		else { // elemSize == 2
			{ // A
				auto p = pTop;
				for (int x = 0; x < width; ++x) {
					u16 a = p[3];
					WriteFile(fh, &a, elemSize, NULL, NULL);
					p += 4;
				}
			}

			if (straighten) {
				for (int j = 1; j < 4; ++j) { // BGR
					int index = toIndex[j];
					auto p = pTop;
					for (int x = 0; x < width; ++x) {
						float a = _u16tof(p[3]);
						if (a != 1.0f && a != 0.0f) {
							float k = 1.0f / a;
							float val = _u16tof(p[index]) * k;
							u16 val16 = _ftob16(val);
							WriteFile(fh, &val16, elemSize, NULL, NULL);
						}
						else {
							u16 val16 = p[index];
							WriteFile(fh, &val16, elemSize, NULL, NULL);
						}
						p += 4;
					}
				}
			}
			else { // straighten 無し
				for (int j = 1; j < 4; ++j) { // BGR
					int index = toIndex[j];
					auto p = pTop;
					for (int x = 0; x < width; ++x) {
						u16 val16 = p[index];
						WriteFile(fh, &val16, elemSize, NULL, NULL);

						p += 4;
					}
				}
			}

		}

		pTop += width * 4;
	}

	if (fh) {
		CloseHandle(fh);
	}
	return 1;
}

bool func_output(OUTPUT_INFO *oip) {
	int	i;

	TCHAR path[STRBUF];
	TCHAR name[260];
	TCHAR ext[260];
	TCHAR buf[260];
	TCHAR *p,*p2,*p3;

	int frames = oip->n;

	// バッファのピクセル幅
	int width = oip->w;
	int height = oip->h;

	// 2: half, 4: float
	int elemSize = 2;

	// ファイル名
	StringCchCopy(path, STRBUF, oip->savefile);
	p2 = p3 = nullptr;
	for (p=path;*p;++p) {
		if (*p == '\\') { p2 = p + 1; }
		if (*p == '.') { p3 = p; }
	}
	if (p2 == NULL) p2 = path;
	if (p3 == NULL) p3 = p;
	StringCchCopy(ext, 260, p3);
	*p3 = 0;
	StringCchCopy(name, 260, p2);

	_makeTable();

	auto fourCC = MAKEFOURCC('H', 'F', '6', '4');
	for(i = 0; i < frames; ++i) {
		oip->func_rest_time_disp(i, frames);
		if(oip->func_is_abort()) {
			break;
		}

		auto pixelp = (float *)oip->func_get_video(i, fourCC);

		StringCchPrintf(buf, 260, config.name, i);
		StringCchPrintf(p2, STRBUF, TEXT("%s%s%s"), name, buf, ext);

		makeExr((const u16*)pixelp,
			width, height, path,
			config.straighten,
			elemSize);
		//oip->func_update_preview();
	}

	return true;
}


//---------------------------------------------------------------------
//		出力プラグイン設定関数
//---------------------------------------------------------------------
LRESULT CALLBACK func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam) {
	switch(umsg) {
		case WM_INITDIALOG:
			SetDlgItemText(hdlg,IDC_EDIT0, config.name);

			if (config.straighten == 0) {
				CheckDlgButton(hdlg, IDC_CHECK1, BST_UNCHECKED);
			} else {
				CheckDlgButton(hdlg, IDC_CHECK1, BST_CHECKED);
			}
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wparam)) {
				case IDCANCEL:
					EndDialog(hdlg, LOWORD(wparam));
					break;
				case IDOK:
					GetDlgItemText(hdlg,IDC_EDIT0, config.name, 260);

					if (IsDlgButtonChecked(hdlg, IDC_CHECK1) == BST_CHECKED) {
						config.straighten = 1;
					} else {
						config.straighten = 0;
					}
					EndDialog(hdlg, LOWORD(wparam));
					break;
			}
			break;
	}
	return FALSE;
}

bool func_config(HWND hwnd, HINSTANCE dll_hinst) {
	DialogBox(dll_hinst, TEXT("CONFIG"), hwnd, (DLGPROC)func_config_proc);
	return true;
}


WCHAR gConfigText[1024] = {0};
LPCWSTR func_get_config_text() {
	StringCchPrintf(gConfigText, 1024, TEXT("連番追加書式: %s, RGBをAで割る: %d"), config.name, config.straighten);
	return gConfigText;
}

//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	OUTPUT_PLUGIN_TABLE::FLAG_VIDEO, // フラグ
	TEXT("連番EXR出力"),			//	プラグインの名前
	TEXT("EXR File (*.exr)\0*.exr\0AllFile (*.*)\0*.*\0"),		//	出力ファイルのフィルタ
	TEXT("連番EXR出力 v0.4.2 by ウサギ"),	//	プラグインの情報
	func_output,		//	出力時に呼ばれる関数へのポインタ
	func_config,		//	出力設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_get_config_text,	//	出力設定データを取得する時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C OUTPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetOutputPluginTable(void) {
	return &output_plugin_table;
}
