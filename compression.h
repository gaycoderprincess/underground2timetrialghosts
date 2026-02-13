class CwoeeIStream {
public:
	uint8_t* pData = nullptr;
	size_t nDataSize = 0;
	size_t nCursor = 0;

	CwoeeIStream(uint8_t* data, size_t dataSize) : pData(data), nDataSize(dataSize) {}
	~CwoeeIStream() {
		delete[] pData;
	}
	size_t read(char* out, size_t numBytes) {
		auto bytesRead = std::min(numBytes, nDataSize - nCursor);
		if (!bytesRead) return 0;
		memcpy(out, &pData[nCursor], bytesRead);
		nCursor += bytesRead;
		return bytesRead;
	}
};

class CwoeeOStream {
public:
	std::vector<char> aData;

	void write(const char* in, size_t numBytes) {
		for (int i = 0; i < numBytes; i++) {
			aData.push_back(in[i]);
		}
	}
};

std::string ReadStringFromFile(CwoeeIStream& file) {
	int len = 0;
	file.read((char*)&len, sizeof(len));
	if (len <= 0) return "";

	char* tmp = new char[len];
	file.read(tmp, len);
	std::string str = tmp;
	delete[] tmp;

	return str;
}
void WriteStringToFile(CwoeeOStream& file, const char* string) {
	int len  = lstrlen(string) + 1;
	file.write((char*)&len, sizeof(len));
	file.write(string, len);
}

CwoeeIStream* OpenEncryptedPB(const std::filesystem::path& filePath) {
	auto size = std::filesystem::file_size(filePath);
	auto inFile = std::ifstream(filePath, std::ios::in | std::ios::binary);
	if (!inFile.is_open()) return nullptr;

	auto data = new uint8_t[size];
	inFile.read((char*)data, size);

	uint8_t tmp = 0;
	for (size_t i = 0; i < size; i++) {
		data[i] ^= tmp;
		tmp += 0x10;
	}

	return new CwoeeIStream(data, size);
}

CwoeeIStream* OpenRawPB(const std::filesystem::path& filePath) {
	if (!std::filesystem::exists(filePath)) return nullptr;

	auto size = std::filesystem::file_size(filePath);
	auto inFile = std::ifstream(filePath, std::ios::in | std::ios::binary);
	if (!inFile.is_open()) return nullptr;

	auto data = new uint8_t[size];
	inFile.read((char*)data, size);
	return new CwoeeIStream(data, size);
}

bool EncryptPB(const std::filesystem::path& filePath) {
	auto size = std::filesystem::file_size(filePath);
	auto inFile = std::ifstream(filePath, std::ios::in | std::ios::binary);
	if (!inFile.is_open()) return false;

	auto data = new uint8_t[size];
	inFile.read((char*)data, size);

	auto encrypted = new uint8_t[size];

	uint8_t tmp = 0;
	for (size_t i = 0; i < size; i++) {
		encrypted[i] = data[i] ^ tmp;
		tmp += 0x10;
	}

	auto outFile = std::ofstream(filePath.string() + "2", std::ios::out | std::ios::binary);
	if (!outFile.is_open()) return false;
	outFile.write((char*)encrypted, size);
	return true;
}

bool WriteEncryptedPB(CwoeeOStream* file, const std::filesystem::path& filePath) {
	auto encrypted = new uint8_t[file->aData.size()];

	uint8_t tmp = 0;
	for (size_t i = 0; i < file->aData.size(); i++) {
		encrypted[i] = file->aData[i] ^ tmp;
		tmp += 0x10;
	}

	auto outFile = std::ofstream(filePath.string() + "2", std::ios::out | std::ios::binary);
	if (!outFile.is_open()) {
		delete[] encrypted;
		return false;
	}
	outFile.write((char*)encrypted, file->aData.size());
	delete[] encrypted;
	return true;
}

bool WriteRawPB(CwoeeOStream* file, const std::filesystem::path& filePath) {
	auto outFile = std::ofstream(filePath.string(), std::ios::out | std::ios::binary);
	if (!outFile.is_open()) return false;
	outFile.write((char*)&file->aData[0], file->aData.size());
	return true;
}