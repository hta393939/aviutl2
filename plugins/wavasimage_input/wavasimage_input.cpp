
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
	int audioTrack;
	int videoTrack;
} CONFIG;
static CONFIG config = {
	TEXT("_%05d"),
	1,
	30,
	1,
	-1,
	-1,
};

struct MY_FILE_HANDLE {
	int flag;
	static constexpr int FLAG_VIDEO = 1;
	static constexpr int FLAG_AUDIO = 2;
	HANDLE hFile;
	void* videoformat;
	LONG videoformatsize;
	void* audioformat;
	LONG audioformatsize;
	int dataTop;
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


//WCHAR gBaseName[STRBUF] = { 0 };

// 入力ファイルをクローズする関数へのポインタ
// ih		: 入力ファイルハンドル
bool func_close(INPUT_HANDLE ih) {
	MY_FILE_HANDLE* p = (MY_FILE_HANDLE*)ih;
	if (p->audioformat) {
		GlobalFree(p->audioformat);
	}
	if (p->videoformat) {
		GlobalFree(p->videoformat);
	}
	if (p->hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(p->hFile);
	}
	GlobalFree(p);
	return true;
}


// 入力ファイルをオープンする関数へのポインタ
// file		: ファイル名
// 戻り値	: TRUEなら入力ファイルハンドル
INPUT_HANDLE func_open(LPCWSTR file) {
	//StringCchCopy(gBaseName, STRBUF, file);

	HANDLE h = GlobalAlloc(GPTR, sizeof(MY_FILE_HANDLE));
	auto p = (MY_FILE_HANDLE*)h;
	if (!p) {
		return NULL;
	}
	p->hFile = INVALID_HANDLE_VALUE;
	p->videoformatsize = sizeof(BITMAPINFOHEADER);
	p->audioformatsize = sizeof(WAVEFORMATEX);
	p->videoformat = GlobalAlloc(GPTR, p->videoformatsize);
	p->audioformat = GlobalAlloc(GPTR, p->audioformatsize);
	if (!p->videoformat || !p->audioformat) {
		func_close(p);
		return NULL;
	}

	{
		auto pw = (WAVEFORMATEX*)p->audioformat;
		pw->wFormatTag = 3;
		pw->nChannels = 1;
		pw->nSamplesPerSec = 48000;
		pw->nBlockAlign = 4 * pw->nChannels;
		pw->nAvgBytesPerSec = pw->nSamplesPerSec * 4 * pw->nChannels;
		pw->wBitsPerSample = 32;
		pw->cbSize = 0;
	}
	{
		auto pv = (BITMAPINFOHEADER*)p->videoformat;
		pv->biSize = sizeof(p->videoformatsize);
		pv->biWidth = 256;
		pv->biHeight = 256;
		pv->biBitCount = 32;
		pv->biClrUsed = 0;
		pv->biPlanes = 1;
		pv->biCompression = 0;
	}

	p->hFile = CreateFile(file,
		FILE_GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (p->hFile == INVALID_HANDLE_VALUE) {
		func_close(p);
		return NULL;
	}

	// 未実装
	// オフセットの検知

	return p;
}


// 入力ファイルの情報を取得する関数へのポインタ
// ih		: 入力ファイルハンドル
// iip		: 入力ファイル情報構造体へのポインタ
// 戻り値	: TRUEなら成功
bool func_info_get(INPUT_HANDLE ih, INPUT_INFO* iip) {
	auto p = (MY_FILE_HANDLE*)ih;
	iip->flag = INPUT_INFO::FLAG_VIDEO | INPUT_INFO::FLAG_AUDIO;
	iip->rate = 30;
	iip->scale = 1;
	iip->n = 120;
	iip->audio_n = 120 * 48000;
	// ポインタ伝達でいいのか?
	iip->audio_format = (WAVEFORMATEX*)p->audioformat;
	iip->audio_format_size = p->audioformatsize;
	iip->format = (BITMAPINFOHEADER*)p->videoformat;
	iip->format_size = p->videoformatsize;
	return true;
}

// 画像データを読み込む関数へのポインタ
// ih		: 入力ファイルハンドル
// frame	: 読み込むフレーム番号
// buf		: データを読み込むバッファへのポインタ
// 戻り値	: 読み込んだデータサイズ
int func_read_video(INPUT_HANDLE ih, int frame, void* buf) {
	auto p = (MY_FILE_HANDLE*)ih;
	//int pxNum = p->videoformat;
	int width = 256;
	int height = 256;
	int pxNum = width * height;
	{
		auto p32 = (unsigned int*)buf;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				*p32 = 0xffffffff;
				++p32;
			}
		}
	}
	return pxNum * 4;
}

int func_read_audio(INPUT_HANDLE ih, int start, int length, void* buf) {
	auto p = (MY_FILE_HANDLE*)ih;
	int chNum = 1;
	int elementSize = 2;
	float* dst = (float*)buf;
	if (true) {
		elementSize = 2;
		short* src = (short*)nullptr + config.audioTrack;
		for (int i = 0; i < length; ++i) {
			//float val = *src;
			float val = 0.5f;
			*dst = val / 32768.0f;
			++src;
			++dst;
		}
	}
	else {
		elementSize = 4;
		float* src = (float*)nullptr + config.audioTrack;
		for (int i = 0; i < length; ++i) {
			*dst = *src;
			src += chNum;
			++dst;
		}
	}
	return length * elementSize;
}

int func_set_track(INPUT_HANDLE ih, int type, int index) {
	switch (type) {
	case INPUT_PLUGIN_TABLE::TRACK_TYPE_VIDEO:
		if (index < 0) {
			return 2;
		}
		if (index >= 2) {
			return -1;
		}
		config.videoTrack = index;
		break;
	case INPUT_PLUGIN_TABLE::TRACK_TYPE_AUDIO:
		if (index < 0) {
			return 2;
		}
		if (index >= 2) {
			return -1;
		}
		config.audioTrack = index;
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
INPUT_PLUGIN_TABLE input_plugin_table = {
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
	func_read_audio, // audio
	func_config,		//	設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_set_track,
	NULL, //func_time_to_frame,
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetInputPluginTable(void) {
	return &input_plugin_table;
}
