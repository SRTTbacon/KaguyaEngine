#include "BinaryWriter.h"

#include <DirectXMath.h>
#include <vector>
#include <string>
#include <ios>

namespace KaguyaEngine::File
{
    bool BinaryWriter::OpenWrite(const std::string filePath, bool bAppend)
    {
        bMemory = false;

        std::ios::openmode mode = std::ios::binary | std::ios::out;
        if (bAppend) {
            mode |= std::ios::app;
        }
        outputStream.open(filePath, mode);
        if (!outputStream.is_open()) {
            return false;
        }
        return true;
    }

    bool BinaryWriter::OpenWrite()
    {
        memoryStream.str("");
        memoryStream.clear();
        bMemory = true;
        return true;
    }

    bool BinaryWriter::IsOpened()
    {
        if (bMemory) {
            return true;
        }

        return outputStream.is_open();
    }

    void BinaryWriter::Write(const std::string& value)
    {
        unsigned char length = static_cast<unsigned char>(value.size());
        Write(length); //長さを書き込む
        if (bMemory) {
            memoryStream.write(value.data(), length); //文字列の内容を書き込む
        }
        else {
            outputStream.write(value.data(), length); //文字列の内容を書き込む
        }
    }

    void BinaryWriter::Write(const char* buffer, size_t size)
    {
        if (bMemory) {
            memoryStream.write(buffer, size);
        }
        else {
            outputStream.write(buffer, size);
        }
    }

    void BinaryWriter::Write(const DirectX::XMMATRIX& matrix)
    {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                Write(matrix.r[i].m128_f32[j]);
            }
        }
    }

    void BinaryWriter::Write(const DirectX::XMFLOAT3& value)
    {
        Write(value.x);
        Write(value.y);
        Write(value.z);
    }

    void BinaryWriter::Write(const DirectX::XMFLOAT4& value)
    {
        Write(value.x);
        Write(value.y);
        Write(value.z);
        Write(value.w);
    }

    void BinaryWriter::Write(const unsigned char byte)
    {
        char c = byte;
        if (bMemory) {
            memoryStream.write(&c, 1);
        }
        else {
            outputStream.write(&c, 1);
        }
    }

    void BinaryWriter::Write(const bool b)
    {
        char c = (char)b;
        if (bMemory) {
            memoryStream.write(&c, 1);
        }
        else {
            outputStream.write(&c, 1);
        }
    }

    std::vector<char> BinaryWriter::GetBuffer() const
    {
        if (bMemory) {
            //memoryStreamからstd::stringを取得
            const std::string& buffer = memoryStream.str();

            //std::stringをstd::vector<char>に変換
            return std::vector<char>(buffer.begin(), buffer.end());
        }
        else {
            return std::vector<char>();
        }
    }

    size_t BinaryWriter::GetNowPointer()
    {
        if (bMemory) {
            return memoryStream.tellp();
        }
        else {
            return outputStream.tellp();
        }
    }

    void BinaryWriter::Close()
    {
        if (bMemory) {
            memoryStream.str("");
        }
        else {
            outputStream.close();
        }
    }
}
