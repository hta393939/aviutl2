
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/input2.h"
#include "description_input.h"
#include "../lib/util.hpp"

#define SINGLE_CHANNEL (0)
#define TWO_CHANNEL (1)

#define APP_NAME "description_input"

#define WAVE_FORMAT_IEEE_FLOAT (3)

#define STRBUF (4096)

//---------------------------------------------------------------------
//		プラグイン内部変数
//---------------------------------------------------------------------
typedef struct CONFIG_ {
	TCHAR name[260];
	unsigned int rate;
	unsigned int scale;
	unsigned int count;
	int straighten;
	int audioTrack;
	int videoTrack;
} CONFIG;
static CONFIG config = {
	TEXT("_%05d"),
	30,
	1,
	50,
	1,
	0,
	0,
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

	DWORD lengthBySample;

	HANDLE buffer;
	int maxBufferByte;
};

TCHAR gDir[STRBUF] = { 0 };
// .au*2 を外した分
TCHAR gBase[STRBUF] = { 0 };
TCHAR gIni[STRBUF] = { 0 };

int resolvePath(HMODULE hModule) {
	auto len = GetModuleFileName(hModule, gDir, STRBUF);
	bool found = false;
	int offset = -1;
	for (int i = len - 1; i >= 0; --i) {
		auto val = gDir[i];
		if (offset < 0) {
			if (val == '.') {
				gDir[i] = 0;
				offset = i;
				StringCchCopy(gBase, STRBUF, gDir);
			}
		}
		else {
			if (val == '/' || val == '\\') {
				gDir[i] = 0;
				found = true;
				break;
			}
		}
	}
	if (!found) {
		return -1;
	}
	StringCchPrintf(gIni, STRBUF, TEXT("%s/%s.ini"), gDir, TEXT(APP_NAME));
	return 1;
}

int resolveIni(const TCHAR* src, TCHAR* dst, int maxNum) {
	StringCchCopy(dst, maxNum, src);
	int len = 0;
	for (int i = 0; i < maxNum; ++i) {
		if (dst[i] == 0) {
			len = i;
			break;
		}
	}
	for (int i = len - 1; i >= 0; --i) {
		auto val = dst[i];
		if (val == '.') {
			StringCchCopy(dst + i, STRBUF - i - 1, TEXT(".ini"));
			return 1;
		}
	}
	return -1;
}

int saveSetting(const CONFIG* src, const TCHAR* target) {
	TCHAR buf[STRBUF];
	StringCchPrintf(buf, STRBUF, TEXT("%d"), src->rate);
	WritePrivateProfileString(TEXT(APP_NAME), TEXT("rate"), buf, target);

	StringCchPrintf(buf, STRBUF, TEXT("%d"), src->scale);
	WritePrivateProfileString(TEXT(APP_NAME), TEXT("scale"), buf, target);

	StringCchPrintf(buf, STRBUF, TEXT("%d"), src->count);
	WritePrivateProfileString(TEXT(APP_NAME), TEXT("count"), buf, target);
	return 1;
}

/// <summary>
/// 
/// </summary>
/// <param name="dst">格納済み値はデフォルト値とする</param>
/// <param name="target"></param>
/// <returns></returns>
int loadSetting(CONFIG* dst, const TCHAR* target) {
	dst->rate = GetPrivateProfileInt(TEXT(APP_NAME), TEXT("rate"), dst->rate, target);
	dst->scale = GetPrivateProfileInt(TEXT(APP_NAME), TEXT("scale"), dst->scale, target);
	dst->count = GetPrivateProfileInt(TEXT(APP_NAME), TEXT("scale"), dst->count, target);
	return 1;
}


