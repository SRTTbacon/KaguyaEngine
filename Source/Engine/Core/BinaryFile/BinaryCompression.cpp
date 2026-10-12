#include "BinaryCompression.h"

#include <cstdio>
#include <vector>
#include <lz4.h>

void BinaryCompress(const char* src, size_t srcSize, std::vector<char>& dst)
{
    // 圧縮後の最大サイズを取得
    int maxCompressedSize = LZ4_compressBound(static_cast<int>(srcSize));
    dst.resize(maxCompressedSize);

    // 圧縮
    int compressedSize = LZ4_compress_default(src, dst.data(), static_cast<int>(srcSize), maxCompressedSize);

    if (compressedSize <= 0) {
        printf("圧縮中にエラーが発生しました。\n");
        return;
    }

    // 必要なサイズにリサイズ
    dst.resize(compressedSize);
}

void BinaryDecompress(std::vector<char>& dst, size_t originalSize, const char* src, size_t srcSize)
{
    dst.resize(originalSize);

    // 解凍
    int decompressedSize = LZ4_decompress_safe(src, dst.data(), static_cast<int>(srcSize), static_cast<int>(originalSize));

    if (decompressedSize < 0) {
        printf("解凍中にエラーが発生しました。\n");
    }
}
