#pragma once
#include "BinaryWriter.h"

//バイナリデータを圧縮、解凍する関数

extern void BinaryCompress(const char* src, size_t srcSize, std::vector<char>& dst);

extern void BinaryDecompress(std::vector<char>& dst, size_t originalSize, const char* src, size_t srcSize);
