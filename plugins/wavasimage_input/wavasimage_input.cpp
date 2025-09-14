
#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/input2.h"
#include "wavasimage_input.h"
#include "../lib/util.hpp"

#define SINGLE_CHANNEL (0)
#define TWO_CHANNEL (1)

#define APP_NAME "wavasimage_input"

#define STRBUF (4096)

// 一つ分の高さピクセル数
#define BELT_HEIGHT (128)
// 振幅描画部分の高さ
#define INBELT_HEIGHT (64)
// 2 ** 31
#define B31F (2147483648.0f)
// 2 ** 23
#define B23F (8388608.0f)

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
	100,
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

	// PCMだと44(WAVEFORMATEXだと46)
	int indataStart;
	DWORD byteData;
	DWORD lengthBySample;
	// 読み取り時に使用
	DWORD elementSize;
	// ファイル側のチャンネル数
	DWORD fileNumChannel;
	// ファイル側のタグ(元はWORD)
	DWORD fileTag;

	HANDLE buffer;
	int maxBufferByte;

	unsigned char head[256];
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
	return 0;

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
	return 0;

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

#define WAVE_FORMAT_IEEE_FLOAT (3)

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
	p->maxBufferByte = 8 * 4 * 1024 * 128;
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
		pv->biWidth = 1024;
		pv->biHeight = BELT_HEIGHT * 2;
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

	// オフセットの検知
	int offset = 0;
	DWORD data[2];
	DWORD dwRead = 0;
	BOOL result = ReadFile(p->hFile, data, 8, &dwRead, NULL);
	offset += 8;
	if (!result || dwRead != 8 || data[0] != MAKEFOURCC('R', 'I', 'F', 'F')) {
		func_close(p);
		return NULL;
	}

	result = ReadFile(p->hFile, data, 4, &dwRead, NULL);
	offset += 4;
	if (!result || dwRead != 4 || data[0] != MAKEFOURCC('W', 'A', 'V', 'E')) {
		func_close(p);
		return NULL;
	}

	while (true) {
		result = ReadFile(p->hFile, data, 8, &dwRead, NULL);
		offset += 8;
		if (!result || dwRead != 8) {
			func_close(p);
			return NULL;
		}

		if (data[0] == MAKEFOURCC('f', 'm', 't', ' ')) {
			if (data[1] < 16 || data[1] > 256) { // PCM16バイトは必須
				func_close(p);
				return NULL;
			}
			result = ReadFile(p->hFile, p->head, data[1], &dwRead, NULL);
			if (!result || dwRead != data[1]) {
				func_close(p);
				return NULL;
			}
			offset += dwRead;
		}
		else if (data[0] == MAKEFOURCC('d', 'a', 't', 'a')) {
			p->byteData = data[1];
			p->indataStart = offset;
			break;
		}
		else {
			SetFilePointer(p->hFile, data[1], NULL, FILE_CURRENT);
			offset += dwRead;
		}

	}

	auto pfh = (WAVEFORMATEX*)p->head;
	auto pa = (WAVEFORMATEX*)p->audioformat;
#if (SINGLE_CHANNEL != 0)
	pa->nChannels = 1;
#else
#if (TWO_CHANNEL != 0)
	pa->nChannels = 2;
#else
	pa->nChannels = pfh->nChannels;
#endif
#endif
	pa->nSamplesPerSec = pfh->nSamplesPerSec;
	if (pfh->wFormatTag != WAVE_FORMAT_PCM && pfh->wFormatTag != WAVE_FORMAT_IEEE_FLOAT) {
		func_close(p);
		return NULL;
	}
	p->fileNumChannel = pfh->nChannels;
	p->elementSize = (pfh->wBitsPerSample + 7) / 8;
	p->fileTag = pfh->wFormatTag;

	// 書き出しはfloatとする
	pa->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	pa->nBlockAlign = pa->nChannels * 4;
	pa->nAvgBytesPerSec = pa->nSamplesPerSec * pa->nBlockAlign;
	pa->wBitsPerSample = 32;
	pa->cbSize = 0;
	
	p->lengthBySample = p->byteData / p->elementSize / p->fileNumChannel;
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

