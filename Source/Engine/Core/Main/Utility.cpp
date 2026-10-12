#include "Utility.h"
#include "..\\Hash\\xxhash.h"
#include "..\CoreMinimal.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <Windows.h>
#include <DirectXMath.h>
#include <cassert>
#include <wchar.h>
#include <cstdlib>
#include <DirectXMathVector.inl>
#include <string>
#include <vector>

using namespace DirectX;

static std::string Trim(const std::string& str)
{
    auto front = std::find_if_not(str.begin(), str.end(), ::isspace);
    auto back = std::find_if_not(str.rbegin(), str.rend(), ::isspace).base();
    return (front < back ? std::string(front, back) : "");
}

std::string UTF8ToShiftJIS(std::string utf8Str)
{
    //Unicodeへ変換後の文字列長を得る
    int lenghtUnicode = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), (int)(utf8Str.size()) + 1, NULL, 0);

    //必要な分だけUnicode文字列のバッファを確保
    wchar_t* bufUnicode = new wchar_t[lenghtUnicode];

    //UTF8からUnicodeへ変換
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), static_cast<int>((utf8Str.size())) + 1, bufUnicode, lenghtUnicode);

    //ShiftJISへ変換後の文字列長を得る
    int lengthSJis = WideCharToMultiByte(CP_THREAD_ACP, 0, bufUnicode, -1, NULL, 0, NULL, NULL);

    //必要な分だけShiftJIS文字列のバッファを確保
    char* bufShiftJis = new char[lengthSJis];

    //UnicodeからShiftJISへ変換
    WideCharToMultiByte(CP_THREAD_ACP, 0, bufUnicode, lenghtUnicode + 1, bufShiftJis, lengthSJis, NULL, NULL);

    std::string strSJis(bufShiftJis);

    delete[] bufUnicode;
    delete[] bufShiftJis;

    return strSJis;
}

std::string ShiftJISToUTF8(const std::string& sjis)
{
    if (sjis.empty()) return std::string();

    //Shift-JIS -> UTF-16 (wstring)
    int wlen = ::MultiByteToWideChar(932 /* CP_SHIFT_JIS (Microsoft) */,
        MB_ERR_INVALID_CHARS,
        sjis.data(), static_cast<int>(sjis.size()),
        nullptr, 0);
    if (wlen == 0) {
        // 変換失敗
        return std::string();
    }

    std::wstring wstr;
    wstr.resize(wlen);
    int wlen2 = ::MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
        sjis.data(), static_cast<int>(sjis.size()),
        &wstr[0], wlen);
    if (wlen2 == 0) {
        return std::string();
    }

    //UTF-16 -> UTF-8
    int u8len = ::WideCharToMultiByte(CP_UTF8,
        0,
        wstr.data(), wlen,
        nullptr, 0,
        nullptr, nullptr);
    if (u8len == 0) {
        return std::string();
    }

    std::string utf8;
    utf8.resize(u8len);
    int u8len2 = ::WideCharToMultiByte(CP_UTF8,
        0,
        wstr.data(), wlen,
        &utf8[0], u8len,
        nullptr, nullptr);
    if (u8len2 == 0) {
        return std::string();
    }

    return utf8;
}

std::string GetUTF8(const std::string& str)
{
    int wlen = ::MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (wlen == 0) {
        return str;
    }

    return ShiftJISToUTF8(str);
}

//指定した文字列特有のIDを生成
//超極まれに別の文字列から同じIDが生成される (数億分の1)
uint32 HashString(const std::string& str, bool use32bits)
{
    std::string lowerName = str;
    transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return tolower(c); });
    std::vector<uint8> bytes(lowerName.begin(), lowerName.end());

    const uint32 prime = 16777619;
    const uint32 offset = 2166136261;
    const uint32 mask = 1073741823;
    uint32 hash = offset;
    for (BYTE byte : bytes) {
        hash *= prime;
        hash ^= byte;
    }
    if (use32bits)
        return hash;
    else
        return (hash >> 30) ^ (hash & mask);
}

DirectX::XMFLOAT2 Lerp(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, float t)
{
    DirectX::XMFLOAT2 temp{};
    temp.x = std::lerp(a.x, b.x, t);
    temp.y = std::lerp(a.y, b.y, t);

    return temp;
}

//XMFLOAT3の線形補間
//引数 : 補間元のXMFLOAT3, 補間先のXMFLOAT3, 補間地点(0.0f～1.0f)
DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
{
    DirectX::XMFLOAT3 temp{};
    temp.x = std::lerp(a.x, b.x, t);
    temp.y = std::lerp(a.y, b.y, t);
    temp.z = std::lerp(a.z, b.z, t);

    return temp;
}

//XMFLOAT4の線形補間
//引数 : 補間元のXMFLOAT4, 補間先のXMFLOAT4, 補間地点(0.0f～1.0f)
DirectX::XMFLOAT4 Lerp(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t)
{
    DirectX::XMFLOAT4 temp{};
    temp.x = std::lerp(a.x, b.x, t);
    temp.y = std::lerp(a.y, b.y, t);
    temp.z = std::lerp(a.z, b.z, t);
    temp.w = std::lerp(a.w, b.w, t);

    return temp;
}

std::string GetFileExtension(const std::string& filePath)
{
    //ファイルパス内で最後のドットを検索
    size_t dotPos = filePath.find_last_of(".");

    //ドットが見つからない場合、または最後にない場合は空文字を返す
    if (dotPos == std::string::npos || dotPos == filePath.length() - 1) {
        return "";
    }

    std::string temp = filePath.substr(dotPos);
    size_t nullPos = temp.find('\0');
    if (nullPos != std::string::npos) {
        temp = temp.substr(0, nullPos);
    }

    return Trim(temp);
}

