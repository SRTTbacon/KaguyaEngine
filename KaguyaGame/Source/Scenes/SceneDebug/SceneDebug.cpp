#include "SceneDebug.h"
#include "..\SceneBase\SceneBase.h"

#include <Engine\\3D\\PostProcess\\DepthOfField.h>
#include <Engine\\3D\\PostProcess\\AntiAliasing\\TAA.h>

SceneDebug::SceneDebug()
	: SceneBase()
{
}

SceneDebug::~SceneDebug()
{
}

void SceneDebug::Start()
{
	g_Engine->m_bClearRenderTexture = false;

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	LARGE_INTEGER t1;
	QueryPerformanceCounter(&t1);

	g_Engine->GetSkyBox()->SetSkyTexture("Resource/Texture/Skybox2.dds");
	g_Engine->GetDirectionalLight()->m_diffuseColor = XMFLOAT4(1.2f, 1.2f, 1.2f, 1.0f);
	g_Engine->GetDirectionalLight()->m_ambientColor = XMFLOAT4(0.2f, 0.2f, 0.4f, 1.0f);
	g_Engine->GetMainCamera()->GetPostProcessManager()->AddPostProcess<PTAA>(0);
	g_Engine->GetMainCamera()->m_bEnableJitter = true;
	for (uint32 i = 0; i < g_Engine->GetCameraCount(); i++)
	{
		Camera* pCamera = g_Engine->GetCameraIndex(i);
		pCamera->GetPostProcessManager()->AddPostProcess<PDepthOfField>(1);
	}

	m_pTexture1 = g_Engine->GetUIManager()->AddUITexture("Resource/Body all.png");
	m_pTexture2 = g_Engine->GetUIManager()->AddUITexture("Resource/aaaa.png");
	m_pTexture3 = g_Engine->GetUIManager()->AddUITexture("Resource/Emergency.png");
	m_pTexture4 = g_Engine->GetUIManager()->AddUITexture("Resource/bbbbb.png");
	m_pTexture1->m_size = XMFLOAT2(0.2f, 0.2f);
	m_pTexture2->m_size = XMFLOAT2(0.2f, 0.2f);
	m_pTexture3->m_size = XMFLOAT2(0.2f, 0.2f);
	m_pTexture4->m_size = XMFLOAT2(0.2f, 0.2f);
	m_pModel1 = g_Engine->GetModelManager()->CreateModel<PrimitiveModel>();
	m_pModel2 = g_Engine->GetModelManager()->CreateModel<PrimitiveModel>();
	m_pModel1->m_bVisible = false;
	m_pModel2->m_bVisible = false;
	m_pModel1->LoadModel("Resource/Model/Plane.fbx");
	m_pModel1->SetOnLoaded(
		[this]()
		{
			PrimitiveMaterial* pMaterial = g_Engine->GetMaterialManager()->CreateMaterial<PrimitiveMaterial>().get();
			pMaterial->Initialize();
			m_pModel1->GetMesh(0)->SetMaterial(pMaterial);
		});
	m_pModel2->LoadModel("Resource/Model/Test1.fbx");
	m_pModel2->SetOnLoaded(
		[this]()
		{
			PrimitiveMaterial* pMaterial = g_Engine->GetMaterialManager()->CreateMaterial<PrimitiveMaterial>().get();
			pMaterial->Initialize();
			m_pModel2->GetMesh(0)->SetMaterial(pMaterial);
		});
	m_pModel2->AddRotationX(180.0f);

	m_pCharacter1 = g_Engine->GetModelManager()->CreateModel<CharacterModel>();
	m_pCharacter1->AddRotationX(180.0f);
	m_pCharacter1->LoadModel("Resource/Model/Moe/FBX_Moe.fbx");
	m_pCharacter1->SetOnLoaded([this]()
		{
			BoneMaterial* pBodyMaterial = g_Engine->GetMaterialManager()->CreateMaterial<BoneMaterial>().get();
			pBodyMaterial->Initialize();
			pBodyMaterial->SetTexture(0, "Resource/Model/Moe/Body.png");
			BoneMaterial* pBodyAllMaterial = g_Engine->GetMaterialManager()->CreateMaterial<BoneMaterial>().get();
			pBodyAllMaterial->Initialize();
			pBodyAllMaterial->SetTexture(0, "Resource/Model/Moe/Body all.png");
			BoneMaterial* pClothMaterial = g_Engine->GetMaterialManager()->CreateMaterial<BoneMaterial>().get();
			pClothMaterial->Initialize(D3D12_CULL_MODE_NONE);
			pClothMaterial->SetTexture(0, "Resource/Model/Moe/Cloth.png");
			BoneMaterial* pHairMaterial = g_Engine->GetMaterialManager()->CreateMaterial<BoneMaterial>().get();
			pHairMaterial->Initialize(D3D12_CULL_MODE_NONE);
			pHairMaterial->SetTexture(0, "Resource/Model/Moe/Hair.png");
			for (uint32 i = 0; i < m_pCharacter1->GetMeshCount(); i++) {
				switch (i) {
				case 0:
					m_pCharacter1->GetMesh(i)->SetMaterial(pClothMaterial);
					break;
				case 1:
					m_pCharacter1->GetMesh(i)->SetMaterial(pHairMaterial);
					break;
				case 2:
					m_pCharacter1->GetMesh(i)->SetMaterial(pHairMaterial);
					break;
				case 3:
					m_pCharacter1->GetMesh(i)->SetMaterial(pHairMaterial);
					break;
				case 4:
					m_pCharacter1->GetMesh(i)->SetMaterial(pClothMaterial);
					break;
				case 5:
					m_pCharacter1->GetMesh(i)->SetMaterial(pClothMaterial);
					break;
				case 6:
					m_pCharacter1->GetMesh(i)->SetMaterial(pClothMaterial);
					break;
				case 7:
					m_pCharacter1->GetMesh(i)->SetMaterial(pClothMaterial);
					break;
				case 8:
					m_pCharacter1->GetMesh(i)->SetMaterial(pClothMaterial);
					break;
				case 9:
					m_pCharacter1->GetMesh(i)->SetMaterial(pHairMaterial);
					break;
				case 10:
					m_pCharacter1->GetMesh(i)->SetMaterial(pClothMaterial);
					break;
				case 11:
					m_pCharacter1->GetMesh(i)->SetMaterial(pBodyAllMaterial);
					break;
				case 12:
					//m_pCharacter1->GetMesh(i)->SetMaterial(pBodyMaterial);
					break;
				case 13:
					m_pCharacter1->GetMesh(i)->SetMaterial(pBodyMaterial);
					break;
				case 14:
					m_pCharacter1->GetMesh(i)->SetMaterial(pHairMaterial);
					break;
				}
			}
		});
	LARGE_INTEGER t2;
	QueryPerformanceCounter(&t2);
	float ms =
		(float)(t2.QuadPart - t1.QuadPart)
		* 1000.0f
		/ (float)freq.QuadPart;
	printf("%f ms\n", ms);
}

void SceneDebug::Update()
{
	if (g_Engine->GetInput()->GetKeyState(DIK_LEFTARROW)) {
		g_Engine->GetDirectionalLight()->AddRotationZ(5.0f);
	}
	if (g_Engine->GetInput()->GetKeyState(DIK_RIGHTARROW)) {
		g_Engine->GetDirectionalLight()->AddRotationZ(-5.0f);
	}

	if (g_Engine->GetInput()->GetKeyStateSync(DIK_B))
	{
		g_Engine->GetMainCamera()->m_bPostProcessEnable = !g_Engine->GetMainCamera()->m_bPostProcessEnable;
	}

	if (m_pCharacter1 && m_pCharacter1->IsLoaded())
	{
		//m_pCharacter1->GetBoneManager().GetBone("Left shoulder")->m_rotation = XMFLOAT4(XMConvertToRadians(-30.0f), XMConvertToRadians(-30.0f), XMConvertToRadians(-30.0f), 1.0f);
	}
}

void SceneDebug::Draw2D()
{
	//2D描画のみ行う
	//m_pTexture1->Draw();
	//m_pTexture2->Draw();
	//m_pTexture3->Draw();
	//m_pTexture4->Draw();
}
