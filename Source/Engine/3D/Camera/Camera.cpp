#include "Camera.h"
#include "..\\..\\Core\\CoreMinimal.h"
#include "..\\..\\Core\\EngineDefine.h"
#include "..\\..\\Core\\DescriptorHeap\\DescriptorHeap.h"
#include "..\\..\\Core\\Pipeline\\PipelineState.h"
#include "..\\..\\Core\\Pipeline\\RootSignature.h"
#include "..\\PostProcess\\Core\\PostProcessManager.h"
#include "..\\Lights\\DirectionalLight.h"

#include "..\\..\\Engine.h"

#include <d3dx12.h>
#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <DirectXMathVector.inl>
#include <dxgiformat.h>
#include <d3d12.h>
#include <cstdio>
#include <vector>
#include <Windows.h>

using namespace DirectX;
using namespace KaguyaEngine::Core;
using namespace KaguyaEngine::PostProcess;

namespace KaguyaEngine
{
	// CreateRenderTexture()で作成するレンダーテクスチャのデフォルト数
    static constexpr uint32 DEFAULT_RENDERTARGET_COUNT = 4;

	Camera::Camera(Core::DescriptorHeap* pRtvDescriptorHeap, Core::DescriptorHeap* pDsvDescriptorHeap, DirectX::XMUINT2 renderSize, const DirectX::XMUINT2* pWindowsSize)
        : m_pRtvDescriptorHeap(pRtvDescriptorHeap)
        , m_pDsvDescriptorHeap(pDsvDescriptorHeap)
        , m_pWindowsSize(pWindowsSize)
        , m_eyePos(XMVectorSet(0.0f, 1.0f, -3.0f, 0.0f))
        , m_targetPos(XMVectorSet(0.0f, 0.0f, 0.75f, 0.0f))
        , m_upFoward(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))
        , m_direction(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))
        , m_fov(XMConvertToRadians(45.0f))
        , m_viewMatrix(XMMatrixIdentity())
        , m_projMatrix(XMMatrixIdentity())
        , m_drawPosition(0.0f, 0.0f)
        , m_drawSize(static_cast<float>(renderSize.x), static_cast<float>(renderSize.y))
        , m_aspect(static_cast<float>(renderSize.x) / static_cast<float>(renderSize.y))
        , m_cameraResource(CameraResource())
        , m_lightingCBuffer(LightingCBuffer())
        , m_near(0.1f)
        , m_far(500.0f)
        , m_priority(0)
        , m_jitter(XMFLOAT2(0.0f, 0.0f))
        , m_prevJitter(XMFLOAT2(0.0f, 0.0f))
        , m_jitterIndex(0)
        , m_rtvDescriptorHeapIndex(0)
        , m_bManualMode(false)
        , m_bPostProcessEnable(true)
		, m_bEnableJitter(false)
	{
        m_position = XMFLOAT3(0.0f, 1.0f, -10.0f);

        m_renderViewport.TopLeftX = 0;
        m_renderViewport.TopLeftY = 0;
        m_renderViewport.Width = static_cast<float>(renderSize.x);
        m_renderViewport.Height = static_cast<float>(renderSize.y);
        m_renderViewport.MinDepth = 0.0f;
        m_renderViewport.MaxDepth = 1.0f;

        m_renderScissor.left = 0;
        m_renderScissor.right = static_cast<int>(m_pWindowsSize->x);
        m_renderScissor.top = 0;
        m_renderScissor.bottom = static_cast<int>(m_pWindowsSize->y);

        m_dsvDescriptorHeapIndex = m_pDsvDescriptorHeap->GetNextAvailableDescriptor();

        m_pPostProcessManager = CreateObjectPtr<PostProcessManager>(this, m_pRtvDescriptorHeap);

        m_mainRenderResource.pResource = nullptr;

        for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
            m_pLightingCBufferMapped[i] = nullptr;
        }
    }

    Camera::Camera(Core::DescriptorHeap* pRtvDescriptorHeap, Core::DescriptorHeap* pDsvDescriptorHeap, D3D12_VIEWPORT viewport, D3D12_RECT scissor, const DirectX::XMUINT2* pWindowsSize)
        : m_pRtvDescriptorHeap(pRtvDescriptorHeap)
        , m_pDsvDescriptorHeap(pDsvDescriptorHeap)
        , m_pWindowsSize(pWindowsSize)
        , m_eyePos(XMVectorSet(0.0f, 1.0f, -3.0f, 0.0f))
        , m_targetPos(XMVectorSet(0.0f, 0.0f, 0.75f, 0.0f))
        , m_upFoward(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))
        , m_direction(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))
        , m_fov(XMConvertToRadians(35.0f))
        , m_viewMatrix(XMMatrixIdentity())
        , m_projMatrix(XMMatrixIdentity())
        , m_drawPosition(0.0f, 0.0f)
        , m_drawSize(static_cast<float>(pWindowsSize->x), static_cast<float>(pWindowsSize->y))
        , m_cameraResource(CameraResource())
		, m_lightingCBuffer(LightingCBuffer())
        , m_near(0.1f)
        , m_far(500.0f)
        , m_priority(0)
        , m_jitter(XMFLOAT2(0.0f, 0.0f))
        , m_prevJitter(XMFLOAT2(0.0f, 0.0f))
        , m_jitterIndex(0)
        , m_rtvDescriptorHeapIndex(0)
        , m_dsvDescriptorHeapIndex(0)
        , m_bManualMode(false)
        , m_bPostProcessEnable(true)
        , m_bEnableJitter(false)
    {
        m_position = XMFLOAT3(0.0f, 1.0f, -10.0f);
        m_renderViewport = viewport;
        m_renderScissor = scissor;
		m_aspect = m_renderViewport.Width / m_renderViewport.Height;

        m_dsvDescriptorHeapIndex = m_pDsvDescriptorHeap->GetNextAvailableDescriptor();

        m_pPostProcessManager = CreateObjectPtr<PostProcessManager>(this, m_pRtvDescriptorHeap);

        m_mainRenderResource.pResource = nullptr;

        for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
            m_pLightingCBufferMapped[i] = nullptr;
        }
    }

    Camera::~Camera()
    {
        // ライティング用CBufferの解放
        for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
            if (m_pLightingCBufferMapped[i]) {
                m_pLightingCBuffer[i]->Unmap(0, nullptr);
                m_pLightingCBufferMapped[i] = nullptr;
            }
        }
    }

    void Camera::Initialize(uint32 renderTargetCount)
    {
        m_pPostProcessManager->SetFrameResource(this);
        m_pPostProcessManager->Initialize();

        if (m_rtvDescriptorHeapIndex == 0)
        {
			m_rtvDescriptorHeapIndex = m_pRtvDescriptorHeap->GetNextAvailableDescriptor(renderTargetCount == 0 ? DEFAULT_RENDERTARGET_COUNT : renderTargetCount);
        }

        m_lightingCBuffer.sceneTime = 0.0f;

        CreateLightingPipeline();
        ReBuildRenderTextures(false);
    }

    void Camera::ReBuildRenderTextures(bool bResetPostProcess)
    {
        m_renderTextures.clear();

        // レンダーテクスチャを作成
        CreateRenderTexture(m_pMainRenderTexture, m_mainRenderResource, DXGI_FORMAT_R16G16B16A16_FLOAT, 0);
        CreateRenderTexture(m_pAlbedoTexture, m_albedoRenderResource, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
        CreateRenderTexture(m_pNormalTexture, m_cameraResource.normalResource, DXGI_FORMAT_R10G10B10A2_UNORM, 2);
        CreateRenderTexture(m_pVelocityTexture, m_cameraResource.velocityResource, DXGI_FORMAT_R16G16_FLOAT, 3);
		CreateRenderTexture(m_pPrevDepthTexture, m_cameraResource.prevDepthResource, DXGI_FORMAT_R32_TYPELESS, 4, false);   // 前フレームの深度バッファはレンダーターゲットとして使用しない

        // 深度テクスチャを作成
        D3D12_CLEAR_VALUE dsvClearValue{};
        dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        dsvClearValue.DepthStencil.Depth = 1.0f;
        dsvClearValue.DepthStencil.Stencil = 0;

        CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resourceDesc(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            static_cast<uint64>(m_renderViewport.Width),
            static_cast<uint32>(m_renderViewport.Height),
            1,
            1,
            DXGI_FORMAT_R32_TYPELESS,
            1,
            0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        HRESULT hr = m_pDevice->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &dsvClearValue,
            IID_PPV_ARGS(m_pDepthTexture.ReleaseAndGetAddressOf())
        );

        if (FAILED(hr))
        {
            printf("DepthStencilBufferの生成に失敗しました。\n");
            return;
        }

        //ディスクリプタを作成
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDsvDescriptorHeap->GetCpuDescriptorHandle(m_dsvDescriptorHeapIndex);

        //深度ステンシルビュー設定用構造体の設定
        D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        desc.Flags = D3D12_DSV_FLAG_NONE;

        m_pDevice->CreateDepthStencilView(m_pDepthTexture.Get(), &desc, dsvHandle);

		// 深度テクスチャのSRVを作成
        m_cameraResource.depthResource.pResource = m_pDepthTexture.Get();
        uint32 depthHeapIndex = m_pDescriptor->pMainHeap->SetResource(m_cameraResource.depthResource.pResource, DXGI_FORMAT_R32_FLOAT);
        m_cameraResource.depthResource.mainSrvHandleCPU = m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(depthHeapIndex);
        m_cameraResource.depthResource.mainSrvHandleGPU = m_pDescriptor->pMainHeap->GetGpuDescriptorHandle(depthHeapIndex);
        m_cameraResource.depthResource.mainSrvIndex = depthHeapIndex;

        if (bResetPostProcess)
        {
            m_pPostProcessManager->ChangeRenderSize();
        }
    }

    void Camera::UpdateFirstPerson(DirectX::XMFLOAT3& target, DirectX::XMFLOAT3& rotation, float distance)
    {
        XMMATRIX rotMatrix = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);

        // カメラの正面方向（ターゲットからカメラへ）
        XMVECTOR defaultForward = XMVectorSet(0, 0, 1, 0); // 1人称なので前方
        XMVECTOR cameraOffsetDir = XMVector3TransformNormal(defaultForward, rotMatrix);
        cameraOffsetDir = XMVector3Normalize(cameraOffsetDir);

        // キャラクターの中心位置（高さオフセット含む）
        XMVECTOR targetPos = XMVectorSet(target.x, target.y, target.z, 1.0f);

        // カメラの位置 = ターゲット位置 + カメラ方向 * 距離
        m_eyePos = targetPos;
        m_targetPos = XMVectorAdd(targetPos, XMVectorScale(cameraOffsetDir, 1.0f));
        m_direction = XMVector3Normalize(XMVectorSubtract(m_eyePos, m_targetPos));

        // 上方向ベクトルを回転から算出
        XMVECTOR defaultUp = XMVectorSet(0, 1, 0, 0);
        XMVECTOR upDir = XMVector3TransformNormal(defaultUp, rotMatrix);

        XMStoreFloat3(&m_position, m_eyePos);
    }

    void Camera::UpdateThirdPerson(DirectX::XMFLOAT3& target, DirectX::XMFLOAT3& rotation, float distance)
    {
        XMMATRIX rotMatrix = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);

        // カメラの正面方向（ターゲットからカメラへ）
        XMVECTOR defaultForward = XMVectorSet(0, 0, -1, 0); // 3人称なので後方
        XMVECTOR cameraOffsetDir = XMVector3TransformNormal(defaultForward, rotMatrix);
        cameraOffsetDir = XMVector3Normalize(cameraOffsetDir);

        // キャラクターの中心位置（高さオフセット含む）
        XMVECTOR targetPos = XMVectorSet(target.x, target.y, target.z, 1.0f);

        // カメラの位置 = ターゲット位置 + カメラ方向 * 距離
        m_eyePos = XMVectorAdd(targetPos, XMVectorScale(cameraOffsetDir, -distance));
        m_targetPos = targetPos;
        m_direction = XMVector3Normalize(XMVectorSubtract(m_eyePos, m_targetPos));

        // 上方向ベクトルを回転から算出
        XMVECTOR defaultUp = XMVectorSet(0, 1, 0, 0);
        XMVECTOR upDir = XMVector3TransformNormal(defaultUp, rotMatrix);

        XMStoreFloat3(&m_position, m_eyePos);
    }

    void Camera::LateUpdate()
    {
        if (!m_bManualMode)
        {
            XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);

            //方向ベクトルを作る
            XMVECTOR defaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            m_direction = XMVector3Normalize(XMVector3TransformNormal(defaultForward, rotationMatrix));

            m_eyePos = XMVectorSet(m_position.x, m_position.y, m_position.z, 0.0f);

            //ターゲット座標を設定
            m_targetPos = XMVectorAdd(m_eyePos, m_direction);
        }

		m_viewMatrixPrev = m_viewMatrix;
        m_viewMatrix = XMMatrixLookAtRH(m_eyePos, m_targetPos, m_upFoward);
		m_projMatrixPrev = m_projMatrix;
        m_projMatrix = XMMatrixPerspectiveFovRH(m_fov, m_aspect, m_near, m_far);

        if (m_bEnableJitter) {
            UpdateJitter();
        }

		// ライティング用のCBufferを更新
		m_lightingCBuffer.invView = XMMatrixInverse(nullptr, m_viewMatrix);
		m_lightingCBuffer.invProjection = XMMatrixInverse(nullptr, m_projMatrix);
		m_lightingCBuffer.lightDir = g_Engine->GetDirectionalLight()->GetLightDirection();
		m_lightingCBuffer.diffuseColor = g_Engine->GetDirectionalLight()->m_diffuseColor;
        m_lightingCBuffer.ambientColor = g_Engine->GetDirectionalLight()->m_ambientColor;
		m_lightingCBuffer.fogColor = g_Engine->GetDirectionalLight()->m_fogColor;
		m_lightingCBuffer.fogStartEnd = g_Engine->GetDirectionalLight()->m_fogStartEnd;
        m_lightingCBuffer.sceneTime += *m_pFrameTime;
		m_lightingCBuffer.cameraPosition = GetEyePosFloat();

        if (m_pLightingCBufferMapped[*m_pCurrentBackBufferIndex]) {
            memcpy(m_pLightingCBufferMapped[*m_pCurrentBackBufferIndex], &m_lightingCBuffer, sizeof(LightingCBuffer));
        }
    }

    void Camera::BeginDraw(bool bClear)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvDescriptorHeap->GetCpuDescriptorHandle(m_albedoRenderResource.mainRtvIndex);
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDsvDescriptorHeap->GetCpuDescriptorHandle(m_dsvDescriptorHeapIndex);

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles(m_renderTextures.size() - 1);

        // レンダーターゲットのリソース状態を変更
        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        for (uint32 i = 1; i < static_cast<uint32>(m_renderTextures.size()); i++)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTextures[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            barriers.push_back(barrier);
        }
        GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());

        // レンダーテクスチャをクリア (真っ黒)
        if (bClear)
        {
            float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            for (uint32 i = 1; i < static_cast<uint32>(m_renderTextures.size()); i++)
            {
                GetCommandList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
				rtvHandle.ptr += m_pRtvDescriptorHeap->GetHeapSize();
            }
		}

        rtvHandles[0] = m_pRtvDescriptorHeap->GetCpuDescriptorHandle(m_albedoRenderResource.mainRtvIndex);
        for (size_t i = 1; i < m_renderTextures.size() - 1; i++) {
            rtvHandles[i] = rtvHandles[i - 1];
            rtvHandles[i].ptr += m_pRtvDescriptorHeap->GetHeapSize();
        }

        // 深度ステンシルをクリア
        GetCommandList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        GetCommandList()->RSSetViewports(1, &m_renderViewport);
        GetCommandList()->RSSetScissorRects(1, &m_renderScissor);

        GetCommandList()->OMSetRenderTargets(static_cast<uint32>(rtvHandles.size()), rtvHandles.data(), false, &dsvHandle);
    }

    void Camera::EndDraw3D()
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        for (uint32 i = 1; i < static_cast<uint32>(m_renderTextures.size()); i++)
        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTextures[i], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            barriers.push_back(barrier);
        }

        // ライティングパスに渡すように順番を考慮してSRVをコピー
        RenderResource* renderResources[6] = { &m_albedoRenderResource, &m_cameraResource.normalResource, &m_cameraResource.velocityResource, &m_cameraResource.depthResource, &m_cameraResource.prevDepthResource, &m_mainRenderResource };
        for (RenderResource* renderResource : renderResources)
        {
            uint32 copyIndex = m_pDescriptor->pCopyHeap->GetNextAvailableDescriptor();
            m_pDevice->CopyDescriptorsSimple(1, m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex), renderResource->mainSrvHandleCPU, m_pDescriptor->pMainHeap->GetHeapType());
            renderResource->copySrvHandleCPU = m_pDescriptor->pCopyHeap->GetCpuDescriptorHandle(copyIndex);
            renderResource->copySrvHandleGPU = m_pDescriptor->pCopyHeap->GetGpuDescriptorHandle(copyIndex);
        }

        // リソース状態をライティングパス用に変更
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pDepthTexture.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pMainRenderTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
        GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
        barriers.clear();

        // ライティングパス
        DrawLightingPass();

        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pDepthTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
        GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
        barriers.clear();
    }

    void Camera::EndDraw()
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pDepthTexture.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pMainRenderTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
        GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
        barriers.clear();

        if (m_bPostProcessEnable && m_pPostProcessManager && m_pPostProcessManager->GetPostProcessCount() > 0)
        {
            m_pPostProcessManager->Render(m_mainRenderResource);
        }

        // 深度テクスチャのリソースをコピー可能な状態にする
        barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pDepthTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE));
        GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
        barriers.clear();

		// 深度テクスチャをコピー
        CopyDepthBuffer();
    }

    void Camera::SetEyePos(const DirectX::XMFLOAT3& eyePos)
    {
        m_eyePos = XMLoadFloat3(&eyePos);
    }

    void Camera::SetTargetPos(const DirectX::XMFLOAT3& targetPos)
    {
        m_targetPos = XMLoadFloat3(&targetPos);
    }

    void Camera::SetFov(float fovDegree)
    {
        m_fov = XMConvertToRadians(fovDegree);
    }

    RenderResource* Camera::GetFinalRender()
    {
        if (m_bPostProcessEnable && m_pPostProcessManager && m_pPostProcessManager->GetPostProcessCount() > 0)
        {
			return m_pPostProcessManager->GetLastRender();
        }

        return &m_mainRenderResource;
    }

    void Camera::CreateRenderTexture(ComPtr<ID3D12Resource>& pResource, RenderResource& renderResource, DXGI_FORMAT format, uint32 renderTargetOffset, bool bRenderTexture)
    {
        //中間テクスチャをフレーム数分作成
        D3D12_RESOURCE_DESC desc2 = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<uint32>(m_renderViewport.Width), static_cast<uint32>(m_renderViewport.Height));
        desc2.SampleDesc.Count = 1;
        desc2.MipLevels = 1;
        desc2.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE* pClearValue = nullptr;

        D3D12_CLEAR_VALUE clearValue = {};
        if (bRenderTexture)
        {
            clearValue.Format = format;
            for (uint32 i = 0; i < 3; i++) {
                clearValue.Color[i] = 0.0f;
            }
            clearValue.Color[3] = 1.0f; // アルファを1.0に設定
            pClearValue = &clearValue;
        }
        else {
            desc2.Flags = D3D12_RESOURCE_FLAG_NONE;
        }

        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = m_pDevice->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc2,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            pClearValue,
            IID_PPV_ARGS(pResource.ReleaseAndGetAddressOf())
        );
        if (FAILED(hr))
        {
            printf("エラー: レンダーテクスチャの作成に失敗しました。\n");
            return;
        }
        pResource->SetName(std::wstring(L"Camera::CreateRenderTexture : " + std::to_wstring(renderTargetOffset)).c_str());

        if (bRenderTexture)
        {
            //中間テクスチャ用のRTVを作成
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = format;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            m_pDevice->CreateRenderTargetView(pResource.Get(), &rtvDesc, m_pRtvDescriptorHeap->GetCpuDescriptorHandle(m_rtvDescriptorHeapIndex + renderTargetOffset));
			renderResource.mainRtvIndex = m_rtvDescriptorHeapIndex + renderTargetOffset;
        }

        // 登録済みの場合は解放
        if (renderResource.mainSrvIndex != UINT32_MAX)
        {
            m_pDescriptor->pMainHeap->ClearDescriptor(renderResource.mainSrvIndex);
			renderResource.mainSrvIndex = UINT32_MAX;
        }

        // RenderResourceに登録
        renderResource.pResource = pResource.Get();
        uint32 mainHeapIndex;
        if (bRenderTexture) {
            mainHeapIndex = m_pDescriptor->pMainHeap->SetResource(renderResource.pResource);
        }
        else {
			mainHeapIndex = m_pDescriptor->pMainHeap->SetResource(renderResource.pResource, DXGI_FORMAT_R32_FLOAT);
        }
        renderResource.mainSrvHandleCPU = m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(mainHeapIndex);
        renderResource.mainSrvHandleGPU = m_pDescriptor->pMainHeap->GetGpuDescriptorHandle(mainHeapIndex);
        renderResource.mainSrvIndex = mainHeapIndex;

        if (bRenderTexture) {
            m_renderTextures.push_back(pResource.Get());
        }
    }

    void Camera::DrawLightingPass()
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDsvDescriptorHeap->GetCpuDescriptorHandle(m_dsvDescriptorHeapIndex);

        GetCommandList()->RSSetViewports(1, &m_renderViewport);
        GetCommandList()->RSSetScissorRects(1, &m_renderScissor);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvDescriptorHeap->GetCpuDescriptorHandle(m_mainRenderResource.mainRtvIndex);
		GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		GetCommandList()->SetGraphicsRootSignature(m_pLightingRootSignature->GetRootSignature());
		GetCommandList()->SetPipelineState(m_pLightingPipelineState->GetPipelineState());
		GetCommandList()->SetGraphicsRootConstantBufferView(0, m_pLightingCBuffer[*m_pCurrentBackBufferIndex]->GetGPUVirtualAddress());
        GetCommandList()->SetGraphicsRootDescriptorTable(1, m_albedoRenderResource.copySrvHandleGPU);

		GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GetCommandList()->DrawInstanced(3, 1, 0, 0);
    }

	float Camera::GetHaltonJitter(uint32 frameIndex, int base) const
	{
        float f = 1.0f;
        float result = 0.0f;

        while (frameIndex > 0)
        {
            f /= base;
            result += f * (frameIndex % base);
            frameIndex /= base;
        }
        return result;
	}

    void Camera::UpdateJitter()
    {
        m_prevJitter = m_jitter;

        int frame = m_jitterIndex % 1024 + 1;

        float jitterX = 2.0f * GetHaltonJitter(frame, 2) - 1.0f;
        float jitterY = 2.0f * GetHaltonJitter(frame + 1, 3) - 1.0f;

        m_jitter.x = jitterX / m_renderViewport.Width;
        m_jitter.y = jitterY / m_renderViewport.Height;

        m_projMatrix.r[2].m128_f32[0] += m_jitter.x * 2.0f;
        m_projMatrix.r[2].m128_f32[1] += m_jitter.y * 2.0f;

        m_jitterIndex++;
    }

    void Camera::CopyDepthBuffer()
    {
        std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

        // 深度テクスチャのリソースをコピー可能な状態にする
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pPrevDepthTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        barriers.push_back(barrier);
        GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
        barriers.clear();

        GetCommandList()->CopyResource(m_pPrevDepthTexture.Get(), m_pDepthTexture.Get());

        // リソースの状態を戻す
        barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pPrevDepthTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barriers.push_back(barrier);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pDepthTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        barriers.push_back(barrier);
        GetCommandList()->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
    }

    void Camera::CreateLightingPipeline()
    {
        PsoDescKey psoDesc;
        psoDesc.bDisableDepth = true;
        psoDesc.numRenderTargets = 1;
        psoDesc.rtvFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.vertexShaderPath = GetShaderPath("DeferredLightingVS.cso");
        psoDesc.pixelShaderPath = GetShaderPath("DeferredLightingPS.cso");

        // RootSignature
        CD3DX12_DESCRIPTOR_RANGE gbufferRange{};
        gbufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

        CD3DX12_ROOT_PARAMETER rootParams[2]{};
        // b0 : CB
        rootParams[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
        // t0 ～ t4 : G-Buffer
        rootParams[1].InitAsDescriptorTable(1, &gbufferRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

        // 作成
        uint64 rootSignatureHash = GetShaderHash(psoDesc.vertexShaderPath, psoDesc.pixelShaderPath);
        m_pLightingRootSignature = CreateObjectPtr<RootSignature>();
		m_pLightingRootSignature->Initialize(m_pDevice, rootSignatureHash, rootParams, _countof(rootParams), &sampler, 1);

		m_pLightingPipelineState = CreateObjectPtr<PipelineState>();
		m_pLightingPipelineState->Initialize(m_pDevice, nullptr, 0, m_pLightingRootSignature->GetRootSignature(), psoDesc);

		// ライティング用のCBufferを作成
        D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(GetAlignedSize(sizeof(LightingCBuffer)));
        for (int i = 0; i < FRAME_BUFFER_COUNT; ++i)
        {
            m_pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_pLightingCBuffer[i]));
            m_pLightingCBuffer[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_pLightingCBufferMapped[i]));
			m_pLightingCBuffer[i]->SetName(L"LightingCBuffer");
        }
    }
}
