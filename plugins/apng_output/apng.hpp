/**
 * @file apngs.cpp
 */

#include <windows.h>
#include <gdiplus.h>

#pragma comment(lib,"gdiplus.lib")

using namespace Gdiplus;

typedef unsigned char u8;
typedef unsigned int u32;

#define IEND_CRC 0x4E426082

typedef struct _CHUNK {
	int offset;
	u32 bodyByte;
	u32 tag;
	u32 crc;
} CHUNK;


/// <summary>
/// 
/// </summary>
/// <see>https://qiita.com/mikecat_mixc/items/e5d236e3a3803ef7d3c5</see>
/// <param name="pSrc"></param>
/// <param name="byteNum"></param>
/// <returns></returns>
u32 calcCrc(const u8* pSrc, int byteNum) {
	u32 crc = 0xFFFFFFFF;   /* 0xFFFFFFFFで初期化する */
	u32 magic = 0xEDB88320; /* 反転したマジックナンバー */
	u32 table[256];                   /* 下位8ビットに対応する値を入れるテーブル */
	int i, j;

	/* テーブルを作成する */
	for (i = 0; i < 256; i++) {      /* 下位8ビットそれぞれについて計算する */
		u32 table_value = i;      /* 下位8ビットを添え字に、上位24ビットを0に初期化する */
		for (j = 0; j < 8; j++) {
			int b = (table_value & 1);   /* 上(反転したので下)から1があふれるかをチェックする */
			table_value >>= 1;           /* シフトする */
			if (b) table_value ^= magic; /* 1があふれたらマジックナンバーをXORする */
		}
		table[i] = table_value;        /* 計算した値をテーブルに格納する */
	}

	/* テーブルを用いてCRC32を計算する */
	for (i = 0; i < byteNum; i++) {
		crc = table[(crc ^ ((u32)pSrc[i])) & 0xff] ^ (crc >> 8); /* 1バイト投入して更新する */
	}
	return ~crc;
}

int writeu16be(u8* pDst, int inOffset, u32 v, int maxByte) {
	u32 val[4] = { (v >> 8) & 0xff, v & 0xff };
	u8* p = pDst + inOffset;
	p[0] = val[0];
	p[1] = val[1];
	return 2;
}

int writeu32be(u8* pDst, int inOffset, u32 v, int maxByte) {
	u32 val[4] = { (v >> 24) & 0xff, (v >> 16) & 0xff, (v>>8) & 0xff, v & 0xff };
	u8* p = pDst + inOffset;
	p[0] = val[0];
	p[1] = val[1];
	p[2] = val[2];
	p[3] = val[3];
	return 4;
}

u32 readu32be(const u8* pSrc, int offset) {
	u32 val[4] = { pSrc[0], pSrc[1], pSrc[2], pSrc[3] };
	return (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
}

/// <summary>
/// チャンクパース
/// </summary>
/// <param name="pSrc"></param>
/// <param name="byteNum"></param>
/// <param name="pChunks"></param>
/// <param name="maxNum"></param>
/// <returns>見つかったチャンク個数</returns>
int search(const u8* pSrc, int byteNum, CHUNK* pChunks, int maxNum) {
	// シグネチャ8バイト
	int offset = 8;
	int chunkNum = 0;
	CHUNK* p = pChunks;
	for (int i = 0; i < maxNum; ++i) {
		if (offset + 12 > byteNum) {
			break;
		}

		p->offset = offset;

		p->bodyByte = readu32be(pSrc, offset);
		offset += 4;

		p->tag = readu32be(pSrc, offset);
		offset += 4;

		offset += p->bodyByte;
		if (offset + 4 > byteNum) {
			break;
		}

		p->crc = readu32be(pSrc, offset);
		offset += 4;
		
		++chunkNum;
		++p;
	}
	return chunkNum;
}


int makeChunk(u8* pDst, int inOffset, int byteNum, u32 fourcc) {
	int offset = inOffset;
	u32 bodybyte = byteNum - 12;
	pDst[offset] = (bodybyte >> 24) & 0xff;
	pDst[offset + 1] = (bodybyte >> 16) & 0xff;
	pDst[offset + 2] = (bodybyte >> 8) & 0xff;
	pDst[offset + 3] = bodybyte & 0xff;

	offset += 4;
	pDst[offset] = (fourcc >> 24) & 0xff;
	pDst[offset + 1] = (fourcc >> 16) & 0xff;
	pDst[offset + 2] = (fourcc >> 8) & 0xff;
	pDst[offset + 3] = fourcc & 0xff;

	offset = byteNum - 4;
	auto crc = calcCrc(pDst + 4, bodybyte + 4);
	pDst[offset] = (crc >> 24) & 0xff;
	pDst[offset + 1] = (crc >> 16) & 0xff;
	pDst[offset + 2] = (crc >> 8) & 0xff;
	pDst[offset + 3] = crc & 0xff;
	return 1;
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


HGLOBAL makeMemoryPng(const float* pSrc,
	int imgWidth,
	int imgHeight,
	int* pByte,
	int isStraighten) {
	if (pSrc == nullptr) {
		return NULL;
	}

	int retVal = 0;
	HGLOBAL hRet = NULL;
	HGLOBAL hMem = NULL;
	do {
		// GDI+を使用

		GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

		CLSID encoderClsid;
		Status stat;

		// 処理ピクセル幅
		int width = imgWidth;
		int height = imgHeight;
		const int pixNum = width * height;

		// 出力画像用メモリ
		HGLOBAL hImg = GlobalAlloc(GPTR, pixNum * 4);
		if (hImg == NULL) {
			return NULL;
		}

		int* p32 = (int*)hImg;
		auto pAddr = pSrc;
		for (int i = 0; i < pixNum; ++i) {
			float r = pAddr[0];
			float g = pAddr[1];
			float b = pAddr[2];
			float a = pAddr[3];
			float k = (isStraighten && a != 0.0f) ? (1.0f / a) : 1.0f;
			r *= k;
			g *= k;
			b *= k;
			auto ir = (unsigned int)(r + 0.5f);
			auto ig = (unsigned int)(g + 0.5f);
			auto ib = (unsigned int)(b + 0.5f);
			auto ia = (unsigned int)(a + 0.5f);
			ir = (ir >= 255) ? 255 : ir;
			ig = (ig >= 255) ? 255 : ig;
			ib = (ib >= 255) ? 255 : ib;
			ia = (ia >= 255) ? 255 : ia;
			*p32 = (ia << 24) | (ir << 16) | (ig << 8) | ib;

			pAddr += 4;
			++p32;
		}

		// 出力用
		Bitmap* image = new Bitmap(width, height,
			width * 4, PixelFormat32bppARGB, (BYTE*)hImg);
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

