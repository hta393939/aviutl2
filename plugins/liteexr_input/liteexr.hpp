
#include <string>
#include <vector>
#include <map>

typedef unsigned __int64 u64;
typedef unsigned short u16;
typedef unsigned char u8;

// half float 1.0 の2バイトLE表現
#define HALF_ONE_BIT (0x3c00)

#define ERR_TYPEMISMATCH (-5)

enum {
	PIXELTYPE_UINT = 0,
	PIXELTYPE_HALF = 1,
	PIXELTYPE_FLOAT = 2,
};

enum {
	NO_COMPRESSION = 0,
	RLE_COMPRESSION,
	ZIPS_COMPRESSION,
	ZIP_COMPRESSION,
};

enum {
	INCREASING_Y = 0,
	DECREASING_Y = 1,
	RANDOM_Y = 2,
};

struct BOX2I {
	int left;
	int top;
	// 
	int right;
	// 
	int bottom;
};

class LiteExr {
public:
	LiteExr() {

	}
	virtual ~LiteExr() {
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
		{ // dict
			while (c < byteNum) {
				std::string name;
				std::string valtype;
				auto byte1 = this->_parseNullTerm(buf + c, name);
				if (byte1 <= 0) {
					return -2; // 
				}
				c += byte1;
				if (byte1 == 1) {
					this->offsetTableTop = c;
					break; // dict の終了
				}

				auto byte2 = this->_parseNullTerm(buf + c, valtype);
				if (byte2 <= 0) {
					return -3; // 
				}
				c += byte2;
				// データのバイト数
				int dataByte = *((int*)(buf + c));
				c += 4;

				if (name == "compression") {
					if (valtype != "compression") {
						return ERR_TYPEMISMATCH;
					}
					u8 comp = *((u8*)(buf + c));
					this->compression = comp;
				}
				else if (name == "dataWindow") {
					if (valtype != "box2i") {
						return ERR_TYPEMISMATCH;
					}
					auto p = (int*)(buf + c);
					this->dataWindow.left = p[0];
					this->dataWindow.top = p[1];
					this->dataWindow.right = p[2];
					this->dataWindow.bottom = p[3];
				}
				else if (name == "displayWindow") {
					if (valtype != "box2i") {
						return ERR_TYPEMISMATCH;
					}
					// ignore
					auto p = (int*)(buf + c);
					this->displayWindow.left = p[0];
					this->displayWindow.top = p[1];
					this->displayWindow.right = p[2];
					this->displayWindow.bottom = p[3];
				}
				else if (name == "lineOrder") {
					if (valtype != "lineOrder") {
						return ERR_TYPEMISMATCH;
					}
					// ignore
					this->lineOrder = *((u8*)(buf + c));
				}
				else if (name == "pixelAspectRatio") {
					if (valtype != "float") {
						return ERR_TYPEMISMATCH;
					}
					// ignore
				}
				else if (name == "screenWindowCenter") {
					if (valtype != "v2f") {
						return ERR_TYPEMISMATCH;
					}
					// ignore
				}
				else if (name == "screenWindowWidth") {
					if (valtype != "float") {
						return ERR_TYPEMISMATCH;
					}
					// ignore
				}
				else if (name == "channels") {
					if (valtype != "chlist") {
						return ERR_TYPEMISMATCH;
					}

					// dict
					int order = 0;
					while (c < byteNum) {
						std::string subname;
						auto byte5 = this->_parseNullTerm(buf + c, subname);
						if (byte5 <= 0) {
							errCode = -11;
							break; // 
						}
						c += byte5;
						if (byte5 == 1) {
							break; // dict 終了
						}
						int* p32 = (int*)(buf + c);
						// 2: float, 1: half
						int pixelType = p32[0];
						//p32[1]; // 0 pLinear and three reserved
						//p32[2]; // 1 xSampling
						//p32[3]; // 1 ySampling			
						c += 16;

						int index = -1;
						if (subname == "A") {
							index = 3;
						}
						else if (subname == "R") {
							index = 0;
						}
						else if (subname == "G") {
							index = 1;
						}
						else if (subname == "B") {
							index = 2;
						}
						else if (subname == "V") {
							index = 4;
						}
						// 他、ViewLayer.Combined.A など
						if (index >= 0 && index <= 4) {
							this->pixelType[order] = pixelType;
							this->channelElementOffset[order] = index;
						} else {
							errCode = -12; // 知らないチャンネル名
							break;
						}
						order += 1;
					}

					if (errCode < 0) {
						return errCode;
					}
					dataByte = 0;
				}

				c += dataByte;
			}

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

		this->dwWidth = this->dataWindow.right - this->dataWindow.left + 1;
		// bottom は内
		this->dwHeight = this->dataWindow.bottom - this->dataWindow.top + 1;

		if (this->compression != NO_COMPRESSION) {
			return -13;
		}

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
	/// offsetTable をファイルからベクターに読み取る
	/// </summary>
	/// <param name="buf"></param>
	/// <param name="byteNum"></param>
	/// <returns></returns>
	int loadOffsetTable(HANDLE f) {
		if (f == INVALID_HANDLE_VALUE) {
			return -1;
		}
		SetFilePointer(f, this->offsetTableTop, NULL, FILE_BEGIN);
		this->dataOffset.resize(this->dwHeight);
		int reqByte = this->dwHeight * 8;
		DWORD dwRead = 0;
		auto bresult = ReadFile(f, this->dataOffset.data(), reqByte, &dwRead, NULL);
		if (!bresult || reqByte != dwRead) {
			return -2;
		}
		return this->dwHeight;
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
		const int num = this->dataOffset.size();
		// 書き込み先
		const int byteNum = width * height * chNum * 2;
		// 読み取り
		const int elementSize = this->pixelType[0] == PIXELTYPE_HALF ? 2 : 4;
		const int reqByte = 8 + this->dwWidth * elementSize * chNum;

		ZeroMemory(buf, byteNum);
		if (!this->refBuffer || this->refBufferByte < reqByte) {
			return 0;
		}

		DWORD dwRead = 0;
		auto p32 = (DWORD*)this->refBuffer;
		for (int i = 0; i < num; ++i) {
			int c = this->dataOffset[i];
			SetFilePointer(f, c, NULL, FILE_BEGIN);
			BOOL bresult = ReadFile(f, this->refBuffer, reqByte, &dwRead, NULL);
			if (!bresult || (dwRead != reqByte)) {
				return 0; // ファイル不足エラー
			}
			int dy = *p32;
			if (dy >= height) {
				return 0; // 範囲オーバーエラー
			}
			u16* psrc16 = (u16*)(this->refBuffer + 8);
			float* psrcf = (float*)(this->refBuffer + 8);
			if (chNum >= 3) {
				for (int j = 0; j < chNum; ++j) {
					int elmOffset = this->channelElementOffset[j];
					int dstOffset = width * dy * 4 + elmOffset;
					unsigned short* pdst = ((unsigned short*)buf) + dstOffset;
					if (elementSize == 2) {
						for (int x = 0; x < width; ++x) {
							*pdst = *psrc16;
							psrc16++;
							pdst += 4;
						}
					}
					else {
						for (int x = 0; x < width; ++x) {
							*pdst = _ftob16(*psrcf);
							psrcf++;
							pdst += 4;
						}
					}
				}
				if (chNum == 3) {
					int dstOffset = width * dy * 4 + 3;
					unsigned short* pdst = ((unsigned short*)buf) + dstOffset;
					for (int x = 0; x < width; ++x) {
						*pdst = HALF_ONE_BIT;
						pdst += 4;
					}
				}
			}
			else {
				// グレースケール
				int dstOffset = width * dy * 4;
				unsigned short* pdst = ((unsigned short*)buf) + dstOffset;
				if (elementSize == 2) {
					for (int x = 0; x < width; ++x) {
						auto src = *psrc16;
						pdst[0] = src;
						pdst[1] = src;
						pdst[2] = src;
						pdst[3] = HALF_ONE_BIT;
						psrc16++;
						pdst += 4;
					}
				}
				else {
					for (int x = 0; x < width; ++x) {
						auto src = _ftob16(*psrcf);
						pdst[0] = src;
						pdst[1] = src;
						pdst[2] = src;
						pdst[3] = HALF_ONE_BIT;
						psrcf++;
						pdst += 4;
					}
				}

			}

		}

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
	BOX2I dataWindow = { 0,0,0,0 };
	// 使用しない
	BOX2I displayWindow = { 0,0,0,0 };
	std::vector<u64> dataOffset;

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

