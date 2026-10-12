//*****************************************************************************
//
// カメラクラス
//
// Camera.h
//
// T_Takeuchi
//
// 2026/6/14
//
//*****************************************************************************

#pragma once

#include "..\\..\\Core\CoreMinimal.h"
#include "..\\..\\Core\\EngineDefine.h"
#include "..\\Transform\\Transform.h"

#include <d3d12.h>
#include <dxgiformat.h>
#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathVector.inl>
#include <vector>

namespace KaguyaEngine::PostProcess
{
	class PostProcessManager;
}
namespace KaguyaEngine::Core
{
	class DescriptorHeap;
	class PipelineState;
	class RootSignature;
}

namespace KaguyaEngine
{
	struct CameraResource
	{
		RenderResource depthResource;		// 深度バッファ
		RenderResource prevDepthResource;	// 前フレームの深度バッファ
		RenderResource normalResource;		// 法線バッファ
		RenderResource velocityResource;	// 速度バッファ
	};

	class Camera : public Transform
	{
	public:
		Camera(Core::DescriptorHeap* pRtvDescriptorHeap, Core::DescriptorHeap* pDsvDescriptorHeap, DirectX::XMUINT2 renderSize, const DirectX::XMUINT2* pWindowsSize);
		Camera(Core::DescriptorHeap* pRtvDescriptorHeap, Core::DescriptorHeap* pDsvDescriptorHeap, D3D12_VIEWPORT viewport, D3D12_RECT scissor, const DirectX::XMUINT2* pWindowsSize);

		~Camera();

		virtual void Initialize(uint32 renderTargetCount = 0);

		// 一人称視点カメラ
		void UpdateFirstPerson(DirectX::XMFLOAT3& target, DirectX::XMFLOAT3& rotation, float distance);

		// 三人称視点カメラ
		void UpdateThirdPerson(DirectX::XMFLOAT3& target, DirectX::XMFLOAT3& rotation, float distance);

		// レンダーテクスチャの再構築
		// カメラの解像度を変更した際に実行
		virtual void ReBuildRenderTextures(bool bResetPostProcess = true);

		// カメラ情報からマトリックスを作成
		// ※エンジンから実行される
		virtual void LateUpdate();

		virtual void BeginDraw(bool bClear = true);
		virtual void EndDraw3D();
		virtual void EndDraw();

		void SetEyePos(const DirectX::XMFLOAT3& eyePos);

		void SetTargetPos(const DirectX::XMFLOAT3& targetPos);

		void SetFov(float fovDegree);

