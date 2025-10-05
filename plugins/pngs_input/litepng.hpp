
#include <string>
//#include <vector>
//#include <map>

#include <gdiplus.h>
using namespace Gdiplus;

typedef unsigned __int64 u64;
typedef unsigned char u8;

// half float 1.0 の2バイトLE表現
#define HALF_ONE_BIT (0x3c00)

#define COLORTYPE_PAL (1)
#define COLORTYPE_COL (2)
#define COLORTYPE_ALPHA (4)

class LitePng {
public:
	LitePng() {

	}
	virtual ~LitePng() {
	}

	int r32(unsigned char* p) {
		return ((int)p[0]) << 24 |
			((int)p[1]) << 16 |
			((int)p[2]) << 8 |
			(int)p[3];
	}

	/// <summary>
	/// 21+バイト以上
	/// </summary>
	/// <param name="buf"></param>
	/// <param name="byteNum"></param>
	/// <returns></returns>
	int parse(unsigned char* buf, int byteNum) {
		int errCode = 0;
		int c = 0;

		{ // 8バイト
			if ((buf[0] != 0x89) || (buf[1] != 0x50) || (buf[2] != 0x4e) || (buf[3] != 0x47)
				|| buf[4] != 0x0d || buf[5] != 0x0a || buf[6] != 0x1a || buf[7] != 0x0a) {
				return -1;
			}
			c += 8;

			c += 4; // 13
			if (*((DWORD*)(buf + c)) != MAKEFOURCC('I', 'H', 'D', 'R')) {
				return -2;
			}
			c += 4;

			this->dwWidth = this->r32(buf + c);
			c += 4;
			this->dwHeight = this->r32(buf + c);
			c += 4;

			this->depth = *((u8*)(buf + c)); // 8 など、1,2,4,8,16
			c += 1;
			this->colorType = *((u8*)(buf + c)); // 6 など、1: pal, 2: color, 4: a
			c += 1;
		}
		return c;
	}

	/// <summary>
	/// 参照メモリのセットが必要。
	/// </summary>
	/// <param name="buf"></param>
	/// <param name="byteNum"></param>
	/// <param name="buf"></param>
	/// <returns></returns>
	int getData(HANDLE f, unsigned char* buf) {
		const int width = this->dwWidth;
		const int height = this->dwHeight;
		// 書き込み先
		const int byteNum = width * height * 4;

		ZeroMemory(buf, byteNum);

		GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken;
		do {
			GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
			ImageCodecInfo* pImageCodecInfo;
			UINT  num;        // number of image decoders
			UINT  size;       // size, in bytes, of the image decoder array

			GetImageDecodersSize(&num, &size);
			// Create a buffer large enough to hold the array of ImageCodecInfo
			// objects that will be returned by GetImageDecoders.
			pImageCodecInfo = (ImageCodecInfo*)(malloc(size));

			// GetImageDecoders creates an array of ImageCodecInfo objects
			// and copies that array into a previously allocated buffer. 
			// The third argument, imageCodecInfo, is a pointer to that buffer. 
			GetImageDecoders(num, size, pImageCodecInfo);

			for (UINT j = 0; j < num; ++j)
			{
				wprintf(L"%s\n", pImageCodecInfo[j].MimeType);
			}
			// 実装していない

			free(pImageCodecInfo);
		} while (false);
		GdiplusShutdown(gdiplusToken);

		return byteNum;
	}

public:
	unsigned int dwWidth = 0;
	// 
	unsigned int dwHeight = 0;

	int depth = 0;
	int colorType = 0;
};

