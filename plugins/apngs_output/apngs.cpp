/**
 * @file apngs.cpp
 */

#include <windows.h>
#include <gdiplus.h>

#pragma comment(lib,"gdiplus.lib")

using namespace Gdiplus;

/// <summary>文字列の変換</summary>
/// <param name="src">[in]変換元マルチバイト文字列</param>
/// <param name="dst">[out]変換後ワイド文字列</param>
int conv(const char* src,WCHAR* dst) {
	if (src == NULL || dst == NULL) {
		return -1;
	}

	int result = MultiByteToWideChar(932,
		0,src,-1,
		dst,4096-16);
	return result;
}

/// <summary>MSDNのHelper Function.</summary>
/// <param name="format">[in]L"image/png" などを指定する。</param>
/// <param name="pClsid">[out]ここに出力される。</param>
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	 UINT  num = 0;
	 UINT  size = 0;
	 ImageCodecInfo* pImageCodecInfo;
	 char* buf;
	 GetImageEncodersSize(&num, &size);
	 if(size == 0) {
		  return -1;
	 }
	 buf = new char[size];
	 if (buf == NULL) {
		 return -1;
	 }
	 pImageCodecInfo = (ImageCodecInfo*)buf;

	 GetImageEncoders(num, size, pImageCodecInfo);
	 for(UINT n=0; n<num; ++n) {
		  if( wcscmp(pImageCodecInfo[n].MimeType, format) == 0 ) {
			   *pClsid = pImageCodecInfo[n].Clsid;
			   delete[] buf;
			   return n;
		  }
	 }
	 delete [] buf;
	 return -1;
}

/// <summary>メモリ上に PNG ファイルを作る</summary>
int makePng(unsigned char* pSrc,
			int imgWidth,
			int imgHeight,
			wchar_t* name) {
	int retVal;
	int width,height;
	int x,y;
	int result;
	int quad;
	int* p32;
	unsigned char* pAdr;
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
		int dstPitch = width * 4;
		quad = 0xFF000000;
		p32 = (int *)hImg;
		unsigned char* pDst = (unsigned char*)hImg;
		for (y = 0; y < height; ++y) {
			pAdr = pSrc + (height - 1 - y) * srcPitch;
			pDst = ((unsigned char*)hImg) + y * dstPitch;
			for (x = 0; x < width; ++x) {
				CopyMemory(&quad, pAdr, 3);
				*p32 = quad;

				pAdr += 3;
				++p32;
			}
		}

// 出力用
		Bitmap* image = new Bitmap(width, height, dstPitch, PixelFormat32bppARGB, (BYTE *)hImg);
		do {
			stat = image->GetLastStatus();
			if (stat != Ok)
			{
				retVal = -10;
				return NULL;
			}

			// 保存方法を指定する
			result = GetEncoderClsid(L"image/png", &encoderClsid);
			if (result < 0) {
				retVal = -11;
				return NULL;
			}

			// 保存する
			stat = image->Save(name, &encoderClsid);
			if (stat != Ok) {
				retVal = -14;
				return NULL;
			}
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
	} while(false);

	return 1;
}

/// <summary>アルファチャンネル有り PNG ファイルを作る</summary>
/// <param name="bufWidth">バッファのピクセル幅</param>
/// <param name="imgHeight">高さピクセル数</param>
int makePng7(const unsigned char* pSrc,
			int bufWidth,
			int imgHeight,
			const wchar_t* name) {
	int x,y;
	const unsigned char* pAddr;

	if (pSrc == nullptr || name == nullptr) {
		return -1;
	}

	int retVal = 0;
	do {
// GDI+を使用

		GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

		CLSID encoderClsid;
		Status stat;

		// 処理ピクセル幅
		int width = bufWidth / 2;
		int height = imgHeight;

// 出力画像用メモリ
		HGLOBAL hImg = GlobalAlloc(GPTR, width * height * 4);
		if (hImg == NULL) {
			return -2;
		}

// 処理
		int srcPitch = (bufWidth * 3 + 3) / 4 * 4;
		int toAlpha = width * 3 + 0;
		const unsigned char* pAlpha;
		int* p32 = (int *)hImg;
		for (y = 0; y < height; ++y) {
			pAddr = pSrc + (height - 1 - y) * srcPitch;
			pAlpha = pAddr + toAlpha;
			for (x = 0; x < width; ++x) {
				// 青成分(B)をそのまま使っている
//				int quad = ((int)(*pAlpha)) << 24;
				// 全部計算する
				int quad = ((29*((int)pAlpha[0]) + 150*((int)pAlpha[1]) + 77*((int)pAlpha[2])) >> 8) << 24;

				CopyMemory(&quad, pAddr, 3);
				*p32 = quad;

				pAddr += 3;
				pAlpha += 3;
				++p32;
			}
		}

// 出力用
		Bitmap* image = new Bitmap(width, height,
			width*4, PixelFormat32bppARGB, (BYTE *)hImg);
		do {
			stat = image->GetLastStatus();
			if (stat != Ok) {
				retVal = -10;
				return NULL;
			}

// 保存方法を指定する
			int result = GetEncoderClsid(L"image/png", &encoderClsid);
			if (result < 0) {
				retVal = -11;
				return NULL;
			}

			// 保存する
			stat = image->Save(name, &encoderClsid);
			if (stat != Ok) {
				retVal = -14;
				return NULL;
			}
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
	} while(false);

	return 1;
}
