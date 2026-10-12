//*****************************************************************************
//
// テクスチャの読み込みおよび管理
// 使用する際はObjectPtr<Texture2D>推奨
//
// Texture2D.h
//
// T_Takeuchi
//
// 2026/5/24
//
//*****************************************************************************

#pragma once

#include "..\\CoreMinimal.h"

#include <d3d12.h>
#include <dxgitype.h>
#include <DirectXTex.h>
#include <string>
#include <memory>
#include <dxgiformat.h>
#include <vector>
#include <functional>
#include <atomic>

namespace KaguyaEngine::Texture
{
	struct TextureInfo
	{
		std::string filePath;	//ファイルパス
		std::string fileName;	//hcsファイルの場合の識別名

		DirectX::TexMetadata metadata;	//テクスチャのメタデータ

		DXGI_RGBA textureColor;

		bool bUseColor;
		bool bHCSFile;			//hcs形式かどうか
		bool bDDSFormat;		//dds形式かどうか
		bool bBuffer;			//ファイルからではなく、メモリから読み取ったテクスチャかどうか

		TextureInfo()
			: filePath("")
			, fileName("")
			, textureColor(DXGI_RGBA(0.0f, 0.0f, 0.0f, 1.0f))
			, metadata(DirectX::TexMetadata())
			, bUseColor(false)
			, bHCSFile(false)
			, bDDSFormat(false)
			, bBuffer(false)
		{}
	};

	struct TextureManage
	{
		ComPtr<ID3D12Resource> pResource;		// リソース
		TextureInfo textureInfo;
		uint32 uniqueID;				// ユニークID
		std::atomic<bool> bLoaded;		// 読み込み完了
		bool bSimpleTex;				// 単色

		TextureManage();
		~TextureManage();
	};

	class Texture2D : public std::enable_shared_from_this<Texture2D>
	{
	public:
		// ファイルパスからテクスチャを読み込む
		// 非同期で読み込まれるため、IsValid()で読み込み完了を確認してから使用してください。
		Texture2D();
		~Texture2D();

		// ファイルパスからテクスチャを読み込む (非同期)
		// 非同期で読み込まれるため、IsValid()で読み込み完了を確認してから使用してください。
		void LoadTexture(const std::string& path, bool bDDSFile);
		// バイナリデータからテクスチャを読み込む (非同期)
		// dataはstd::moveされるため、使用後は無効になります。
		void LoadTexture(std::vector<char>& data, bool bDDSFile);
		// バイナリデータからテクスチャを読み込む (コピー&非同期)
		void LoadTexture(const char* data, size_t dataSize, bool bDDSFile);
		// 単色テクスチャを生成 (同期)
		// 引数 : r, g, b 0.0f～1.0fの範囲で色を指定
		void LoadTexture(float r, float g, float b, float a = 1.0f, uint32 width = 16, uint32 height = 16, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

		// テクスチャのロードをメインスレッドに伝える
		// ※非同期でのロードのみ実行
		void SetOnLoaded(std::function<void()> callback);

		bool IsValid() const { return m_pManage && m_pManage->bLoaded; }
		bool IsSimpleTex() const {return m_pManage && m_pManage->bSimpleTex; }

		inline ID3D12Resource* GetResource() const { return m_pManage ? m_pManage->pResource.Get() : nullptr; }

		inline TextureManage* GetTextureManage() const { return m_pManage.get(); }
		inline TextureInfo* GetTextureInfo() { return m_pManage ? &m_pManage->textureInfo : nullptr; }

	private:
		struct AsyncData
		{
			ComPtr<ID3D12Resource> m_pOutResource;
			TextureInfo m_outTextureInfo;
			uint16 m_loadIndex;

			AsyncData()
				: m_pOutResource(nullptr)
				, m_outTextureInfo(TextureInfo())
				, m_loadIndex(0)
			{}
		};

	private:
		void operator = (const Texture2D&) = delete;

		void LoadAsync(const std::string& path, bool bDDSFile);
		void LoadAsync(std::vector<char>& data, bool bDDSFile);
		void LoadAsync(ObjectPtr<Texture2D> pSelfPtr, ObjectPtr<AsyncData> pAsyncData, DirectX::TexMetadata& meta, DirectX::ScratchImage& image);

		void Initialize();

		void Destroy();

	private:
		static std::vector<ObjectPtr<TextureManage>> s_textures;

		std::atomic<uint16> m_loadIndex;		// LoadTextureの実行回数 (非同期で順番が入れ替わらないように)

		std::function<void()> m_onLoaded;	// メインスレッドに完了を報告

		ObjectPtr<TextureManage> m_pManage;	//テクスチャ管理構造体
	};
}