	public:
		inline DirectX::XMMATRIX GetViewMatrix() const { return m_viewMatrix; }
		inline DirectX::XMMATRIX GetViewMatrixPrev() const { return m_viewMatrixPrev; }
		inline DirectX::XMMATRIX GetProjMatrix() const { return m_projMatrix; }
		inline DirectX::XMMATRIX GetProjMatrixPrev() const { return m_projMatrixPrev; }
		inline DirectX::XMVECTOR GetEyePos() const { return m_eyePos; }
		inline DirectX::XMFLOAT3 GetEyePosFloat() const { DirectX::XMFLOAT3 eyePos; DirectX::XMStoreFloat3(&eyePos, m_eyePos); return eyePos; }
		inline DirectX::XMVECTOR GetTargetPos() const { return m_targetPos; }
		inline DirectX::XMFLOAT3 GetTargetPosFloat3() const { DirectX::XMFLOAT3 targetPos; DirectX::XMStoreFloat3(&targetPos, m_targetPos); return targetPos; }
		inline DirectX::XMVECTOR GetDirection() const { return m_direction; }
		inline DirectX::XMFLOAT3 GetDirectionFloat3() const { DirectX::XMFLOAT3 direction; DirectX::XMStoreFloat3(&direction, m_direction); return direction; }
		inline DirectX::XMVECTOR GetFoward() const { return DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(m_targetPos, m_eyePos)); }
		inline DirectX::XMVECTOR GetRight() const { return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(m_upFoward, GetFoward())); };
		inline DirectX::XMVECTOR GetUpFoward() const { return m_upFoward; }
		inline DirectX::XMFLOAT2 GetJitter() const { return m_jitter; }
		inline DirectX::XMFLOAT2 GetJitterPrev() const { return m_prevJitter; }
		inline DirectX::XMFLOAT2 GetDrawSize() const { return DirectX::XMFLOAT2(m_drawSize.x / m_pWindowsSize->x, m_drawSize.y / m_pWindowsSize->y); }
		inline DirectX::XMUINT2 GetRenderSize() const { return DirectX::XMUINT2(static_cast<uint32>(m_drawSize.x), static_cast<uint32>(m_drawSize.y)); }
		inline uint32 GetRenderTargetCount() const { return static_cast<uint32>(m_renderTextures.size()); }

		RenderResource* GetFinalRender();
		inline const CameraResource& GetCameraResource() const { return m_cameraResource; }
		inline KaguyaEngine::PostProcess::PostProcessManager* GetPostProcessManager() const { return m_pPostProcessManager.get(); }

	public:
		bool operator<(const Camera& pAnother) const
		{
			return m_priority < pAnother.m_priority;
		}

	public:
		D3D12_VIEWPORT m_renderViewport;	// 3Dビューポート
		D3D12_RECT m_renderScissor;			// シザー矩形

		// 最終描画優先度 (0=最後尾。0に近いほど奥に描画される)
		uint32 m_priority;

		float m_near;	// 近距離を見えなくする範囲
		float m_far;	// 遠距離を見えなくする範囲
		float m_fov;	// 視野

		// 最終描画位置 (0.0f～1.0f)
		DirectX::XMFLOAT2 m_drawPosition;

		// m_eyePosやm_targetPosの更新をユーザーが行うか
		bool m_bManualMode;
		// PostProcessが有効かどうか
		bool m_bPostProcessEnable;
		// ジッターを有効にするかどうか (TAAやFSR等を使用する際に有効化)
		bool m_bEnableJitter;

	protected:
		DirectX::XMVECTOR m_eyePos;
		DirectX::XMVECTOR m_targetPos;
		DirectX::XMVECTOR m_direction;
		DirectX::XMVECTOR m_upFoward;

		DirectX::XMMATRIX m_viewMatrix;
		DirectX::XMMATRIX m_viewMatrixPrev;
		DirectX::XMMATRIX m_projMatrix;
		DirectX::XMMATRIX m_projMatrixPrev;

		float m_aspect;

	protected:
		// ディファードレンダリングのライティング用のバッファ
		struct LightingCBuffer
		{
			DirectX::XMMATRIX invView;			// 逆ビュー行列
			DirectX::XMMATRIX invProjection;	// 逆プロジェクション行列
			DirectX::XMFLOAT4 lightDir;			// ライトの方向
			DirectX::XMFLOAT4 diffuseColor;		// 標準の色
			DirectX::XMFLOAT4 ambientColor;		// 影色
			DirectX::XMFLOAT4 fogColor;			// フォグの色
			DirectX::XMFLOAT2 fogStartEnd;		// フォグの開始距離、終了距離
			DirectX::XMFLOAT3 cameraPosition;	// カメラの位置
			float sceneTime;					// 時間
		};

		LightingCBuffer m_lightingCBuffer;

	protected:
		std::vector<ID3D12Resource*> m_renderTextures;	// レンダーターゲットの配列

		ObjectPtr<KaguyaEngine::PostProcess::PostProcessManager> m_pPostProcessManager;
		ObjectPtr<class KaguyaEngine::Core::PipelineState> m_pLightingPipelineState;
		ObjectPtr<class KaguyaEngine::Core::RootSignature> m_pLightingRootSignature;

		ComPtr<ID3D12Resource> m_pMainRenderTexture;	// ライティング用
		ComPtr<ID3D12Resource> m_pAlbedoTexture;		// アルベド用
		ComPtr<ID3D12Resource> m_pDepthTexture;			// 深度テクスチャ
		ComPtr<ID3D12Resource> m_pPrevDepthTexture;		// 前フレームの深度テクスチャ
		ComPtr<ID3D12Resource> m_pNormalTexture;		// 法線テクスチャ
		ComPtr<ID3D12Resource> m_pVelocityTexture;		// 速度テクスチャ

		ComPtr<ID3D12Resource> m_pLightingCBuffer[FRAME_BUFFER_COUNT];	// ライティング用のCBuffer
		void* m_pLightingCBufferMapped[FRAME_BUFFER_COUNT];				// ライティング用のCBufferのマップ先

		Core::DescriptorHeap* m_pRtvDescriptorHeap;
		Core::DescriptorHeap* m_pDsvDescriptorHeap;

		RenderResource m_mainRenderResource;
		RenderResource m_albedoRenderResource;
		CameraResource m_cameraResource;

		DirectX::XMFLOAT2 m_drawSize;		// 最終描画サイズ (0.0f～1.0f)
		const DirectX::XMUINT2* m_pWindowsSize;

		// ----------ジッター変数----------
		DirectX::XMFLOAT2 m_jitter;
		DirectX::XMFLOAT2 m_prevJitter;
		uint32 m_jitterIndex;

		// ----------RTVとSRV変数----------
		uint32 m_rtvDescriptorHeapIndex;
		uint32 m_dsvDescriptorHeapIndex;

		void CreateRenderTexture(ComPtr<ID3D12Resource>& pResource, RenderResource& renderResource, DXGI_FORMAT format, uint32 renderTargetOffset, bool bRenderTexture = true);

		// ディファードレンダリングのライティングパス
		void DrawLightingPass();

		// ジッターを取得
		float GetHaltonJitter(UINT frameIndex, int base) const;
		// カメラにジッターを適用
		void UpdateJitter();
		// 深度バッファをprevにコピー
		void CopyDepthBuffer();
		// ライティングパス用のパイプラインを作成
		void CreateLightingPipeline();
	};
}
