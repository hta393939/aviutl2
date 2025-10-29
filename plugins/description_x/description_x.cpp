
#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>

#include <memory>
#include <algorithm>
#include "../aviutl2_sdk/plugin2.h"
// PIXEL_RGBA
//#include "../aviutl2_sdk/filter2.h"
#include "../lib/util.hpp"

#define APP_NAME L"description_x"

#define WAVE_FORMAT_IEEE_FLOAT (3)

#define STRBUF (4096)

#define IDC_BUTTON 101
#define IDC_STATIC -1

EDIT_HANDLE* edit_handle;

//---------------------------------------------------------------------
//	オブジェクトエイリアスデータ定義
//---------------------------------------------------------------------
LPCSTR alias = u8R"(
[Object]
[Object.0]
effect.name=テキスト
サイズ=150.00
文字色=3bcb48
テキスト=さんぷる
[Object.1]
effect.name=標準描画
[Object.2]
effect.name=縁取り
サイズ=10
縁色=ffffff
)";

//---------------------------------------------------------------------
//	ウィンドウプロシージャ
//---------------------------------------------------------------------
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
	case WM_COMMAND:
		switch (LOWORD(wparam)) {
		case IDC_BUTTON:
			// 編集セクションで処理を呼び出す
			edit_handle->call_edit_section([](EDIT_SECTION* edit) {
				// エイリアスデータからオブジェクトを作成
				edit->create_object_from_alias(alias, edit->info->layer, edit->info->frame, 10);
				});
			SetFocus(NULL); // ボタンのフォーカスを外す
			return 0;
		}
		break;
	}
	return DefWindowProc(hwnd, message, wparam, lparam);
}






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

EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE *host) {
	// プラグインの情報を設定
	host->set_plugin_information(L"Sample Window Client v0.4.1 By ウサギ");

	// 自身のウィンドウを作成
	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpszClassName = APP_NAME;
	wcex.lpfnWndProc = wnd_proc;
	wcex.hInstance = GetModuleHandle(0);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClassEx(&wcex)) {
		return;
	}
	auto hwnd = CreateWindowEx(
		0,
		APP_NAME,
		APP_NAME,
		WS_POPUP, // 親ウィンドウの指定無しでWS_CHILDが作れないので一旦WS_POPUPで作成しています
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr,
		nullptr,
		GetModuleHandle(0),
		nullptr);
	if (!hwnd) {
		return;
	}
	// ボタンの作成
	CreateWindowEx(
		0,
		WC_BUTTON,
		L"オブジェクト作成",
		WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
		10, 10, 200, 40,
		hwnd,
		(HMENU)IDC_BUTTON,
		GetModuleHandle(0),
		nullptr);

	// ウィンドウを登録
	host->register_window_client(APP_NAME, hwnd);

	// 編集ハンドルを作成
	edit_handle = host->create_edit_handle();

}

