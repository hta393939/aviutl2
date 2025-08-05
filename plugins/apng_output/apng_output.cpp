/**
 * @file apng_output.cpp
 */

#include <windows.h>
#include <strsafe.h>
#include "../aviutl2_sdk/output2.h"
#include "apng_output.h"

#define STRBUF (4096)

int makePng(unsigned char* pSrc,
			int imgWidth,
			int imgHeight,
			wchar_t* name);
int makePng7(const unsigned char* pSrc,
			int bufWidth,
			int imgHeight,
			const wchar_t* name);
HGLOBAL makeMemoryPng(const float* pSrc,
	int imgWidth,
	int imgHeight,
	int* pByte,
	int isStraighten);

/// プラグイン側 ///

//---------------------------------------------------------------------
//		出力プラグイン内部変数
//---------------------------------------------------------------------
typedef struct CONFIG_ {
	TCHAR name[260];
	int isStraighten;
} CONFIG;
static CONFIG config = {
	L"_%05d",
	1,
};


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
//	BOOL oip->func_rest_time_disp( int now,int total );
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
	int frames = oip->n;

	// バッファのピクセル幅
	int height = oip->h;
	// 処理ピクセル幅
	int width = oip->w;

// ファイル名
	//StringCchCopy(path, 800, oip->savefile);

	auto fourCC = MAKEFOURCC('H', 'F', '6', '4');
	for(int i = 0; i < frames; ++i) {
		oip->func_rest_time_disp(i, frames);
		if(oip->func_is_abort()) {
			break;
		}

		auto pixelp = oip->func_get_video(i, fourCC);
		//wprintf_s(buf,1000,config.name,i);
		//wprintf_s(p2,800, L"%s%s%s",name,buf,ext);

		int byteNum = 0;
		auto mem = makeMemoryPng((const float*)pixelp,
			width, height, 
			&byteNum,
			config.isStraighten);

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
			SetDlgItemText(hdlg, IDC_EDIT0, config.name);

			if (config.isStraighten == 0) {
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
					GetDlgItemText(hdlg, IDC_EDIT0, config.name, sizeof(config.name));

					if (IsDlgButtonChecked(hdlg, IDC_CHECK1) == BST_CHECKED) {
						config.isStraighten = 1;
					} else {
						config.isStraighten = 0;
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
	StringCchPrintf(gConfigText, STRBUF, TEXT("name, %d, isStraighten, %d"), config.name, config.isStraighten);
	return gConfigText;
}

//---------------------------------------------------------------------
//		出力プラグイン構造体定義
//---------------------------------------------------------------------
OUTPUT_PLUGIN_TABLE output_plugin_table = {
	OUTPUT_PLUGIN_TABLE::FLAG_VIDEO, // フラグ
	L"APNG出力",			//	プラグインの名前
	L"PNG File (*.png)\0*.png\0AllFile (*.*)\0*.*\0",		//	出力ファイルのフィルタ
	L"APNG出力 v0.2.1 by ウサギ",	//	プラグインの情報
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

