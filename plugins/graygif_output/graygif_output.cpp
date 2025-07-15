/**
 * @file graygif_output.cpp
 */

#include <windows.h>
#include <stdio.h>

#include "../../aviutl2_sdk/output2.h"
#include "graygif_output.h"

//#include <Shlwapi.h>

#include <strsafe.h>

#include <gdiplus.h>
using namespace Gdiplus;

//#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

#define STRBUF (4096)


HGLOBAL makeGif(unsigned char* pSrc,
			int imgWidth,
			int imgHeight,
			int* pByte,
			unsigned char* palette);
HGLOBAL makeGif7(unsigned char* pSrc,
			int bufWidth,
			int imgHeight,
			int* pByte,
			unsigned char* palette);

HMODULE gDLL = NULL;

/**
 * 16LEで書き込む。
 *
 * @param[out] buf 全体バッファ
 * @param[in] offset バッファのオフセット
 * @param[in] v 書き込む値
 */
int writeu16(unsigned char* buf, int offset, int v) {
	buf[offset + 0] =  v & 0xff;
	buf[offset + 1] = (v >> 8) & 0xff;
	return 2;
}


/// プラグイン側 ///

BOOL APIENTRY DllMain(HMODULE hinstDLL,DWORD fdwReason,LPVOID lpvReserved) {
	gDLL = hinstDLL;
	OutputDebugString(TEXT("DllMain graygif_output"));
	return TRUE;
}


//---------------------------------------------------------------------
//		出力プラグイン内部変数
//---------------------------------------------------------------------
typedef struct {
	int isAlpha; /**< 右をアルファとして使う */
	int repeat; /**< リピート */
} CONFIG;
static CONFIG config = {
	0,
	0
};

/**
 * パレットを生成する。
 *
 * @param[out] p 書き出し先
 */
void makePalette(ColorPalette* p) {
	int r, g, b;

	p->Flags = 0;
	p->Count = 256;

	p->Entries[0] = 0x00000000;

	for (int i = 1; i < 256; ++i) {
		r = i;
		g = i;
		b = i;
		p->Entries[i] = 0xff000000 | (r << 16) | (g << 8) | b;
	}

}