//---------------------------------------------------------------------
//		プラグイン設定関数
//---------------------------------------------------------------------
LRESULT CALLBACK func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam) {
	switch(umsg) {
		case WM_INITDIALOG:
			loadSetting(&config, gIni);

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

					saveSetting(&config, gIni);
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
	if (p->buffer) {
		GlobalFree(p->buffer);
	}
	if (p->hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(p->hFile);
	}
	GlobalFree(p);
	return true;
}

// 入力ファイルをオープンする関数へのポインタ
// file		: ファイル名
INPUT_HANDLE func_open(LPCWSTR file) {
	HANDLE h = GlobalAlloc(GPTR, sizeof(MY_FILE_HANDLE));
	auto p = (MY_FILE_HANDLE*)h;
	if (!p) {
		return NULL;
	}

	TCHAR fileIni[STRBUF] = { 0 };
	resolveIni(file, fileIni, STRBUF);
	loadSetting(&config, gIni);
	loadSetting(&config, fileIni);
	saveSetting(&config, fileIni);

	p->hFile = INVALID_HANDLE_VALUE;
	p->videoformatsize = sizeof(BITMAPINFOHEADER);
	p->audioformatsize = sizeof(WAVEFORMATEX);
	p->maxBufferByte = 1024 * 1024;
	p->videoformat = GlobalAlloc(GPTR, p->videoformatsize);
	p->audioformat = GlobalAlloc(GPTR, p->audioformatsize);
	p->buffer = GlobalAlloc(GPTR, p->maxBufferByte);
	if (!p->videoformat || !p->audioformat || !p->buffer) {
		func_close(p);
		return NULL;
	}

	{
		auto pv = (BITMAPINFOHEADER*)p->videoformat;
		pv->biSize = sizeof(p->videoformatsize);
		pv->biWidth = 256;
		pv->biHeight = 256;
		pv->biBitCount = 32;
		pv->biClrUsed = 0;
		pv->biPlanes = 1;
		pv->biCompression = BI_RGB;
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

	auto pa = (WAVEFORMATEX*)p->audioformat;
	pa->nChannels = 2;
	pa->nSamplesPerSec = 48000;

	// 書き出しはfloatとする
	pa->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	pa->nBlockAlign = pa->nChannels * 4;
	pa->nAvgBytesPerSec = pa->nSamplesPerSec * pa->nBlockAlign;
	pa->wBitsPerSample = 32;
	pa->cbSize = 0;
	
	p->lengthBySample = 48000;
	return p;
}


// 入力ファイルの情報を取得する関数へのポインタ
// ih		: 入力ファイルハンドル
// iip		: 入力ファイル情報構造体へのポインタ
bool func_info_get(INPUT_HANDLE ih, INPUT_INFO* iip) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	iip->flag = INPUT_INFO::FLAG_VIDEO | INPUT_INFO::FLAG_AUDIO;
	iip->rate = config.rate;
	iip->scale = config.scale;
	int denom = pa->nSamplesPerSec * iip->scale;
	iip->n = (p->lengthBySample * iip->rate + denom - 1) / denom;
	iip->audio_n = p->lengthBySample;
	// ポインタ伝達でいいのか?
	iip->audio_format = pa;
	iip->audio_format_size = p->audioformatsize;
	iip->format = (BITMAPINFOHEADER*)p->videoformat;
	iip->format_size = p->videoformatsize;
	return true;
}

int makeView(MY_FILE_HANDLE* p, int frame, void* buf) {
	auto pv = (BITMAPINFOHEADER*)p->videoformat;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	const int chIndex = config.audioTrack / 2;
	const int writeIndex = config.audioTrack % 2;
	const int fileChNum = 3;
	const int readBlockByte = fileChNum * 3;

	// サンプル単位時刻での開始時刻
	const int ratev = config.rate;
	const int scalev = config.scale;
	const int ratea = pa->nSamplesPerSec;
	int timestart = (frame - 4) * ratea * scalev / ratev;
	// サンプル要求長さ
	int timelength = 8 * ratea * scalev / ratev;
	// ファイル上のオフセット(サンプル時刻単位)
	int filestart = timestart;
	// ファイルへの要求長さ
	int filelength = timelength;
	// オフセットそのものは変更しないので4つとも必要

	if (filestart < 0) {
		filelength += filestart;
		filestart = 0;
	}

	int width = pv->biWidth;
	int height = pv->biHeight;
	int pxNum = width * height;
	int byteNum = pxNum * 4;
	DWORD opaque = 0xff3fff3f;
	DWORD empty = 0xff3f3f3f; // 上からARGB
	ZeroMemory(buf, byteNum);
	{
		if (config.videoTrack) {
			opaque = 0x80ffffff;
		}

		DWORD* p32;
		float maxVal = -9999.0f;
		float minVal = 9999.0f;
		int count = 0;
		int dx = 0;
		for (int x = 0; x < 0; ++x) {
			if (count >= config.count) {
				if (minVal <= maxVal) {
					// ドット打ち
					int top = (int)((1.0f - maxVal) * 32.0f + 0.5f);
					int bottom = (int)((1.0f - minVal) * 32.0f + 0.5f);

					for (int y = 0; y < height; ++y) {
						p32 = ((DWORD*)buf) + width * (height - 1 - y) + dx;
						if (top <= y && y <= bottom) {
							*p32 = opaque;
						}
						else {
							*p32 = empty;
						}
					}
					dx += 1;
				}

				maxVal = -9999.0f;
				minVal = 9999.0f;
				count = 0;
			}
			count += 1;

			int curTime = timestart + x;
			if (curTime < 0) {
				continue; // 無効値
			}
			int curBufferOffset = curTime - filestart;
			float fval = 0.0f;
			maxVal = (fval >= maxVal) ? fval : maxVal;
			minVal = (fval <= minVal) ? fval : minVal;
		}

		if (count >= 1 && minVal <= maxVal) {
			// ドット打ち
			int top = (int)((1.0f - maxVal) * 32.0f + 0.5f);
			int bottom = (int)((1.0f - minVal) * 32.0f + 0.5f);

			for (int y = 0; y < height; ++y) {
				p32 = ((DWORD*)buf) + width * (height - 1 - y) + dx;
				if (top <= y && y <= bottom) {
					*p32 = opaque;
				}
				else {
					*p32 = empty;
				}
			}
			dx += 1;
		}
	}
	return byteNum;
}

int makeData(MY_FILE_HANDLE* p, int frame, void* buf) {
	auto pv = (BITMAPINFOHEADER*)p->videoformat;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	auto fileChNum = 3;
	//int pxNum = p->videoformat;

	/*
	DWORD reqBufferByte = length * chNum * p->elementSize;
	if (p->maxBufferByte < reqBufferByte) {
		reqBufferByte = p->maxBufferByte;
	}
	SetFilePointer(p->hFile,
		p->indataStart + start * chNum * p->elementSize,
		NULL, FILE_BEGIN);
	DWORD read = 0;
	auto resultbuf = ReadFile(p->hFile, p->buffer, reqBufferByte, &read, NULL);
	if (resultbuf == FALSE) {
		return 0;
	}
	int realLength = read / chNum / p->elementSize;
	*/

	int width = pv->biWidth;
	int height = pv->biHeight;
	int pxNum = width * height;
	int byteNum = pxNum * 4;
	ZeroMemory(buf, byteNum);
	DWORD noData = 0x00000000;
	{
		auto pstart = (unsigned int*)buf;
		for (int y = 0; y < height; ++y) {
			auto p32 = pstart + (height - 1 - y) * width;
			for (int x = 0; x < width; ++x) {
				*p32 = 0x80ffffff;
				++p32;
			}
		}
	}
	return byteNum;
}

// 画像データを読み込む関数へのポインタ
// ih		: 入力ファイルハンドル
// frame	: 読み込むフレーム番号
// buf		: データを読み込むバッファへのポインタ
// 戻り値	: 読み込んだデータサイズ
int func_read_video(INPUT_HANDLE ih, int frame, void* buf) {
	auto p = (MY_FILE_HANDLE*)ih;
	if ((config.videoTrack % 2) == 0) {
		return makeView(p, frame, buf);
	}
	return makeData(p, frame, buf);
}

int func_read_audio(INPUT_HANDLE ih, int start, int length, void* buf) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto ph = (WAVEFORMATEX*)p->audioformat;
	const int fileChNum = 3;
	const int chIndex = config.audioTrack / 2;
	const int writeIndex = chIndex % 2;
	float* dst = (float*)buf;
	int sampleNum = 0;
	return sampleNum;
}

