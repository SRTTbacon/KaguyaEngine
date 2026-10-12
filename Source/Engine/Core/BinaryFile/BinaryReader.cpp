#include "BinaryReader.h"
#include "..\CoreMinimal.h"

#include <cstdio>
#include <algorithm>
#include <DirectXMath.h>
#include <DirectXMathMatrix.inl>
#include <vector>
#include <Windows.h>
#include <string>

using namespace DirectX;

namespace KaguyaEngine::File
{
	BinaryReader::BinaryReader(const std::string& filePath, uint32 maxReadKB)
		: bMemory(false)
		, readBufferSize(1024 * maxReadKB)
	{
		hFile = CreateFileA(filePath.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			return;
		}

		buffer.resize(readBufferSize);
		FillBuffer();
	}

	BinaryReader::BinaryReader(std::vector<char>& buffer, uint32 maxReadKB)
		: bMemory(true)
		, hFile(nullptr)
		, readBufferSize(1024 * maxReadKB)
	{
		this->buffer = buffer;
		bufferSize = buffer.size();
		bufferIndex = 0;
	}

	BinaryReader::~BinaryReader()
	{
		if (hFile == nullptr || hFile == INVALID_HANDLE_VALUE) {
			return;
		}
		Close();
	}

	DirectX::XMMATRIX BinaryReader::ReadMatrix()
	{
		DirectX::XMMATRIX matrix = DirectX::XMMatrixIdentity();
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				matrix.r[i].m128_f32[j] = ReadFloat();
			}
		}

		return matrix;
	}

	DirectX::XMFLOAT2 BinaryReader::ReadFloat2()
	{
		return Read<XMFLOAT2>();
	}

	DirectX::XMFLOAT3 BinaryReader::ReadFloat3()
	{
		return Read<XMFLOAT3>();
	}

	DirectX::XMFLOAT4 BinaryReader::ReadFloat4()
	{
		return Read<XMFLOAT4>();
	}

	//ファイルから8バイトだけ読み取り、double型に入れる
	double BinaryReader::ReadDouble()
	{
		return Read<double>();
	}
	//ファイルから4バイトだけ読み取り、float型に入れる
	float BinaryReader::ReadFloat()
	{
		return Read<float>();
	}
	//ファイルから4バイトだけ読み取り、int型に入れる
	int BinaryReader::ReadInt32()
	{
		return Read<int>();
	}
	//ファイルから4バイトだけ読み取り、unsigned int型に入れる
	uint32 BinaryReader::ReadUInt32()
	{
		return Read<uint32>();
	}
	//ファイルから2バイトだけ読み取り、unsigned short型に入れる
	uint16 BinaryReader::ReadUInt16()
	{
		return Read<uint16>();
	}
	//ファイルから2バイトだけ読み取り、short型に入れる
	short BinaryReader::ReadInt16()
	{
		return Read<short>();
	}
	//ファイルから1バイトだけ読み取り、unsigned char型に入れる
	unsigned char BinaryReader::ReadByte()
	{
		if (bufferIndex >= bufferSize) {
			if (!FillBuffer()) {
				return 0x00;
			}
		}
		return buffer[bufferIndex++];
	}
	char BinaryReader::ReadSByte()
	{
		if (bufferIndex >= bufferSize) {
			if (!FillBuffer()) {
				return 0x00;
			}
		}
		return (char)buffer[bufferIndex++];
	}
	bool BinaryReader::ReadBoolean()
	{
		if (bufferIndex >= bufferSize) {
			if (!FillBuffer()) {
				return false;
			}
		}
		return buffer[bufferIndex++] != 0;
	}
	//ファイルからreadsizeだけ読み取り、char*型に入れる
	std::vector<char> BinaryReader::ReadBytes(uint32 readSize)
	{
		std::vector<char> result;
		result.reserve(readSize);

		size_t totalRead = 0;

		while (totalRead < readSize) {
			//現在のバッファに残りがあるかチェック
			size_t available = bufferSize - bufferIndex;

			if (available == 0) {
				if (bMemory || !FillBuffer()) {
					break;  //これ以上読めない
				}
				available = bufferSize - bufferIndex;
			}

			size_t want = readSize - totalRead;
			size_t canRead = (std::min)(available, want);

			result.insert(result.end(),
				buffer.begin() + bufferIndex,
				buffer.begin() + bufferIndex + canRead);

			bufferIndex += canRead;
			totalRead += canRead;
		}

		return result;
	}
	std::string BinaryReader::ReadString()
	{
		return ReadString(ReadByte());
	}
	std::string BinaryReader::ReadString(uint32 readSize)
	{
		std::vector<char> readChar = ReadBytes(readSize);
		std::string text(readChar.begin(), readChar.end());
		return text;
	}
	//ファイルからreadsizeだけ読み取り、unsigned char*型に入れる
	std::vector<uint8> BinaryReader::ReadUBytes(uint32 readSize)
	{
		std::vector<uint8> result;
		result.reserve(readSize);  // パフォーマンスのため推奨

		size_t totalRead = 0;

		while (totalRead < readSize)
		{
			size_t available = bufferSize - bufferIndex;

			if (available == 0)
			{
				if (bMemory || !FillBuffer())
				{
					break;  // これ以上読めない
				}
				available = bufferSize - bufferIndex;
			}

			size_t want = readSize - totalRead;
			size_t canRead = (std::min)(available, want);

			// ★ここが一番大きな変更点
			result.insert(result.end(),
				buffer.begin() + bufferIndex,
				buffer.begin() + bufferIndex + canRead);

			bufferIndex += canRead;
			totalRead += canRead;
		}

		return result;
	}
	//現在の位置からseekLengthバイトだけ進める
	void BinaryReader::Seek(uint32 seekLength, SeekType seekType)
	{
		if (hFile == INVALID_HANDLE_VALUE && !bMemory) {
			return;
		}

		if (!SetFilePointer(hFile, seekLength, nullptr, seekType)) {
			printf("シーク先がファイルの範囲を超えています。\n");
			CloseHandle(hFile);
			return;
		}

		FillBuffer();
	}
	//バイナリファイルを閉じる
	void BinaryReader::Close()
	{
		if (hFile == INVALID_HANDLE_VALUE || bMemory) {
			return;
		}
		CloseHandle(hFile);
		hFile = nullptr;
	}

	bool BinaryReader::FillBuffer()
	{
		if (hFile == INVALID_HANDLE_VALUE || bMemory) {
			return false;
		}

		DWORD bytesRead = 0;
		bool success = ReadFile(hFile, buffer.data(), readBufferSize, &bytesRead, NULL);
		if (!success || bytesRead == 0) {
			printf("ErrorReadFile\n");
			return false; //ファイルの終端
		}

		bufferIndex = 0;
		bufferSize = bytesRead;
		return true;
	}
}