/// <summary>アニメgifファイル出力</summary>
int outputGif(OUTPUT_INFO* oip) {
	if (oip == nullptr) {
		//outBox(fp, "内部エラー: -1");
		return -1;
	}

	int width = oip->w;
	int height = oip->h;

	unsigned char chunk[64];
	unsigned char* buf = nullptr;
	unsigned int full = 0xFFFFffffL;

	Gdiplus::ColorPalette* palette = (Gdiplus::ColorPalette*)malloc(sizeof(Gdiplus::ColorPalette) + 256 * 4);
	makePalette(palette);
/*
	auto handle = CreateFile(oip->savefile,
		GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		return -7;
	}
	*/
	FILE* pfOut = NULL;
	_wfopen_s(&pfOut, oip->savefile, TEXT("wb"));
	if (pfOut == NULL) {
		//outBox(fp, "書き出しファイルのオープンに失敗しました");
		return -7;
	}

	int byteRead;

	int scaleNum = oip->scale;
	int rateDiv = oip->rate;

	if (rateDiv > 65535) {
		if (((scaleNum % 10000) == 0)
			&& ((rateDiv % 10000) == 0)) {
			scaleNum /= 10000;
			rateDiv /= 10000;
		}
		while (rateDiv > 65535) {
			scaleNum = (scaleNum + 9) / 10;
			rateDiv = (rateDiv + 9) / 10;
		}
	}

	unsigned char* buffer = nullptr;
	try {
		buffer = new unsigned char[width * height * 4];
	} catch (...) {
		buffer = nullptr;
	}
	if (buffer == nullptr) {
		fclose(pfOut);
		//outBox(fp, "メモリ不足です");
		return -8;
	}

	int repeat = config.repeat;

	int preTS = 0;
	int nowTS;

	int frames = oip->n;

	for (int i = 0; i < frames; ++i) {
		oip->func_rest_time_disp(i, frames);
		if (oip->func_is_abort()) {
			break;
		}

		unsigned char* ptop = (unsigned char*)oip->func_get_video(i, 0);

		unsigned char* target = ptop;

		HGLOBAL hRet;
		if (config.isAlpha == 0) {
			hRet = makeGif(target,
				width, height, &byteRead,
				(unsigned char*)palette);
		}
		else {
			hRet = makeGif7(target,
				width, height, &byteRead,
				(unsigned char*)palette);
		}

		unsigned char* buf = (unsigned char*)hRet;

		int idx = 0;
		int beginBody = -1;
		/// <summary>Graphic Control Extension かどうか</summary>
		int isGCE = 0;
		int endBody = -1;

		nowTS = (i + 1) * scaleNum * 100 / rateDiv;

		idx += 13 + 3 * 256; // GIF Header

		beginBody = idx;
		// 開始 Graphic Control Extension かどうか
		if (buf[idx] == 0x21 && buf[idx + 1] == 0xf9) {
			isGCE = 1;
		}

		// 止め
		if (buf[byteRead - 2] == 0x00 && buf[byteRead - 1] == 0x3b) {
			endBody = byteRead - 1;
		} else { // エラー
			endBody = byteRead;
		}

		if (isGCE) {
			int flags = (2 << 2) | 1; // 9
			buf[beginBody + 3] = (unsigned char)flags;

			int diff = nowTS - preTS;
			writeu16(buf, beginBody + 4, diff);
		} else {

		}


		if (i == 0) { // GIF Header
			fwrite(buf, 1, 13 + 3 * 256, pfOut);

			// Animation Extension
			chunk[0] = 0x21;
			chunk[1] = 0xff;

			chunk[2] = 11; // 3-13

			chunk[3] = 'N';
			chunk[4] = 'E';
			chunk[5] = 'T';
			chunk[6] = 'S';
			chunk[7] = 'C';
			chunk[8] = 'A';
			chunk[9] = 'P';
			chunk[10] = 'E';

			chunk[11] = '2';
			chunk[12] = '.';
			chunk[13] = '0';

			chunk[14] = 3;

			chunk[15] = 1;
			chunk[16] = repeat & 0xff;
			chunk[17] = (repeat >> 8) & 0xff;

			chunk[18] = 0;

			fwrite(chunk, 1, 19, pfOut);
		}

		fwrite(buf + beginBody, 1, endBody - beginBody, pfOut);

		if (hRet) {
			GlobalFree(hRet);
		}

		preTS = nowTS;
		Sleep(1);
	}
	delete [] buffer;

	if (pfOut) { // Comment Ex
		chunk[0] = 0x21;
		chunk[1] = 0xfe;

		chunk[2] = 3;

		chunk[3] = 'T';
		chunk[4] = 'K';
		chunk[5] = 'O';
		chunk[6] = 0x00;

		fwrite(chunk, 1, 7, pfOut);

		// Trailer
		chunk[0] = 0x3b;

		fwrite(chunk, 1, 1, pfOut);

		fclose(pfOut);

		//oip->func_update_preview(); // NOTE: 無いらしい
	}

	free(palette);

	//WCHAR str[STRBUF] ;
	//StringCbPrintf(str, STRBUF, TEXT("出力しました\n%d コマ"), oip->n);
	//MessageBoxW(NULL, str, oip->savefile, MB_OK);

	//CloseHandle(handle);
	return 0;
}


/**
 * @note
 * Graphic Control Extension
 * 正しいと 21 F9 04 "09" "03 00" 00   00
 * そして画像とかが続く。Image Block
 * 2C "00 00" "00 00" "40 01" "B4 00"
 * http://www.tohoho-web.com/wwwgif.htm
 * @endnote
 */


