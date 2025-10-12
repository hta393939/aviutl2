
#include <windows.h>
#include <strsafe.h>

#include <memory>
#include "../aviutl2_sdk/filter2.h"
#include "description_filter.h"
#include "../lib/util.hpp"

#define APP_NAME "description_filter"

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

TCHAR gDir[STRBUF] = { 0 };
// .au*2 を外した分
TCHAR gBase[STRBUF] = { 0 };
TCHAR gIni[STRBUF] = { 0 };


//---------------------------------------------------------------------
//	フィルタ設定項目定義
//---------------------------------------------------------------------
auto width = FILTER_ITEM_TRACK(L"横", 100, 1, 1000);
auto height = FILTER_ITEM_TRACK(L"縦", 100, 1, 1000);
auto color = FILTER_ITEM_COLOR(L"色", 0xffffff);
auto frequency = FILTER_ITEM_TRACK(L"周波数", 1000, 1, 24000);
void* items[] = { &width, &height, &color, &frequency, nullptr };


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

bool func_video_proc(FILTER_PROC_VIDEO* video) {
	auto w = (int)width.value;
	auto h = (int)height.value;
	if (w <= 0 || h <= 0) {
		return false;
	}

	//video->scene->rate
	//video->scene->scale
	//video->object->frame_total
	// フレーム番号のオリジンは?? オブジェクトそのものの相対0-origin not 全体
	auto curFrame = video->object->frame;
	//video->object->

	// 指定サイズ、色の四角形の画像データを作成
	auto col = color.value;
	auto buffer = std::make_unique<PIXEL_RGBA[]>(w * h);
	auto p = buffer.get();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			p->r = col.r;
			p->g = col.g;
			p->b = col.b;
			p->a = 255;
			p++;
		}
	}

	video->set_image_data(buffer.get(), w, h);
	return true;
}

bool func_audio_proc(FILTER_PROC_AUDIO* audio) {
	// 使うなら 0L or 1R
	auto sample_index = audio->object->sample_index;
	// サンプル数
	auto sample_num = audio->object->sample_num;
	auto channel_num = audio->object->channel_num;

	// 指定周波数のサイン波の音声データを作成
	auto step = (3.141592653589793 * 2.0) * frequency.value / audio->scene->sample_rate;
	auto buffer = std::make_unique<float[]>(sample_num);
	auto p = buffer.get();
	for (int i = 0; i < sample_num; i++) {
		*p++ = (float)sin(sample_index++ * step);
	}

	for (int i = 0; i < channel_num; i++) {
		audio->set_sample_data(buffer.get(), i);
	}

	//audio->set_sample_data(nullptr, 0);
	//audio->set_sample_data(nullptr, 1);
	return true;
}

FILTER_PLUGIN_TABLE filter_plugin_table = {
	FILTER_PLUGIN_TABLE::FLAG_VIDEO
		| FILTER_PLUGIN_TABLE::FLAG_INPUT
		| FILTER_PLUGIN_TABLE::FLAG_AUDIO,
	TEXT("descriptionメディアオブジェクト"),
	nullptr,
	TEXT("descriptionメディアオブジェクト v0.3.1 by ウサギ"),
	items,
	func_video_proc, 
	func_audio_proc,
};


//---------------------------------------------------------------------
//	プラグインDLL初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) { // versionは本体のバージョン番号
	//resolvePath(hinstDLL);
	return true;
}

//---------------------------------------------------------------------
//	プラグインDLL解放関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void UninitializePlugin() {
}


//---------------------------------------------------------------------
//		出力プラグイン構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C FILTER_PLUGIN_TABLE __declspec(dllexport) * __stdcall GetFilterPluginTable(void) {
	return &filter_plugin_table;
}
