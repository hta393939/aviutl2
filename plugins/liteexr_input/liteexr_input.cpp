
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/input2.h"
#include "liteexr_input.h"
#include "../lib/util.hpp"
#include "./liteexr.hpp"

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#define STRBUF (4096)

#define APPNAME "liteexr_input"

//---------------------------------------------------------------------
//		プラグイン内部変数
//---------------------------------------------------------------------
typedef struct CONFIG_ {
	TCHAR name[260];
	unsigned int rate;
	unsigned int scale;
	int straighten;
} CONFIG;
static CONFIG config = {
	TEXT("_%05d"),
	1,
	30,
	1,
};

TCHAR gTempText[STRBUF] = { 0 };
u8 gTempBuffer[STRBUF] = { 0 };

SEQSEP gSeqSep = { nullptr, -1, -1, -1, 0, 0 };

struct MY_HANDLE {
	int seqNum;
	DWORD videoformatsize;
	void* videoformat;
	LiteExr topParser;
	HANDLE topFile;
};


int saveSetting(CONFIG* src) {
	StringCchPrintf(gTempText, STRBUF, TEXT("%d"), src->rate);
	WritePrivateProfileString(TEXT(APPNAME), TEXT("rate"), gTempText, nullptr);

	StringCchPrintf(gTempText, STRBUF, TEXT("%d"), src->scale);
	WritePrivateProfileString(TEXT(APPNAME), TEXT("scale"), gTempText, nullptr);
	return 1;
}

int loadSetting(CONFIG* dst) {
	dst->rate = GetPrivateProfileInt(TEXT(APPNAME), TEXT("rate"), 30, nullptr);
	dst->scale = GetPrivateProfileInt(TEXT(APPNAME), TEXT("scale"), 1, nullptr);
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


WCHAR gConfigText[1024] = {0};
//LPCWSTR func_get_config_text() {
//	StringCchPrintf(gConfigText, 1024, TEXT("連番追加書式: %s, RGBをAで割る: %d"), config.name, config.straighten);
//	return gConfigText;
//}


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
	auto bresult = ReadFile(p->topFile, gTempBuffer, STRBUF, &dwRead, NULL);
	if (!bresult) {
		func_close(p);
		return NULL;
	}

	int result = p->topParser.parse(gTempBuffer, dwRead);
	if (result <= 0) {
		func_close(p);
		return NULL;
	}

	{ // フォーマットの指定
		auto bih = (BITMAPINFOHEADER*)p->videoformat;
		bih->biSize = 0;
		bih->biWidth = p->topParser.dwWidth;
		bih->biHeight = p->topParser.dwHeight;
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
		int seqNum = 0;
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
		iip->n = (p->seqNum >= 2) ? p->seqNum : 60;
		iip->format_size = p->videoformatsize;
		iip->format = (BITMAPINFOHEADER*)p->videoformat;
	}
	{
		iip->audio_format_size = 0;
	}
	return false;
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
		TCHAR numFilename[STRBUF];
		StringCchPrintf(numFilename, STRBUF, TEXT("%d.exr"), gSeqSep.begin + frame);

		HANDLE f = CreateFile(numFilename,
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
		auto bresult = ReadFile(f, buf, 256, &dwRead, NULL);
		if (!bresult) {
			return 0;
		}
		LiteExr parser;
		auto result = parser.parse(nullptr, dwRead);
		if (result <= 0) {
			return 0;
		}

		int byteNum = parser.getData(f, (unsigned char*)buf);
		if (byteNum <= 0) {
			return 0;
		}
		return byteNum;
	}

	// 1枚のみ
	int byteNum = p->topParser.getData(p->topFile, (unsigned char*)buf);
	if (byteNum <= 0) {
		return 0;
	}
	return byteNum;
}


//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
INPUT_PLUGIN_TABLE input_plugin_table = {
	INPUT_PLUGIN_TABLE::FLAG_VIDEO, // フラグ
	TEXT("連番EXR入力"),			//	プラグインの名前
	TEXT("EXR File (*.exr)\0*.exr\0AllFile (*.*)\0*.*\0"),		//	ファイルのフィルタ
	TEXT("連番EXR入力 v0.3.1 by ウサギ"),	//	プラグインの情報
	func_open,		//	呼ばれる関数へのポインタ
	func_close,
	func_info_get, //
	func_read_video, // 
	NULL, // audio
	func_config,		//	設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,
	NULL,
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetInputPluginTable(void) {
	return &input_plugin_table;
}
