/**
 * @file graygif.cpp
 */

//#define NOMINMAX
#include <windows.h>
#include <algorithm>

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
 * パレットにおける色を決定する。
 *
 * @param[in] p BGRA色の先頭のポインタ。但しBGR 3バイトしか使わない。
 * @return パレットインデックス
 */
int pickColor(unsigned char* p) {
	int r,g,b;

	r = (int)p[2];
	g = (int)p[1];
	b = (int)p[0];

	int ret = (r * 87 + g * 150 + b * 29 + 128) >> 8;
	ret = (ret > 255) ? 255 : ((ret < 1) ? 1 : ret);
	return ret;
}


typedef struct Vector3_ {
	int b;
	int g;
	int r;
	int a;
} Vector3;

/**
 * メモリ上に GIF ファイルを作る。
 */
HGLOBAL makeGif(unsigned char* pSrc,
			int imgWidth,
			int imgHeight,
			int* pByte,
			unsigned char*palette) {
	int retVal;
	HGLOBAL hMem;
	int width,height;
	int x,y;
	int result;
	unsigned char* pAddr;
	HGLOBAL hImg = NULL;
	HGLOBAL hRet = NULL;

	if (pSrc == nullptr) {
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

		width = imgWidth;
		height = imgHeight;

// 出力画像用メモリ
		hImg = GlobalAlloc(GPTR, width * height * 4);
		if (hImg == NULL) {
			retVal = -8;
			return NULL;
		}

// 処理
		int srcPitch = (width * 3 + 3) / 4 * 4;
		int dstPitch = (width + 3) / 4 * 4;
		unsigned char* pDst = (unsigned char*)hImg;
		for (y = 0; y < height; ++y) {
			pAddr = pSrc + (height - 1 - y) * srcPitch;
			pDst = ((unsigned char*)hImg) + y * dstPitch;
			for (x = 0; x < width; ++x) {
				int idx = pickColor(pAddr);

				pDst[x] = idx;
				pAddr += 3;
			}
		}

// 出力用
		Bitmap* image = new Bitmap(width,
			height,
			dstPitch,
			PixelFormat8bppIndexed,
			(BYTE *)hImg);
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
		} while(false);

		if (image) {
			delete image;
			image = NULL;
		}

		if (hImg) {
			GlobalFree(hImg);
			hImg = NULL;
		}

		GdiplusShutdown(gdiplusToken);
	}
	while(false);

	return hRet;
}


/**
 * メモリ上にGIFファイルを作る。
 * @param[inptr] pSrc 変換元BGRの先頭のポインタ位置
 * @param[in] bufWidth 左右両方を合算した幅
 * @param[in] imgHeight 画像高さ
 * @param[out] pByte バイト数を書き込む先
 */
HGLOBAL makeGif7(unsigned char* pSrc,
			int bufWidth,
			int imgHeight,
			int* pByte,
			unsigned char* palette) {
	int retVal;
	HGLOBAL hMem;
	int width,height;
	int x,y;
	int result;
	unsigned char* pAddr;
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

		width = bufWidth / 2;
		height = imgHeight;

// 出力画像用メモリ 4倍確保しておけばだいたい足りる
		hImg = GlobalAlloc(GPTR, width * height * 4);
		if (hImg == NULL) {
			retVal = -8;
			return NULL;
		}

// 処理
		int srcPitch = (bufWidth * 3 + 3) / 4 * 4;
		int dstPitch = (width + 3) / 4 * 4;
		unsigned char* pDst = (unsigned char*)hImg;
		for (y = 0; y < height; ++y) {
			pAddr = pSrc + (height - 1 - y) * srcPitch;
			unsigned char* p = pAddr + width * 3;
			pDst = ((unsigned char*)hImg) + y * dstPitch;
			for (x = 0; x < width; ++x) {
				int v = ((int)p[0]) + ((int)p[1]) + ((int)p[2]);
				int idx = (v != 0) ? pickColor(pAddr) : 0;

				pDst[x] = idx;

				pAddr += 3;
				p += 3;
			}
		}

// 出力用
		Bitmap* image = new Bitmap(width,
			height,
			dstPitch,
			PixelFormat8bppIndexed,
			(BYTE *)hImg);
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
		}
		while(false);

		if (image) {
			delete image;
			image = NULL;
		}

		if (hImg) {
			GlobalFree(hImg);
			hImg = NULL;
		}

		GdiplusShutdown(gdiplusToken);
	} while(false);

	return hRet;
}


