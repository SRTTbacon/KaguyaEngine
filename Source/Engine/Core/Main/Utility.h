//*****************************************************************************
//
// エンジンで使用する便利なグローバル関数をまとめたもの
//
// Utility.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>

constexpr const unsigned int HASH_SAMPLERATE = 10;

extern std::string UTF8ToShiftJIS(std::string utf8Str);
extern std::string ShiftJISToUTF8(const std::string& sjis);
extern std::string GetUTF8(const std::string& str);

//指定した文字列特有のIDを生成
//超極まれに別の文字列から同じIDが生成される (数億分の1)
extern unsigned int HashString(const std::string& str, bool use32bits = true);

//XMFLOAT2の線形補間
//引数 : 補間元のXMFLOAT2, 補間先のXMFLOAT2, 補間地点(0.0f～1.0f)
extern DirectX::XMFLOAT2 Lerp(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, float t);
//XMFLOAT3の線形補間
//引数 : 補間元のXMFLOAT3, 補間先のXMFLOAT3, 補間地点(0.0f～1.0f)
extern DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t);
//XMFLOAT4の線形補間
//引数 : 補間元のXMFLOAT4, 補間先のXMFLOAT4, 補間地点(0.0f～1.0f)
extern DirectX::XMFLOAT4 Lerp(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t);

//ファイルパスから拡張子を抽出 ('.'付き)
extern std::string GetFileExtension(const std::string& filePath);
extern std::wstring GetFileExtension(const std::wstring& filePath);

//stringからwstringへの変換
extern std::wstring GetWideString(const std::string& str);
extern std::u8string GetU8String(const std::string& str);
extern std::u16string GetU16String(const std::string& str);
extern std::string GetNarrowString(const std::wstring& str);
extern std::string GetStringFromU8(const std::u8string& str);
extern std::string GetStringFromU16(const std::u16string& str);

extern bool MoveFileToRecycle(std::wstring filePath);

extern std::vector<std::string> GetSprits(std::string& str, char delim);

//ファイル内容から一意のIDを生成
extern unsigned int GenerateIDFromFile(const std::string& filePath);
extern unsigned int GenerateIDFromFile(const std::wstring& filePath);
extern unsigned int GenerateIDFromFile(const std::vector<char>& buffer);
extern unsigned int GenerateIDFromFile(const char* data, size_t size);

//RGBAからUINTへ変換
extern unsigned int ColorToUINT(float r, float g, float b, float a = 1.0f);

extern DirectX::XMVECTOR ExtractEulerAngles(const DirectX::XMMATRIX& matrix);
