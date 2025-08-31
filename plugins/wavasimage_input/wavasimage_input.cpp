
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/input2.h"
#include "wavasimage_input.h"

#define SINGLE_CHANNEL (1)

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
	unsigned char head[64];
	// PCMだと44(WAVEFORMATEXだと46)
	int indataStart;
	DWORD byteData;
	DWORD lengthBySample;
	// 読み取り時に使用
	DWORD elementSize;
	// ファイル側のチャンネル数
	DWORD numChannel;

	HANDLE buffer;
	int maxBufferByte;
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
	if (p->buffer) {
		GlobalFree(p->buffer);
	}
	if (p->hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(p->hFile);
	}
	GlobalFree(p);
	return true;
}

#define WAVE_FORMAT_IEEE_FLOAT (3)

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

	// JUNKチャンクには対応しない
	// オフセットの検知
	int offset = 0;
	auto result = ReadFile(p->hFile, p->head, 48, NULL, NULL);
	if (result == FALSE) {
		func_close(p);
		return NULL;
	}
	DWORD* p32 = (DWORD*)p->head;
	if (p32[0] != MAKEFOURCC('R', 'I', 'F', 'F') || p32[2] != MAKEFOURCC('W','A','V','E')) {
		func_close(p);
		return NULL;
	}
	// PCM時
	if (p32[3] != MAKEFOURCC('f','m','t',' ') || p32[9] != MAKEFOURCC('d', 'a', 't', 'a')) {
		func_close(p);
		return NULL;
	}
	p->byteData = p32[10];
	p->indataStart = 44;

	auto pfh = (WAVEFORMATEX*)(p->head + 20);
	auto pa = (WAVEFORMATEX*)p->audioformat;
#if (SINGLE_CHANNEL != 0)
	pa->nChannels = 1;
#else
	pa->nChannels = pfh->nChannels;
#endif
	pa->nSamplesPerSec = pfh->nSamplesPerSec;
	if (pfh->wFormatTag != WAVE_FORMAT_PCM && pfh->wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
		func_close(p);
		return NULL;
	}
	p->numChannel = pfh->nChannels;
	p->elementSize = (pfh->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ? 4 : (pfh->wBitsPerSample / 8);

	// 書き出しはfloatとする
	pa->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	pa->nBlockAlign = pa->nChannels * 4;
	pa->nAvgBytesPerSec = pa->nSamplesPerSec * pa->nChannels * 4;
	pa->wBitsPerSample = 32;
	pa->cbSize = 0;
	
	p->lengthBySample = p->byteData / p->elementSize / p->numChannel;
	return p;
}


// 入力ファイルの情報を取得する関数へのポインタ
// ih		: 入力ファイルハンドル
// iip		: 入力ファイル情報構造体へのポインタ
bool func_info_get(INPUT_HANDLE ih, INPUT_INFO* iip) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	iip->flag = INPUT_INFO::FLAG_VIDEO | INPUT_INFO::FLAG_AUDIO;
	iip->rate = 30;
	iip->scale = 1;
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
	auto ph = (BITMAPINFOHEADER*)p->videoformat;
	int width = ph->biWidth * 0 + 1024;
	int height = ph->biHeight * 0 + 64;
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
		for (int x = 0; x < width; ++x) {
			int top = 4;
			int bottom = 60;
			for (int y = 0; y < height; ++y) {
				p32 = ((DWORD*)buf) + width * y + x;
				if (top <= y && y <= bottom) {
					*p32 = opaque;
				}
				else {
					*p32 = empty;
				}
			}
		}
	}
	return byteNum;
}

int makeData(MY_FILE_HANDLE* p, int frame, void* buf) {
	auto ph = (BITMAPINFOHEADER*)p->videoformat;
	//int pxNum = p->videoformat;
	int width = ph->biWidth * 0 + 256;
	int height = ph->biHeight * 0 + 256;
	int pxNum = width * height;
	int byteNum = pxNum * 4;
	ZeroMemory(buf, byteNum);
	{
		auto p32 = (unsigned int*)buf;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				*p32 = 0xffffffff;
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
	if (true) {
		return makeView(p, frame, buf);
	}
	return makeData(p, frame, buf);
}

int func_read_audio(INPUT_HANDLE ih, int start, int length, void* buf) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto ph = (WAVEFORMATEX*)p->audioformat;
	int chNum = ph->nChannels;
	float* dst = (float*)buf;
	int sampleNum = 0;

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

	if (p->elementSize == 2) {
		auto psrc = ((short*)p->buffer) + config.audioTrack;
		for (int i = 0; i < realLength; ++i) {
			if (true) {
				// SINGLE_CHANNEL 実装
				*dst = ((float)*psrc) / 32768.0f;
				++dst;
				++sampleNum;
			}
			psrc += chNum;
		}
	}
	else if (p->elementSize == 3) {
		int byteOffset = config.audioTrack * 3;
		for (int i = 0; i < realLength; ++i) {
			int val = 0;
			CopyMemory(&val, ((unsigned char*)p->buffer) + byteOffset, 3);

			val = (val << 8) >> 8;
			*dst = ((float)val) / ((float)0x1000000);
			++dst;
			++sampleNum;

			byteOffset += chNum * 3;
		}
	}
	else {
		auto psrc = ((float*)p->buffer) + config.audioTrack;
		float val = 0.0f;
		for (int i = 0; i < realLength; ++i) {
			// SINGLE_CHANNEL 実装
			*dst = *psrc;
			++dst;
			++sampleNum;

			psrc += chNum;
		}
	}
	return sampleNum;
}

int func_set_track(INPUT_HANDLE ih, int type, int index) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	int numCh = pa->nChannels;
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


//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
INPUT_PLUGIN_TABLE input_plugin_table = {
	INPUT_PLUGIN_TABLE::FLAG_VIDEO
		| INPUT_PLUGIN_TABLE::FLAG_CONCURRENT
		| INPUT_PLUGIN_TABLE::FLAG_MULTI_TRACK
		| INPUT_PLUGIN_TABLE::FLAG_AUDIO, // フラグ
	TEXT("wav画像入力"),			//	プラグインの名前
	TEXT("wav File (*.exr)\0*.wav\0AllFile (*.*)\0*.*\0"),		//	ファイルのフィルタ
	TEXT("wav画像入力 v0.3.1 by ウサギ"),	//	プラグインの情報
	func_open,		//	呼ばれる関数へのポインタ
	func_close,
	func_info_get, //
	func_read_video, // 
	func_read_audio, // 
	func_config,	//	設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_set_track,
	NULL, //func_time_to_frame,
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C INPUT_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetInputPluginTable(void) {
	return &input_plugin_table;
}
