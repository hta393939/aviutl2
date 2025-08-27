
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/output2.h"
#include "apng_output.h"
#include "apng.hpp"

#define STRBUF (4096)
#define LOCALBUF (260)
#define MAXCHUNKNUM (256)

//---------------------------------------------------------------------
//		出力プラグイン内部変数
//---------------------------------------------------------------------
typedef struct CONFIG_ {
	int repeat;
	int straighten;
} CONFIG;
static CONFIG config = {
	0,
	1,
};


bool func_output(OUTPUT_INFO *oip) {
	int frames = oip->n;

	// ピクセル高さ
	int height = oip->h;
	// ピクセル幅
	int width = oip->w;

	int rate = oip->rate;
	int scale = oip->scale;
	if (scale > 65535 || rate > 65535) {
		auto minVal = (scale <= rate) ? scale : rate;
		for (int i = minVal; i >= 2; --i) {
			if ((scale % i) == 0 && (rate % i) == 0) {
				scale /= i;
				rate /= i;
			}
		} // 通分
		if (scale > 65536 || rate > 65535) {
			// 小さくする
			if (rate < 32768) {
				rate = (rate * 60000 + scale - 1) / scale;
				scale = 60000;
			}
			else {
				do {
					scale = (scale + 9) / 10;
					rate = (rate + 9) / 10;
				} while (scale > 65536 || rate > 65536);
			}
		}
	}

	auto fh = CreateFile(oip->savefile,
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		return false;
	}

	u8 buf[LOCALBUF] = { 0 };
	CHUNK pChunk[MAXCHUNKNUM];
	//CHUNK* pChunk = new CHUNK[MAXCHUNKNUM];
	int seq = 0;
	int retVal = 0;

	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	auto ccIDAT = MAKEFOURCC('I', 'D', 'A', 'T');
	auto fourCC = MAKEFOURCC('H', 'F', '6', '4');
	for(int i = 0; i < frames; ++i) {
		oip->func_rest_time_disp(i, frames);
		if(oip->func_is_abort()) {
			break;
		}

		auto pixelp = oip->func_get_video(i, fourCC);

		int resultByteNum = 0;
		auto mem = makeMemoryPng((const unsigned short*)pixelp,
			width, height, 
			&resultByteNum,
			config.straighten,
			i);
		if (!mem) {
			retVal = -1;
			break;
		}

		// パースする
		if (!pChunk) {
			retVal = -2;
			break;
		}
		auto num = search((u8*)mem, resultByteNum, pChunk, MAXCHUNKNUM);

		if (i == 0) { // 初回のとき
			{ // signature
				int byteNum = 8;
				WriteFile(fh, ((u8*)mem), byteNum, NULL, NULL);
			}
			for (int j = 0; j < num; ++j) { // IHDR などを書き出す
				auto cc = pChunk[j].cc;
				if (cc == ccIDAT) {
					break; // IDAT 以降はヘッダっぽく扱わない
				}

				if (cc == MAKEFOURCC('p', 'H', 'Y', 's')) {
					continue; // 入れない
				}

				int byteNum = pChunk[j].bodyByte + 12;
				u8* p = ((u8*)mem) + pChunk[j].offset;
				WriteFile(fh, p, byteNum, NULL, NULL);
			}

			{
				int byteNum = 20;
				writeu32be(buf, 8, frames, LOCALBUF);
				writeu32be(buf, 12, config.repeat, LOCALBUF); // repeat 数 0 は無限ループ
				makeChunk(buf, 0, byteNum, MAKEFOURCC('a', 'c', 'T', 'L'));
				WriteFile(fh, buf, byteNum, NULL, NULL);
			}
		}

		{ // fcTL
			int byteNum = 26 + 12;
			writeu32be(buf, 8, seq, LOCALBUF);
			++seq;
			writeu32be(buf, 12, width, LOCALBUF);
			writeu32be(buf, 16, height, LOCALBUF);
			writeu32be(buf, 20, 0, LOCALBUF); // x_offset
			writeu32be(buf, 24, 0, LOCALBUF); // y_offset

			// rate: 30, scale: 1
			writeu16be(buf, 28, scale, LOCALBUF); // nume 
			writeu16be(buf, 30, rate, LOCALBUF); // deno

			buf[32] = 0; // dispose_op 0: 残す、1: 透過(0: 上書きなので0でもよい)
			buf[33] = 0; // blend_op 0: 上書き
			auto result = makeChunk(buf, 0, byteNum, MAKEFOURCC('f', 'c', 'T', 'L'));
			WriteFile(fh, buf, byteNum, NULL, NULL);
		}

		for (int j = 0; j < num; ++j) {
			auto cc = pChunk[j].cc;
			if (cc != ccIDAT) {
				continue;
			}


			if (i == 0) { // IDAT のまま
				u8* p = ((u8*)mem) + pChunk[j].offset;
				int byteNum = pChunk[j].bodyByte + 12;
				WriteFile(fh, p, byteNum, NULL, NULL);
			}
			else { // 初回じゃないとき fdAT
				// NOTE: ここが怪しいのだが;;
				u8* p = ((u8*)mem);
				auto top = pChunk[j].offset - 4;
				int byteNum = pChunk[j].bodyByte + 12 + 4;
				writeu32be(p, top + 8, seq, top + 12);
				++seq;
				auto result = makeChunk(
					p,
					top,
					byteNum, MAKEFOURCC('f', 'd', 'A', 'T'));
				WriteFile(fh, p + top, byteNum, NULL, NULL);
			}
		}

		if (mem) {
			GlobalFree(mem);
		}

		//oip->func_update_preview();
	}


	{ // 最後に書き出す
		int byteNum = 16 + 12;
		buf[ 8] = 'C';
		buf[ 9] = 'o';
		buf[10] = 'm';
		buf[11] = 'm';
		buf[12] = 'e';
		buf[13] = 'n';
		buf[14] = 't';
		buf[15] = 0;
		buf[16] = 'f';
		buf[17] = 'f';
		buf[18] = 'f';
		buf[19] = 'f';
		buf[20] = 'f';
		buf[21] = 'f';
		buf[22] = 'f';
		buf[23] = 'e';
		auto result = makeChunk(buf, 0, byteNum, MAKEFOURCC('i', 'T', 'X', 't'));
		WriteFile(fh, buf, byteNum, NULL, NULL);
	}
	{ // IEND
		int byteNum = 12;
		auto result = makeChunk(buf, 0, byteNum, MAKEFOURCC('I', 'E', 'N', 'D'));
		WriteFile(fh, buf, byteNum, NULL, NULL);
	}
	CloseHandle(fh);

	GdiplusShutdown(gdiplusToken);

	if (retVal < 0) {
		return false;
	}
	return true;
}


