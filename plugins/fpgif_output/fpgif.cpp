/**
 * @file graygif.cpp
 */

//#define NOMINMAX
#include <windows.h>
//#include <algorithm>

#include <gdiplus.h>

typedef unsigned char u8;

using namespace Gdiplus;

/**
 * MSDNのHelper Function
 *
 * @param[in] format L"image/gif" などを指定する。
 * @param[out] pClsid ここに出力される。
 */
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	 UINT  num = 0;
	 UINT  size = 0;
	 ImageCodecInfo* pImageCodecInfo;
	 GetImageEncodersSize(&num, &size);
	 if(size == 0)
		  return -1;
	 pImageCodecInfo = (ImageCodecInfo*)new char[size];
	 if(pImageCodecInfo == NULL)
		  return -1;
	 GetImageEncoders(num, size, pImageCodecInfo);
	 for(UINT n=0; n<num; ++n) {
		  if( wcscmp(pImageCodecInfo[n].MimeType, format) == 0 ) {
			   *pClsid = pImageCodecInfo[n].Clsid;
			   delete pImageCodecInfo;
			   return n;
		  }
	 }
	 delete pImageCodecInfo;
	 return -1;
}

/**
 * メモリ上にGIFファイルを作る。
 * @param[inptr] pSrc 変換元RGBAの先頭のポインタ位置
 * @param[in] imgWidth 画像幅
 * @param[in] imgHeight 画像高さ
 * @param[out] pByte バイト数を書き込む先
 */
HGLOBAL makeGrayGif(float* pSrc,
	int imgWidth,
	int imgHeight,
	int* pByte,
	unsigned char* palette) {
	int retVal;
	HGLOBAL hMem;
	int x, y;
	int result;
	HGLOBAL hImg = NULL;
	HGLOBAL hRet = NULL;

	if (pSrc == NULL) {
		return NULL;
	}

	retVal = 0;
	do {
		// GDI+を使用

		GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

		CLSID encoderClsid;
		Status stat;

		int width = imgWidth;
		int height = imgHeight;

		// 出力画像用メモリ 4倍確保しておけばだいたい足りる
		hImg = GlobalAlloc(GPTR, width * height * 4);
		if (hImg == NULL) {
			retVal = -8;
			return NULL;
		}

		// 処理
		int srcPitch = width * 4;
		int dstPitch = (width + 3) / 4 * 4;
		unsigned char* pDst = (unsigned char*)hImg;
		for (y = 0; y < height; ++y) {
			auto pAddr = pSrc + y * srcPitch;
			pDst = ((unsigned char*)hImg) + y * dstPitch;
			for (x = 0; x < width; ++x) {
				int idx = 0;
				auto alpha = pAddr[3];
				if (alpha > 0.0f) {
					auto luma = pAddr[0] * 0.299f + pAddr[1] * 0.587f + pAddr[2] * 0.114f;
					int q = (int)(luma * 255.0f + 0.5f);
					idx = (q > 255) ? 255 : ((q < 1) ? 1 : q);
				}
				pDst[x] = idx;
				pAddr += 4;
			}
		}

		// 出力用
		Bitmap* image = new Bitmap(width,
			height,
			dstPitch,
			PixelFormat8bppIndexed,
			(BYTE*)hImg);
		do {
			stat = image->GetLastStatus();
			if (stat != Ok) {
				retVal = -10;
				return NULL;
			}

			stat = image->SetPalette((ColorPalette*)palette);

			// 保存方法を指定する
			result = GetEncoderClsid(L"image/gif", &encoderClsid);
			if (result < 0) {
				retVal = -11;
				return NULL;
			}

			// メモリ上
			hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
			if (hMem == NULL) {
				retVal = -12;
				return NULL;
			}
			LPSTREAM stream;
			HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE, &stream);
			// hMem が自動で消える
			if (FAILED(hr)) {
				retVal = -13;
				return NULL;
			}

			// 保存する
			stat = image->Save(stream, &encoderClsid);
			if (stat != Ok) {
				retVal = -14;
				return NULL;
			}

			LARGE_INTEGER move;
			ULARGE_INTEGER pos;
			move.QuadPart = 0LL;
			hr = stream->Seek(move, STREAM_SEEK_END, &pos);
			ULONG memByte = (DWORD)pos.QuadPart;

			hr = stream->Seek(move, STREAM_SEEK_SET, &pos);

			hRet = GlobalAlloc(GMEM_FIXED, memByte);
			if (!hRet) {
				retVal = -15;
				return NULL;
			}

			ULONG cbRead;
			hr = stream->Read(hRet, memByte, &cbRead);

			if (pByte) {
				*pByte = memByte;
			}

			stream->Release();
		} while (false);

		if (image) {
			delete image;
			image = NULL;
		}

		if (hImg) {
			GlobalFree(hImg);
			hImg = NULL;
		}

		GdiplusShutdown(gdiplusToken);
	} while (false);

	return hRet;
}
