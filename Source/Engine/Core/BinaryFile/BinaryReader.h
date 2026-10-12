#pragma once

#include "..\\CoreMinimal.h"

#include <DirectXMath.h>
#include <string>
#include <vector>
#include <string.h>
#include <type_traits>

namespace KaguyaEngine::File
{
	class BinaryReader
	{
	public:
		enum SeekType
		{
			Begin,
			Current,
			End
		};

		//コンストラクタ
		//引数 : ファイルパス
		BinaryReader(const std::string& filePath, uint32 maxReadKB = 1024 * 10);
		BinaryReader(std::vector<char>& buffer, uint32 maxReadKB = 1024 * 10);

		~BinaryReader();

		//ファイルから128バイト読み取り、XMMATRIX型に入れる
		DirectX::XMMATRIX ReadMatrix();

		DirectX::XMFLOAT2 ReadFloat2();

		//ファイルから12バイト読み取り、XMFLOAT3型に入れる
		DirectX::XMFLOAT3 ReadFloat3();

		//ファイルから16バイト読み取り、XMFLOAT4型に入れる
		DirectX::XMFLOAT4 ReadFloat4();

		//ファイルから8バイト読み取り、double型に入れる
		double ReadDouble();

		//ファイルから4バイト読み取り、float型に入れる
		float ReadFloat();

		//ファイルから4バイト読み取り、int型に入れる
		int ReadInt32();

		//ファイルから4バイト読み取り、unsigned int型に入れる
		uint32 ReadUInt32();

		//ファイルから2バイト読み取り、unsigned short型に入れる
		uint16 ReadUInt16();

		//ファイルから2バイト読み取り、short型に入れる
		short ReadInt16();

		//ファイルから1バイト読み取り、unsigned char型に入れる
		uint8 ReadByte();
		//ファイルから1バイト読み取り、char型に入れる
		char ReadSByte();

		//ファイルから1バイト読み取り、bool型に入れる
		bool ReadBoolean();

		//ファイルからreadsizeバイト読み取り、char*型に入れる
		//引数 : 読み取るサイズ (バイト単位)
		//注釈 : 戻り値は必ずdelete[]を行う
		std::vector<char> ReadBytes(uint32 readSize);

		//ファイルから1バイト読み取り、その数値ぶん文字列として読み取る
		std::string ReadString();
		std::string ReadString(uint32 readSize);

		//ファイルからreadsize読み取り、unsigned char*型に入れる
		//引数 : 読み取るサイズ (バイト単位)
		//注釈 : 戻り値は必ずdelete[]を行う
		std::vector<uint8> ReadUBytes(uint32 readSize);

		//指定した位置からseekLengthバイト進める
		//引数 ; スキップするサイズ (バイト単位)
		void Seek(uint32 seekLength, SeekType seekType);

		//バイナリファイルを閉じる
		void Close();

	public:
		inline bool GetIsOpen() const { return (hFile != nullptr && hFile != INVALID_HANDLE_VALUE); }

	private:
		HANDLE hFile;

		std::vector<char> buffer;
		size_t bufferIndex = 0;
		size_t bufferSize = 0;
		bool bMemory;
		uint32 readBufferSize = 1024 * 1024 * 10; //10MBずつ読み取る

		//1度に10MBずつファイルから読み取り、メモリに保存
		bool FillBuffer();

		template <typename T>
		T Read();
	};

	template<typename T>
	inline T BinaryReader::Read()
	{
		const size_t typeSize = sizeof(T);
		size_t bytesRemaining = bufferSize - bufferIndex;

		if (bytesRemaining < typeSize) {
			if (bMemory) {
				if constexpr (std::is_arithmetic<T>::value) {
					return T(0); //算術型の場合はゼロを返す
				}
				else {
					return T();  //その他の型の場合はデフォルト値を返す
				}
			}

			//バッファに残っているデータを一時保存
			uint8 tempBuffer[typeSize] = { 0 };

			if (bytesRemaining > 0) {
				std::memcpy(tempBuffer, &buffer[bufferIndex], bytesRemaining);
			}

			//バッファを補充
			if (!FillBuffer()) {
				if constexpr (std::is_arithmetic<T>::value) {
					return T(0); //算術型の場合はゼロを返す
				}
				else {
					return T();  //その他の型の場合はデフォルト値を返す
				}
			}

			//補充後のデータを統合
			std::memcpy(tempBuffer + bytesRemaining, buffer.data(), typeSize - bytesRemaining);

			//インデックスを更新
			bufferIndex = typeSize - bytesRemaining;

			//値を作成して返す
			T value{};
			std::memcpy(&value, tempBuffer, typeSize);
			return value;
		}

		//バッファ内で収まる場合
		T value{};
		std::memcpy(&value, &buffer[bufferIndex], typeSize);
		bufferIndex += typeSize;
		return value;
	}
}
