
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/input2.h"
#include "wavasimage_input.h"

#define STRBUF (4096)

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



// 入力ファイルをオープンする関数へのポインタ
// file		: ファイル名
// 戻り値	: TRUEなら入力ファイルハンドル
INPUT_HANDLE func_open(LPCWSTR file) {
	StringCchCopy(gBaseName, STRBUF, file);

	{

	}

	return NULL;
}

// 入力ファイルをクローズする関数へのポインタ
// ih		: 入力ファイルハンドル
// 戻り値	: TRUEなら成功
bool func_close(INPUT_HANDLE ih) {
	GlobalFree(ih);
	return true;
}

// 入力ファイルの情報を取得する関数へのポインタ
// ih		: 入力ファイルハンドル
// iip		: 入力ファイル情報構造体へのポインタ
// 戻り値	: TRUEなら成功
bool func_info_get(INPUT_HANDLE ih, INPUT_INFO* iip) {
	return false;
}

// 画像データを読み込む関数へのポインタ
// ih		: 入力ファイルハンドル
// frame	: 読み込むフレーム番号
// buf		: データを読み込むバッファへのポインタ
// 戻り値	: 読み込んだデータサイズ
int func_read_video(INPUT_HANDLE ih, int frame, void* buf) {
	{

	}
	return 0;
}

int func_set_track(INPUT_HANDLE ih, int type, int index) {
	switch (type) {
	case INPUT_PLUGIN_TABLE::TRACK_TYPE_VIDEO:
		if (index < 0) {
			return 2;
		}
		break;
	case INPUT_PLUGIN_TABLE::TRACK_TYPE_AUDIO:
		if (index < 0) {
			return 2;
		}
		break;
	default:
		if (index < 0) {
			return 0;
		}
		return -1;
	}
	return 0;
}

int func_time_to_frame(INPUT_HANDLE ih, double time) {
	return 0;
}


//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
INPUT_PLUGIN_TABLE output_plugin_table = {
	INPUT_PLUGIN_TABLE::FLAG_VIDEO
		//| INPUT_PLUGIN_TABLE::FLAG_CONCURRENT
		//| INPUT_PLUGIN_TABLE::FLAG_MULTI_TRACK
		| INPUT_PLUGIN_TABLE::FLAG_AUDIO, // フラグ
	TEXT("wav画像入力"),			//	プラグインの名前
	TEXT("wav File (*.exr)\0*.wav\0AllFile (*.*)\0*.*\0"),		//	ファイルのフィルタ
	TEXT("wav画像入力 v0.3.1 by ウサギ"),	//	プラグインの情報
	func_open,		//	呼ばれる関数へのポインタ
	func_close,
	func_info_get, //
	func_read_video, // 
	NULL, // audio
	func_config,		//	設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_set_track,
	func_time_to_frame,
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetOutputPluginTable(void) {
	return &output_plugin_table;
}
