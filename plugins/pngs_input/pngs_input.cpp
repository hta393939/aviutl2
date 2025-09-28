
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/input2.h"
#include "pngs_input.h"
#include "../lib/util.hpp"
#include "./litepng.hpp"

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#define STRBUF (4096)
#define DATABUF (16384)

#define APPNAME "pngs_input"

//---------------------------------------------------------------------
//		プラグイン内部変数
//---------------------------------------------------------------------
typedef struct CONFIG_ {
	// 不使用
	TCHAR name[260];
	unsigned int rate;
	unsigned int scale;
	// 不使用
	int straighten;
} CONFIG;
static CONFIG config = {
	TEXT("_%05d"),
	30,
	1,
	1,
};

TCHAR gTempText[STRBUF] = { 0 };
// 縦が決まる前のヘッダ用
u8 gTempBuffer[DATABUF] = { 0 };

SEQSEP gSeqSep = { nullptr, -1, -1, -1, 0, 0 };

struct MY_HANDLE {
	int seqNum;
	DWORD videoformatsize;
	void* videoformat;
	DWORD bufferByte;
	void* buffer;
	HANDLE topFile;
	LitePng topParser;
};


/// <summary>
/// 未使用
/// </summary>
/// <param name="src"></param>
/// <returns></returns>
int saveSetting(CONFIG* src, WCHAR* file) {
	StringCchPrintf(gTempText, STRBUF, TEXT("%d"), src->rate);
	WritePrivateProfileString(TEXT(APPNAME), TEXT("rate"), gTempText, file);

	StringCchPrintf(gTempText, STRBUF, TEXT("%d"), src->scale);
	WritePrivateProfileString(TEXT(APPNAME), TEXT("scale"), gTempText, file);
	return 1;
}

int loadSetting(CONFIG* dst, WCHAR* file) {
	dst->rate = GetPrivateProfileInt(TEXT(APPNAME), TEXT("rate"), 30, file);
	dst->scale = GetPrivateProfileInt(TEXT(APPNAME), TEXT("scale"), 1, file);
	return 1;
}