//---------------------------------------------------------------------
//		出力プラグイン設定関数
//---------------------------------------------------------------------
LRESULT CALLBACK func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam) {
	BOOL success = FALSE;
	switch(umsg) {
		case WM_INITDIALOG:
			SetDlgItemInt(hdlg, IDC_EDIT0, config.repeat, FALSE);

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
					config.repeat = GetDlgItemInt(hdlg, IDC_EDIT0, &success, FALSE);
					if (success == FALSE) {
						config.repeat = 0;
					}

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
	DialogBox(dll_hinst, L"CONFIG", hwnd, (DLGPROC)func_config_proc);
	return true;
}

/// <summary>
/// 未使用
/// </summary>
/// <param name="data"></param>
/// <param name="size"></param>
/// <returns></returns>
int func_config_get(void *data, int size) {
	if(data) {
		memcpy(data, &config, sizeof(config));
	}
	return sizeof(config);
}

/// <summary>
/// 未使用
/// </summary>
/// <param name="data"></param>
/// <param name="size"></param>
/// <returns></returns>
int func_config_set(void *data, int size) {
	if(size != sizeof(config)) {
		return NULL;
	}
	memcpy(&config, data, size);
	return size;
}

WCHAR gConfigText[STRBUF] = { 0 };
LPCWSTR func_get_config_text() {
	StringCchPrintf(gConfigText, STRBUF,
		TEXT("繰り返し回数: %d, RGBをAで割る: %d"),
		config.repeat,
		config.straighten);
	return gConfigText;
}

//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	OUTPUT_PLUGIN_TABLE::FLAG_VIDEO, // フラグ
	L"APNG出力",			//	プラグインの名前
	L"PNG File (*.png)\0*.png\0AllFile (*.*)\0*.*\0",		//	出力ファイルのフィルタ
	L"APNG出力 v0.3.1 by ウサギ",	//	プラグインの情報
	func_output,		//	出力時に呼ばれる関数へのポインタ
	func_config,		//	出力設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_get_config_text,	//	出力設定データを取得する時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C OUTPUT_PLUGIN_TABLE __declspec(dllexport)* __stdcall GetOutputPluginTable(void) {
	return &output_plugin_table;
}