/// <summary>
/// ビデオ
/// </summary>
/// <param name="p"></param>
/// <param name="frame">ビデオ単位のインデックス</param>
/// <param name="buf"></param>
/// <returns></returns>
int makeView(MY_FILE_HANDLE* p, int frame, void* buf) {
	auto pv = (BITMAPINFOHEADER*)p->videoformat;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	const int chIndex = config.audioTrack;
	const int writeIndex = chIndex % 2;
	const int fileChNum = p->fileNumChannel;
	const int readBlockByte = fileChNum * p->elementSize;


	// サンプル単位時刻での開始時刻
	const int ratev = config.rate;
	const int scalev = config.scale;
	const int ratea = pa->nSamplesPerSec;

	const int width = pv->biWidth;
	const int height = pv->biHeight;
	const int byteNum = width * height * 4;
	const DWORD opaque = 0xff3fff3f;
	const DWORD empty = 0xff3f3f3f; // 上からARGB

	ZeroMemory(buf, byteNum);

	// サンプル要求長さ
	const int timelength = config.count * width;
	// フレーム先頭に対応する時刻。ratev 30 or 60, scale 1
	const int framestart = frame * ratea * scalev / ratev;

	const int timestart = framestart - timelength / 2;

	// ファイル上のオフセット(サンプル時刻単位)
	int filestart = timestart;
	// ファイルへの要求長さ
	int filelength = timelength;

	// 読み取り先オフセット
	int bufferOffsetTime = (timestart < 0) ? -timestart : 0;

	if (filestart < 0) {
		filelength += filestart;
		filestart = 0;
	}
	int over = filestart + filelength - p->lengthBySample;
	if (over > 0) {
		filelength -= over;
	}

	DWORD reqBufferByte = filelength * readBlockByte;
	if (p->maxBufferByte < reqBufferByte) {
		reqBufferByte = p->maxBufferByte;
	}
	SetFilePointer(p->hFile,
		p->indataStart + filestart * readBlockByte,
		NULL, FILE_BEGIN);
	DWORD read = 0;
	auto resultbuf = ReadFile(p->hFile,
		((unsigned char*)p->buffer) + bufferOffsetTime * readBlockByte,
		reqBufferByte, &read, NULL);
	if (resultbuf == FALSE) {
		return 0;
	}
	// filestart からバッファ中で実際に有効なチック数
	int realLength = read / readBlockByte;
	{
		DWORD* p32;
		float maxVal = -9999.0f;
		float minVal = 9999.0f;
		int count = 0;
		// 波形ドット座標
		int dx = 0;
		for (int i = 0; i < timelength; ++i) {
			bool available = true;
			// ファイル全体での時刻
			int curTime = timestart + i;
			if (curTime < 0 || curTime >= p->lengthBySample) {
				available = false;
			}

			if (available) {
				int offsetSample = i * fileChNum + chIndex;
				float fval = 0.0f;
				if (p->elementSize == 2) {
					short* p16 = ((short*)p->buffer) + offsetSample;
					fval = ((float)*p16) / 32768.0f;
				}
				else if (p->elementSize == 1) {
					unsigned char* p8 = ((unsigned char*)p->buffer) + offsetSample;
					fval = (((float)*p8) - 128.0f) / 128.0f;
				}
				else if (p->elementSize == 3) {
					int val32 = 0;
					CopyMemory(&val32, ((unsigned char*)p->buffer) + offsetSample * p->elementSize, 3);
					fval = ((float)((val32 << 8) >> 8)) / B23F;
				}
				else {
					if (p->fileTag == WAVE_FORMAT_IEEE_FLOAT) {
						float* pf = ((float*)p->buffer) + offsetSample;
						fval = *pf;
					}
					else {
						int* p32 = ((int*)p->buffer) + offsetSample;
						fval = ((float)*p32) / B31F;
					}
				}
				maxVal = (fval >= maxVal) ? fval : maxVal;
				minVal = (fval <= minVal) ? fval : minVal;

				{ // データ描画
					// TODO: オフセットを正しく計算する
					// 0, BELT_HEIGHT + BELT_HEIGHT / 2 が framestart
					int shift = (i - timelength / 2) + width * (BELT_HEIGHT + BELT_HEIGHT / 2);
					int dx = shift % width;
					int dy = shift / width;
					if (0 <= shift && dy < height) {
						auto p32 = ((DWORD*)buf) + width * (height - 1 - dy) + dx;
						DWORD b = (fval < 0.0) ? 192 : 255;
						DWORD a = 0xff;
						fval = (fval < 0.0) ? -fval : fval;
						DWORD val32 = (DWORD)(fval * 32768.0f + 0.5f);
						DWORD r = val32 % 256;
						DWORD g = val32 / 256;
						*p32 = (a << 24) | (r << 16) | (g << 8) | b;
					}
				}

			}

			count += 1;
			if (count >= config.count) {
				if (minVal <= maxVal && dx < width) {
					// 波形ドット打ち
					int top = (int)((1.0f - maxVal) * 32.0f + 0.5f);
					int bottom = (int)((1.0f - minVal) * 32.0f + 0.5f);

					for (int y = 0; y < INBELT_HEIGHT; ++y) {
						p32 = ((DWORD*)buf) + width * (height - 1 - (y + (BELT_HEIGHT >>2))) + dx;
						*p32 = (top <= y && y <= bottom) ? opaque : empty;
					}
				}
				dx += 1;

				maxVal = -9999.0f;
				minVal = 9999.0f;
				count = 0;
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
	return makeView(p, frame, buf);
}

int func_read_audio(INPUT_HANDLE ih, int start, int length, void* buf) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	const int fileChNum = p->fileNumChannel;
	// 1tickあたりのバイト数
	const int readBlockByte = fileChNum * p->elementSize;
	const int chIndex = config.audioTrack;
	const int writeIndex = chIndex % 2;
	float* dst = (float*)buf;
	int sampleNum = 0;

	DWORD reqBufferByte = length * readBlockByte;
	if (p->maxBufferByte < reqBufferByte) {
		reqBufferByte = p->maxBufferByte;
	}
	SetFilePointer(p->hFile,
		p->indataStart + start * readBlockByte,
		NULL, FILE_BEGIN);
	DWORD read = 0;
	auto resultbuf = ReadFile(p->hFile, p->buffer, reqBufferByte, &read, NULL);
	if (resultbuf == FALSE) {
		return 0;
	}
	const int realLength = read / readBlockByte;

	ZeroMemory(buf, length * readBlockByte);

	if (p->elementSize == 2) {
		short* psrc = ((short*)p->buffer) + chIndex;
		for (int i = 0; i < realLength; ++i) {
			float val = ((float)*psrc) / 32768.0f;
#if (SINGLE_CHANNEL != 0)
			*dst = val;
			++dst;
			++sampleNum;
#else
			dst[writeIndex] = val;
			dst += 2;
			sampleNum += 2;
#endif

			psrc += fileChNum;
		}
	}
	else if (p->elementSize == 1) { // 未確認
		int byteOffset = chIndex;
		for (int i = 0; i < realLength; ++i) {
			unsigned char val = 0;
			CopyMemory(&val, ((unsigned char*)p->buffer) + byteOffset, 1);
			float fval = (((float)val) - 128.0f) / 128.0f;
#if (SINGLE_CHANNEL!=0)
			*dst = fval;
			++dst;
			++sampleNum;
#else
			dst[writeIndex] = fval;
			dst += 2;
			sampleNum += 2;
#endif

			byteOffset += fileChNum * 1;
		}
	}
	else if (p->elementSize == 3) { // 未確認
		int byteOffset = chIndex * 3;
		for (int i = 0; i < realLength; ++i) {
			int val = 0;
			CopyMemory(&val, ((unsigned char*)p->buffer) + byteOffset, 3);
			val = (val << 8) >> 8;
			float fval = ((float)val) / B23F;
#if (SINGLE_CHANNEL!=0)
			*dst = fval;
			++dst;
			++sampleNum;
#else
			dst[writeIndex] = fval;
			dst += 2;
			sampleNum += 2;
#endif
			byteOffset += fileChNum * 3;
		}
	}
	else {
		if (p->fileTag == WAVE_FORMAT_IEEE_FLOAT) {
			float* psrc = ((float*)p->buffer) + chIndex;
			for (int i = 0; i < realLength; ++i) {
				float val = *psrc;
#if (SINGLE_CHANNEL != 0)
				* dst = val;
				++dst;
				++sampleNum;
#else
				dst[writeIndex] = val;
				dst += 2;
				sampleNum += 2;
#endif
				psrc += fileChNum;
			}
		}
		else {
			int* psrc = ((int*)p->buffer) + chIndex;
			for (int i = 0; i < realLength; ++i) {
				float val = ((float)*psrc) / B31F;
#if (SINGLE_CHANNEL != 0)
				* dst = val;
				++dst;
				++sampleNum;
#else
				dst[writeIndex] = val;
				dst += 2;
				sampleNum += 2;
#endif
				psrc += fileChNum;
			}
		}
	}
	return sampleNum;
}

int func_set_track(INPUT_HANDLE ih, int type, int index) {
	auto p = (MY_FILE_HANDLE*)ih;
	auto pa = (WAVEFORMATEX*)p->audioformat;
	int numCh = p->fileNumChannel;
	switch (type) {
	case INPUT_PLUGIN_TABLE::TRACK_TYPE_VIDEO:
		if (index < 0) {
			return numCh;
		}
		if (index >= numCh) {
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
		//| INPUT_PLUGIN_TABLE::FLAG_CONCURRENT
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

