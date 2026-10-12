//*****************************************************************************
//
// バイナリデータをファイルに書き込むクラス
//
// BinaryWriter.h
//
// T_Takeuchi
//
// 2026/6/28
//
//*****************************************************************************

#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <DirectXMath.h>

//ファイルにバイナリデータを書き込むクラス

namespace KaguyaEngine::File
{
	class BinaryWriter
	{
	public:
		//出力先を指定
		//引数 : const std::string 出力先, bool 追記するかどうか
		bool OpenWrite(const std::string filePath, bool bAppend = false);
		bool OpenWrite();

		//正常に開かれているか
		bool IsOpened();

		//プリミティブ変数を保存
		template<typename T>
		void Write(const T& value)
		{
			if (bMemory) {
				memoryStream.write(reinterpret_cast<const char*>(&value), sizeof(T));
			}
			else {
				outputStream.write(reinterpret_cast<const char*>(&value), sizeof(T));
			}
		}

		//文字列を保存
		void Write(const std::string& value);
		//指定したサイズのバッファを保存
		void Write(const char* buffer, size_t size);
		//マトリックスを保存
		void Write(const DirectX::XMMATRIX& matrix);
		//XMFLOAT3を保存
		void Write(const DirectX::XMFLOAT3& value);
		//XMFLOAT4を保存
		void Write(const DirectX::XMFLOAT4& value);
		//1バイト文字を保存
		void Write(const unsigned char byte);
		//bool型を保存
		void Write(const bool b);

		std::vector<char> GetBuffer() const;

		size_t GetNowPointer();

		void Close();

	private:
		std::ofstream outputStream;			//ファイル出力用
		std::ostringstream memoryStream;	//メモリ出力用

		bool bMemory = false;
	};
}