int func_set_track(INPUT_HANDLE ih, int type, int index) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	int numCh = 3;
	switch (type) {
	case INPUT_PLUGIN_TABLE::TRACK_TYPE_VIDEO:
		if (index < 0) {
			return numCh * 2;
		}
		if (index >= numCh * 2) {
			return -1;
		}
		config.videoTrack = index;
		break;
	case INPUT_PLUGIN_TABLE::TRACK_TYPE_AUDIO:
		if (index < 0) {
			return numCh;
		}
		if (index >= numCh) {
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
	return index;
}

int func_time_to_frame(INPUT_HANDLE ih, double time) {
	return 0;
}

INPUT_PLUGIN_TABLE input_plugin_table = {
	INPUT_PLUGIN_TABLE::FLAG_VIDEO
		| INPUT_PLUGIN_TABLE::FLAG_CONCURRENT
		| INPUT_PLUGIN_TABLE::FLAG_MULTI_TRACK
		| INPUT_PLUGIN_TABLE::FLAG_AUDIO,
	TEXT("description入力"),
	TEXT("desc File (*.desc)\0*.desc\0AllFile (*.*)\0*.*\0"),
	TEXT("description入力 v0.3.1 by ウサギ"),
	func_open,
	func_close,
	func_info_get, //
	func_read_video, // 
	func_read_audio, // 
	func_config,	//	設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_set_track,
	func_time_to_frame,
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetInputPluginTable(void) {
	return &input_plugin_table;
}


EXTERN_C BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		resolvePath(hinstDLL);
		loadSetting(&config, gIni);
		saveSetting(&config, gIni);
		break;
	default:
		break;
	}
	return TRUE;
}

