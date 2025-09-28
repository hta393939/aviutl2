
#include <string>
#include <vector>
#include <map>

typedef unsigned __int64 u64;
typedef unsigned short u16;
typedef unsigned char u8;

// half float 1.0 の2バイトLE表現
#define HALF_ONE_BIT (0x3c00)

#define ERR_TYPEMISMATCH (-5)

class LitePng {
public:
	LitePng() {

	}
	virtual ~LitePng() {
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="pstart"></param>
	/// <param name="dst"></param>
	/// <returns>null-term</returns>
	int _parseNullTerm(u8* pstart, std::string& dst) {
		for (int i = 0; i < 256; ++i) {
			auto val = pstart[i];
			if (val == 0x00) {
				dst.assign((char*)pstart);
				return (i + 1);
			}
		}
		return 0;
	}

	/// <summary>
	/// Blender のみ
	/// </summary>
	/// <param name="buf"></param>
	/// <param name="byteNum"></param>
	/// <returns></returns>
	int parse(unsigned char* buf, int byteNum) {
		int errCode = 0;
		int c = 0;

		{ // 8バイト
			if ((buf[0] != 'v') || (buf[1] != '/') || (buf[2] != '1') || (buf[3] != 0x01)) {
				return -1;
			}
			this->version = *((unsigned int*)(buf + 4));
			c += 8;
		}

		int channelCount = 0;
		for (int i = 0; i < 4; ++i) {
			auto val = this->channelElementOffset[i];
			if (val == 4) {
				if (i != 0) {
					return -14; // Vチャンネルは1つのみ対応
				}
				this->channelElementOffset[0] = 0;
				channelCount = 1;
				break;
			}
			if (val < 0) {
				if (channelCount < 3) {
					return -9; // チャンネルが埋まっていないエラー
				}
				break;
			}
			channelCount += 1;
		}
		this->channelCount = channelCount;
		/*
		{
			int offsetNum = this->dwHeight;
			this->dataOffset.resize(offsetNum);
			for (int j = 0; j < offsetNum; ++j) {
				u64 val = *((u64*)(buf + c));
				this->dataOffset[j] = val;
				c += 8;
			}
		}*/

		return channelCount;
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
		const int chNum = this->channelCount;
		const int num = 0;
		// 書き込み先
		const int byteNum = width * height * chNum * 2;
		// 読み取り
		const int elementSize = 4;
		const int reqByte = 8 + this->dwWidth * elementSize * chNum;

		ZeroMemory(buf, byteNum);
		if (!this->refBuffer || this->refBufferByte < reqByte) {
			return 0;
		}

		DWORD dwRead = 0;
		/*
		//DWORD data[8];
		//u16 u16s[16];
		//float f32s[8];
		//bool err = false;
		for (int i = 0; i < num; ++i) {
			int c = this->dataOffset[i];

			SetFilePointer(f, c, NULL, FILE_BEGIN);
			BOOL bresult = ReadFile(f, data, 8, &dwRead, NULL);
			if (!bresult || dwRead != 8) {
				err = true;
				break;
			}
			// Y成分
			int dy = data[0];
			int dataByteNum = data[1];

			for (int j = 0; j < 4; ++j) {
				int elmOffset = this->channelElementOffset[j];
				int elementSize = this->channelType[j] == CHTYPE_FLOAT ? 4 : 2;
				for (int x = 0; x < width; ++x) {
					if (elementSize == 2) {
						bresult = ReadFile(f, u16s, 2, &dwRead, NULL);
					}
					else {
						bresult = ReadFile(f, f32s, 4, &dwRead, NULL);
					}
					if (!bresult || (dwRead != elementSize)) {
						err = true;
						break;
					}

					int offset = ((x + width * dy) * 4 + elmOffset) * 2;
					u16* p = (u16*)(pdst + offset);
					*p = (elementSize == 2) ? u16s[0] : _ftob16(f32s[0]);
				}
				if (err) {
					break;
				}
			}
		}
		

		if (err) {
			return 0;
		} */
		return byteNum;
	}

	void setRefBuffer(unsigned char* buf, int byteNum) {
		this->refBuffer = buf;
		this->refBufferByte = byteNum;
	}

public:
	unsigned int dwWidth = 0;
	// 
	unsigned int dwHeight = 0;
	// 0: 
	int compression = 0;
	// 0: 
	int lineOrder = -1;

	int channelCount = 0;

	int pixelType[4] = { -1, -1, -1, -1 };
	//  0: R, 1: G, 2: B, 3: A
	int channelElementOffset[4] = { -1, -1, -1, -1 };

	int version = -1;
	// ファイル内位置
	int offsetTableTop = -1;

	// 自分では管理しないバッファへの参照ポインタ
	unsigned char* refBuffer = nullptr;
	int refBufferByte = 0;
};