//---------------------------------------------------------------------
//		出力プラグイン出力関数
//---------------------------------------------------------------------
//
//	oip->flag;				// フラグ
//							// OUTPUT_INFO_FLAG_VIDEO	: 画像データあり
//							// OUTPUT_INFO_FLAG_AUDIO	: 音声データあり
//	oip->w,oip->h;			// 縦横サイズ
//	oip->rate,oip->scale;	// フレームレート
//	oip->n;					// フレーム数
//	oip->size;				// １フレームのバイト数
//	oip->audio_rate;		// 音声サンプリングレート
//	oip->audio_ch;			// 音声チャンネル数
//	oip->audio_n;			// 音声サンプリング数
//	oip->audio_size;		// 音声１サンプルのバイト数
//	oip->savefile;			// セーブファイル名へのポインタ
//
//	void *oip->func_get_video( int frame );
//							// DIB形式(RGB24bit)の画像データへのポインタを取得します。
//							// frame	: フレーム番号
//							// 戻り値	: データへのポインタ
//	void *oip->func_get_audio( int start,int length,int *readed );
//							// 16bitPCM形式の音声データへのポインタを取得します。
//							// start	: 開始サンプル番号
//							// length	: 読み込むサンプル数
//							// readed	: 読み込まれたサンプル数
//							// 戻り値	: データへのポインタ
//	BOOL oip->func_is_abort( void );
//							// 中断するか調べます。
//							// 戻り値	: TRUEなら中断
//	void oip->func_rest_time_disp( int now,int total );
//							// 残り時間を表示させます。
//							// now		: 処理しているフレーム番号
//							// total	: 処理する総フレーム数
//							// 戻り値	: TRUEなら成功
//	int oip->func_get_flag( int frame );
//							//	フラグを取得します。
//							//	frame	: フレーム番号
//							//	戻り値	: フラグ
//							//  OUTPUT_INFO_FRAME_FLAG_KEYFRAME		: キーフレーム推奨
//							//  OUTPUT_INFO_FRAME_FLAG_COPYFRAME	: コピーフレーム推奨
//	BOOL oip->func_update_preview( void );
//							//	プレビュー画面を更新します。
//							//	最後にfunc_get_videoで読み込まれたフレームが表示されます。
//							//	戻り値	: TRUEなら成功
//
bool func_output(OUTPUT_INFO *oip) {
	int result = outputGif(oip);
	if (result < 0) {
		return false;
	}
	return true;
}


//---------------------------------------------------------------------
//		出力プラグイン設定関数
//---------------------------------------------------------------------
LRESULT CALLBACK func_config_proc(HWND hdlg, UINT umsg, WPARAM wparam, LPARAM lparam) {
	switch(umsg) {
		case WM_INITDIALOG:
			SetDlgItemInt(hdlg, IDC_EDIT0, config.repeat,TRUE);
			if (config.isAlpha == 0) {
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
					config.repeat = GetDlgItemInt(hdlg, IDC_EDIT0,NULL,TRUE);
					if (IsDlgButtonChecked(hdlg, IDC_CHECK1) == BST_CHECKED) {
						config.isAlpha = 1;
					} else {
						config.isAlpha = 0;
					}
					EndDialog(hdlg, LOWORD(wparam));
					break;
			}
			break;
	}
	return FALSE;
}

/**
 * 
 *
 * @param[in] hwnd ウインドウハンドル
 * @param[in] dll_hinst インスタンスハンドル
 */
bool func_config(HWND hwnd, HINSTANCE dll_hinst) {
	DialogBox(dll_hinst, TEXT("CONFIG"), hwnd, (DLGPROC)func_config_proc);
	OutputDebugString(TEXT("_config"));
	return true;
}

/**
 *
 * @param[out] data 
 */
int func_config_get(void *data, int size) {
	if(data != nullptr) {
		CopyMemory(data, &config, sizeof(config));
	}
	OutputDebugString(TEXT("_get"));
	return sizeof(config);
}

/**
 *
 * @param[in] data
 */
int func_config_set(void *data, int size) {
	if(size != sizeof(config)) {
		return 0;
	}
	CopyMemory(&config, data, size);
	OutputDebugString(TEXT("_set"));
	return size;
}

LPCWSTR func_get_config_text() {
	WCHAR buf[STRBUF];
	StringCchPrintf(buf, STRBUF, TEXT("isAlpha,%d,repeat,%d"), config.isAlpha, config.repeat);
	return buf;
}


//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	OUTPUT_PLUGIN_TABLE::FLAG_VIDEO, // フラグ
	TEXT("グレーAGIF出力"),			//	プラグインの名前
	TEXT("GIF File (*.gif)\0*.gif\0AllFile (*.*)\0*.*\0"),		//	出力ファイルのフィルタ
	TEXT("グレーAGIF出力 v0.2.1 by ウサギ"),	//	プラグインの情報
	func_output,		//	出力時に呼ばれる関数へのポインタ
	func_config,		//	出力設定のダイアログを要求された時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	func_get_config_text,	//
};

//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C OUTPUT_PLUGIN_TABLE __declspec(dllexport)* __stdcall GetOutputPluginTable(void) {
	return &output_plugin_table;
}
