
#include <string>
#include <vector>
#include <map>

typedef unsigned long long u64;
typedef unsigned short u16;
typedef unsigned char u8;

#define CHTYPE_HALF (1)
#define CHTYPE_FLOAT (2)

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
	/// Blender 
	/// </summary>
	/// <param name="buf"></param>
	/// <param name="byteNum"></param>
	/// <returns></returns>
	int parse(unsigned char* buf, int byteNum) {
		bool err = false;
		int c = 0;

		{ // 8�o�C�g
			if ((buf[0] != 'v') || (buf[1] != '/') || (buf[2] != '1')) {
				return -1;
			}

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
					break; // 
				}

				auto byte2 = this->_parseNullTerm(buf + c, valtype);
				if (byte2 <= 0) {
					return -3; // 
				}
				c += byte2;

				int byte3 = *((int*)(buf + c)); // 
				c += 4;

				if (name == "compression") {
					if (valtype != "compression") {
						return -4;
					}
					u8 comp = *((u8*)(buf + c));
					this->compression = comp;
				}
				else if (name == "dataWindow") {
					if (valtype != "box2i") {
						return -5;
					}
					auto p = (int*)(buf + c);
					this->dataWindow.left = p[0];
					this->dataWindow.top = p[1];
					this->dataWindow.right = p[2];
					this->dataWindow.bottom = p[3];
				}
				else if (name == "displayWindow") {
					// ignore
				}
				else if (name == "lineOrder") {
					if (valtype != "lineOrder") {
						return -6;
					}
					// ignore
					this->lineOrder = *((int*)(buf + c));
				}
				else if (name == "pixelAspectRatio") {
					// ignore
				}
				else if (name == "screenWindowCenter") {
					// ignore
				}
				else if (name == "screenWindowWidth") {
					// ignore
				}
				else if (name == "channels") {
					if (valtype != "chlist") {
						return -7;
					}

					// dict
					int order = 0;
					while (c < byteNum) {
						std::string subname;
						auto byte5 = this->_parseNullTerm(buf + c, subname);
						if (byte5 <= 0) {
							err = true;
							break; // 
						}
						c += byte5;
						if (subname == "") {
							break; // 
						}

						int byte6 = *((int*)(buf + c));
						if (byte6 != 16) {
							err = true;
							break;
						}
						int* p32 = (int*)(buf + c);
						// 2: float, 1: half
						int dataType = p32[0];
						
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
						if (index < 0) {
							err = true; // 
							break;
						}
						this->channelType[order] = dataType;
						this->channelElementOffset[order] = index;

						order += 1;
					}

					if (err) {
						return -8;
					}
				}

				c += byte3;
			}

		}
		this->dwWidth = this->dataWindow.right - this->dataWindow.left + 1;
		// bottom 
		this->dwHeight = this->dataWindow.bottom - this->dataWindow.top + 1;
		{ // 
			int offsetNum = this->dwHeight;
			for (int j = 0; j < offsetNum; ++j) {
				auto p = (u64*)(buf + c);
				this->dataOffset.push_back(*p);
				c += 8;
			}
		}

		return 1;
	}

	/// <summary>
	/// 
	/// </summary>
	/// <param name="buf"></param>
	/// <param name="byteNum"></param>
	/// <param name="pdst"></param>
	/// <returns></returns>
	int getData(HANDLE f, unsigned char* pdst) {
		int width = this->dwWidth;
		int height = this->dwHeight;
		auto num = this->dataOffset.size();
		DWORD dwRead = 0;
		DWORD data[8];
		u16 u16s[16];
		float f32s[8];
		bool err = false;
		for (int i = 0; i < num; ++i) {
			int c = this->dataOffset[i];

			SetFilePointer(f, c, NULL, FILE_BEGIN);
			BOOL bresult = ReadFile(f, data, 8, &dwRead, NULL);
			if (!bresult || dwRead != 8) {
				break;
			}
			// 
			int dy = data[0];
			c += 4;
			int dataByteNum = data[1];
			c += 4;

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
					if (!bresult || dwRead == 2) {
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
		}

		return width * height * 4 * 2;
	}

public:
	// 
	std::map<std::string, std::string> props;
	BOX2I dataWindow = { 0,0,0,0 };
	BOX2I displayWindow = { 0,0,0,0 };
	std::vector<u64> dataOffset;

	unsigned int dwWidth = 0;
	// 
	unsigned int dwHeight = 0;
	// 0: 
	int compression = 0;
	// 0: 
	int lineOrder = 0;

	int channelType[4] = { CHTYPE_HALF, CHTYPE_HALF, CHTYPE_HALF, CHTYPE_HALF };
	//  0: R, 1: G, 2: B, 3: A
	int channelElementOffset[4] = { 3, 2, 1, 0 };
};