//---------------------------------------------------------------------
//		プラグイン設定関数
//---------------------------------------------------------------------
LRESULT CALLBACK func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam) {
	switch(umsg) {
		case WM_INITDIALOG:
			//SetDlgItemText(hdlg,IDC_EDIT0, config.name);
			SetDlgItemInt(hdlg, IDC_EDIT0, config.rate, FALSE);
			SetDlgItemInt(hdlg, IDC_EDIT1, config.scale, FALSE);

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
					//GetDlgItemText(hdlg,IDC_EDIT0, config.name, 260);
					config.rate = GetDlgItemInt(hdlg, IDC_EDIT0, NULL, FALSE);
					config.scale = GetDlgItemInt(hdlg, IDC_EDIT1, NULL, FALSE);

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

WCHAR gBaseName[STRBUF] = { 0 };
WCHAR gLatter[STRBUF] = { 0 };

int makeNumFileName(TCHAR* dst, int index) {
	TCHAR digit[256] = { 0 };
	StringCchPrintf(digit, 256, TEXT("%%s%%0%dd%%s"), gSeqSep.digit);
	StringCchPrintf(dst, STRBUF,
		digit, gBaseName, index, gLatter);
	return index;
}


// 入力ファイルをクローズする関数へのポインタ
// ih		: 入力ファイルハンドル
// 戻り値	: TRUEなら成功
bool func_close(INPUT_HANDLE ih) {
	if (!ih) {
		return true;
	}
	auto p = (MY_HANDLE*)ih;
	if (p->topFile) {
		CloseHandle(p->topFile);
	}
	GlobalFree(p->buffer);
	GlobalFree(p->videoformat);
	GlobalFree(ih);
	return true;
}


// 入力ファイルをオープンする関数へのポインタ
// file		: ファイル名
// 戻り値	: TRUEなら入力ファイルハンドル
INPUT_HANDLE func_open(LPCWSTR file) {
	StringCchCopy(gBaseName, STRBUF, file);

	MY_HANDLE* p = (MY_HANDLE*)GlobalAlloc(GPTR, sizeof(MY_HANDLE));
	if (!p) {
		return NULL;
	}
	p->topFile = INVALID_HANDLE_VALUE;

	p->bufferByte = 8192 * 4 * 4 + 32;
	p->buffer = GlobalAlloc(GPTR, p->bufferByte);

	p->videoformatsize = sizeof(BITMAPINFOHEADER);
	p->videoformat = GlobalAlloc(GPTR, p->videoformatsize);

	if (!p->videoformat) {
		func_close(p);
		return NULL;
	}

	p->topFile = CreateFile(file, GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (p->topFile == INVALID_HANDLE_VALUE) {
		func_close(p);
		return NULL;
	}

	DWORD dwRead = 0;
	auto bresult = ReadFile(p->topFile, gTempBuffer, DATABUF, &dwRead, NULL);
	if (!bresult) {
		func_close(p);
		return NULL;
	}

	/*
	int result = p->topParser.parse(gTempBuffer, dwRead);
	if (result <= 0) {
		func_close(p);
		return NULL;
	}
	p->topParser.setRefBuffer((unsigned char*)p->buffer, p->bufferByte);
	*/

	{ // フォーマットの指定
		auto bih = (BITMAPINFOHEADER*)p->videoformat;
		bih->biSize = 0;
		bih->biWidth = 512;
		bih->biHeight = 512;
		//bih->biWidth = p->topParser.dwWidth;
		//bih->biHeight = p->topParser.dwHeight;
		bih->biPlanes = 1;
		bih->biBitCount = 2 * 4;
		bih->biCompression = MAKEFOURCC('H', 'F', '6', '4');
		bih->biSizeImage = 0;
		bih->biClrUsed = 0;
		bih->biClrImportant = 0;
	}

	_parseSeqSep((TCHAR*)file, &gSeqSep);
	StringCchCopy(gBaseName, STRBUF, file);
	if (gSeqSep.head >= 0) {
		gBaseName[gSeqSep.head] = 0;
	}
	if (gSeqSep.tail >= 0) {
		StringCchCopy(gLatter, STRBUF, file + gSeqSep.tail + 1);
	}

	int seqNum = 0;
	if (gSeqSep.digit >= 1) { // 1桁以上の数値が含まれる
		int cur = gSeqSep.begin;
		for (int i = 0; i < 10000; ++i) {
			makeNumFileName(gTempText, cur);
			auto result = PathFileExists(gTempText);
			if (!result) {
				break;
			}
			cur += 1;
			seqNum += 1;
		}
	}
	p->seqNum = seqNum;

	if (p->seqNum < 2) {
		//p->topParser.loadOffsetTable(p->topFile);
	}

	return p;
}

// 入力ファイルの情報を取得する関数へのポインタ
// ih		: 入力ファイルハンドル
// iip		: 入力ファイル情報構造体へのポインタ
// 戻り値	: TRUEなら成功
bool func_info_get(INPUT_HANDLE ih, INPUT_INFO* iip) {
	if (!ih) {
		return false;
	}
	auto p = (MY_HANDLE*)ih;
	{
		iip->flag = iip->FLAG_VIDEO;
		iip->rate = config.rate;
		iip->scale = config.scale;
		iip->n = (p->seqNum >= 2) ? p->seqNum : 120;
		iip->format_size = p->videoformatsize;
		iip->format = (BITMAPINFOHEADER*)p->videoformat;
	}
	{
		iip->audio_format_size = 0;
		iip->audio_format = nullptr;
	}
	return true;
}

// 画像データを読み込む関数へのポインタ
// ih		: 入力ファイルハンドル
// frame	: 読み込むフレーム番号
// buf		: データを読み込むバッファへのポインタ
// 戻り値	: 読み込んだデータサイズ
int func_read_video(INPUT_HANDLE ih, int frame, void* buf) {
	if (!ih) {
		return 0;
	}
	auto p = (MY_HANDLE*)ih;
	if (p->seqNum >= 2) {
		makeNumFileName(gTempText, gSeqSep.begin + frame);

		HANDLE f = CreateFile(gTempText,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (f == INVALID_HANDLE_VALUE) {
			return 0;
		}
		DWORD dwRead = 0;
		auto bresult = ReadFile(f, gTempBuffer, DATABUF, &dwRead, NULL);
		if (!bresult) {
			CloseHandle(f);
			return 0;
		}
		/*
		LiteExr parser;
		auto result = parser.parse(gTempBuffer, dwRead);
		if (result <= 0) {
			CloseHandle(f);
			return 0;
		}

		if ((parser.dwWidth != p->topParser.dwWidth) || (parser.dwHeight != p->topParser.dwHeight)) {
			CloseHandle(f);
			return 0;
		} // 先頭と解像度が一致していること

		parser.setRefBuffer((unsigned char*)p->buffer, p->bufferByte);
		parser.loadOffsetTable(f);
		int byteNum = parser.getData(f, (unsigned char*)buf);
		*/
		int byteNum = 0;
		CloseHandle(f);
		if (byteNum <= 0) {
			return 0;
		}
		return byteNum;
	}

	// 1枚のみ
	//int byteNum = p->topParser.getData(p->topFile, (unsigned char*)buf);
	int byteNum = 0;
	if (byteNum <= 0) {
		return 0;
	}
	return byteNum;
}


//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
INPUT_PLUGIN_TABLE input_plugin_table = {
		INPUT_PLUGIN_TABLE::FLAG_VIDEO,
	TEXT("連番PNG入力"),
	TEXT("PNG File (*.png)\0*.png\0AllFile (*.*)\0*.*\0"),
	TEXT("連番PNG入力 v0.3.1 by ウサギ"),
	func_open,
	func_close,
	func_info_get, //
	func_read_video,
	NULL, // audio
	func_config,		//	設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL, // func_set_track
	NULL, // func_time_to_frame,
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetInputPluginTable(void) {
	return &input_plugin_table;
}

