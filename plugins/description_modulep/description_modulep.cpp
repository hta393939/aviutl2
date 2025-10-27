
#include <windows.h>
#include <strsafe.h>

#include <memory>
#include <algorithm>
#include "../aviutl2_sdk/module2.h"
// PIXEL_RGBA
#include "../aviutl2_sdk/filter2.h"
#include "../lib/util.hpp"

#define APP_NAME "description_module"

#define WAVE_FORMAT_IEEE_FLOAT (3)

#define STRBUF (4096)

void f01(SCRIPT_MODULE_PARAM* param) {
	auto n = param->get_param_num();

	if (n != 4) {
		param->set_error(u8"引数の数が正しくありません");
		return;
	}
	auto p = (PIXEL_RGBA*)param->get_param_data(0);
	auto w = param->get_param_int(1);
	auto h = param->get_param_int(2);
	auto v = param->get_param_double(3);
	if (!p || w <= 0 || h <= 0) {
		param->set_error(u8"引数の値が正しくありません");
		return;
	}

	// 明るさを調整
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			/*
			p->r = (unsigned char)std::clamp(p->r * v, 0.0, 255.0);
			p->g = (unsigned char)std::clamp(p->g * v, 0.0, 255.0);
			p->b = (unsigned char)std::clamp(p->b * v, 0.0, 255.0);
			*/
			p++;
		}
	}

	param->push_result_array_double(nullptr, 0);

	return;
}

void f02(SCRIPT_MODULE_PARAM* param) {
	auto num = param->get_param_num();
	double total = 1.23;
	param->push_result_double(total);
	return;
}

SCRIPT_MODULE_FUNCTION functions[] = {
	{ L"f01", f01 },
	{ L"f02", f02 },
	{ nullptr }
};

SCRIPT_MODULE_TABLE script_module_table = {
	TEXT("descriptionスクリプトモジュール v0.3.1 by ウサギ"),
	functions,
};


//---------------------------------------------------------------------
//	プラグインDLL初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version) { // versionは本体のバージョン番号
	return true;
}

//---------------------------------------------------------------------
//	プラグインDLL解放関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void UninitializePlugin() {
}

EXTERN_C SCRIPT_MODULE_TABLE __declspec(dllexport) * __stdcall GetScriptModuleTable(void) {
	return &script_module_table;
}