std::wstring GetFileExtension(const std::wstring& filePath)
{
    //ファイルパス内で最後のドットを検索
    size_t dotPos = filePath.find_last_of(L'.');
    size_t slashPos = filePath.find_last_of(L"/\\");

    //ドットが見つからない場合、または最後にない場合は空文字を返す
    //また、ドットがパス区切り文字の後ろにない場合も無効
    if (dotPos == std::wstring::npos || dotPos <= slashPos || dotPos == filePath.length() - 1) {
        return L"";
    }

    //拡張子を返す
    return filePath.substr(dotPos);
}

std::wstring GetWideString(const std::string& str)
{
    if (str.empty()) {
        return std::wstring();
    }

    int num1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
    if (num1 <= 0) {
        num1 = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
        if (num1 <= 0) {
            return std::wstring();
        }
    }

    std::wstring wstr;
    wstr.resize(static_cast<size_t>(num1 - 1)); //終端文字を除く

    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, &wstr[0], num1);

    return wstr;
}

std::u8string GetU8String(const std::string& str)
{
    std::u8string u8str(reinterpret_cast<const char8_t*>(str.c_str()), str.size());
    return u8str;
}

std::u16string GetU16String(const std::string& str)
{
    //UTF-8をUTF-16に変換
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len == 0) return u"";

    std::u16string result(len - 1, u'\0'); //終端nullを除くため -1
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, reinterpret_cast<wchar_t*>(&result[0]), len);
    return result;
}

std::string GetNarrowString(const std::wstring& str)
{
    int num1 = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);

    std::string narrowStr;
    narrowStr.resize(num1);

    int num2 = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &narrowStr[0], num1, nullptr, nullptr);

    assert(num1 == num2);
    return narrowStr;
}

std::string GetStringFromU8(const std::u8string& str)
{
    return std::string(reinterpret_cast<const char*>(str.c_str()), str.size());
}

std::string GetStringFromU16(const std::u16string& str)
{
    // UTF-16をUTF-8に変換
    int len = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(str.c_str()), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) return "";

    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(str.c_str()), -1, &result[0], len, nullptr, nullptr);
    return result;
}

bool MoveFileToRecycle(std::wstring filePath)
{
    SHFILEOPSTRUCT fileOp = { 0 };
    fileOp.wFunc = FO_DELETE;

    // ダブルNULL終端の必要がある
    wchar_t from[MAX_PATH + 1] = { 0 };
    wcsncpy_s(from, filePath.c_str(), MAX_PATH);
    from[filePath.length() + 1] = 0;  // ダブルNULL終端

    fileOp.pFrom = from;
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;

    int result = SHFileOperation(&fileOp);
    return (result == 0); // 成功時は0
}

std::vector<std::string> GetSprits(std::string& str, char delim)
{
    std::vector<std::string> elems;
    std::string item;
    for (char ch : str) {
        if (ch == delim) {
            if (!item.empty())
                elems.push_back(item);
            item.clear();
        }
        else {
            item += ch;
        }
    }
    if (!item.empty())
        elems.push_back(item);
    return elems;
}

uint32 GenerateIDFromFile(const std::string& filePath)
{
    char buffer[_MAX_PATH];
    if (!_fullpath(buffer, filePath.c_str(), _MAX_PATH)) {
        return 0;
    }

    return HashString(buffer);
}

uint32 GenerateIDFromFile(const std::wstring& filePath)
{
    return GenerateIDFromFile(GetNarrowString(filePath));
}

uint32 GenerateIDFromFile(const std::vector<char>& buffer)
{
    return GenerateIDFromFile(buffer.data(), buffer.size());
}

uint32 GenerateIDFromFile(const char* data, size_t size)
{
    size_t step = std::max<size_t>(1, size / HASH_SAMPLERATE);

    XXH32_state_t* state = XXH32_createState();
    XXH32_reset(state, 0);

    for (size_t i = 0; i < size; i += step)
    {
        XXH32_update(state, data + i, 1);
    }

    uint32 hash = XXH32_digest(state);
    XXH32_freeState(state);

    return hash;
}

uint32 ColorToUINT(float r, float g, float b, float a)
{
    uint8 red = static_cast<uint8>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
    uint8 green = static_cast<uint8>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
    uint8 blue = static_cast<uint8>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
    uint8 alpha = static_cast<uint8>(std::clamp(a, 0.0f, 1.0f) * 255.0f);

    //RGBAの順番で32ビット値にエンコード
    return (alpha << 24) | (blue << 16) | (green << 8) | red;
}

DirectX::XMVECTOR ExtractEulerAngles(const DirectX::XMMATRIX& matrix)
{
    float pitch, yaw, roll;

    //Y軸方向の回転（yaw）
    yaw = asinf(-matrix.r[2].m128_f32[0]);

    if (cosf(yaw) > 0.0001f) {
        //X軸とZ軸方向の回転（pitch と roll）
        pitch = atan2f(matrix.r[2].m128_f32[1], matrix.r[2].m128_f32[2]);
        roll = atan2f(matrix.r[1].m128_f32[0], matrix.r[0].m128_f32[0]);
    }
    else {
        //Y軸が 90度または -90度の時は、Gimbal Lockに対応
        pitch = atan2f(-matrix.r[0].m128_f32[2], matrix.r[1].m128_f32[1]);
        roll = 0.0f;
    }

    return XMVectorSet(pitch, yaw, roll, 0.0f); //ラジアン角で出力
}
