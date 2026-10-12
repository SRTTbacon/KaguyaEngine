#include "Bone.h"
#include "..\..\..\Core\CoreMinimal.h"

#include <DirectXMath.h>
#include <DirectXMathVector.inl>
#include <DirectXMathMatrix.inl>
#include <cstdint>
#include <string>
#include <utility>
#include <algorithm>

using namespace DirectX;

namespace KaguyaEngine::Model
{
    Bone::Bone(std::string boneName, const uint32 boneIndex, const XMFLOAT3* pModelPosition, const XMFLOAT4* pModelRotation, const XMFLOAT3* pModelScale)
        : m_position(0.0f, 0.0f, 0.0f)
        , m_rotation(0.0f, 0.0f, 0.0f, 1.0f)
        , m_scale(1.0f, 1.0f, 1.0f)
        , m_pParentBone(nullptr)
        , m_boneName(boneName)
        , m_boneWorldMatrix(XMMatrixIdentity())
        , m_boneOffset(XMMatrixIdentity())
        , m_boneLocalOffset(XMMatrixIdentity())
        , m_boneWorldMatrixWithoutLocal(XMMatrixIdentity())
        , m_boneFinalOffset(XMMatrixIdentity())
        , m_boneGlobalOffset(XMMatrixIdentity())
        , m_parentBoneIndex(UINT32_MAX)
        , m_boneIndex(boneIndex)
        , m_pModelPosition(pModelPosition)
        , m_pModelRotation(pModelRotation)
        , m_pModelScale(pModelScale)
    {}

    void Bone::SetBoneOffset(const XMMATRIX& offset)
    {
        m_boneOffset = offset;
    }

    void Bone::AddChildBone(Bone* pChildBone, bool bAutoSetParent)
    {
        m_childBones.push_back({ pChildBone, pChildBone->GetBoneIndex() });

        if (bAutoSetParent) {
            pChildBone->SetParentBone(this, false);
        }
    }

    void Bone::SetParentBone(Bone* pParentBone, bool bAutoAddChild)
    {
		// すでに親ボーンが存在する場合、子ボーンリストから削除
        if (m_pParentBone && m_pParentBone != pParentBone) {
            auto& children = m_pParentBone->m_childBones;
            children.erase(std::remove_if(children.begin(), children.end(),
                [this](const ChildBone& child) {
                    return child.boneIndex == GetBoneIndex();
                }), children.end());
        }

        m_pParentBone = pParentBone;
        m_parentBoneIndex = pParentBone->GetBoneIndex();

		if (bAutoAddChild) {
			pParentBone->AddChildBone(this, false);
		}
    }

    void Bone::UpdateGlobalMatix(DirectX::XMMATRIX& globalTransform)
    {
        m_boneWorldMatrix = globalTransform;
    }

    void Bone::UpdateGlobalMatrixWithoutLocal(DirectX::XMMATRIX& globalTransform)
    {
        XMMATRIX modelScaleMat = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
        XMMATRIX rotX = XMMatrixRotationX(m_pModelRotation->x);
        XMMATRIX rotY = XMMatrixRotationY(m_pModelRotation->y);
        XMMATRIX rotZ = XMMatrixRotationZ(m_pModelRotation->z);
        XMMATRIX modelRotMat = rotX * rotY * rotZ;
        XMMATRIX modelPosMat = XMMatrixTranslation(m_pModelPosition->x, m_pModelPosition->y, m_pModelPosition->z);
        m_boneWorldMatrixWithoutLocal = m_boneOffset * globalTransform * modelScaleMat * modelRotMat * modelPosMat;
    }

    void Bone::Clone(Bone* pToBone) const
    {
        pToBone->m_position = m_position;
        pToBone->m_rotation = m_rotation;
        pToBone->m_scale = m_scale;
        pToBone->m_boneOffset = m_boneOffset;
        pToBone->m_boneLocalOffset = m_boneLocalOffset;
        pToBone->m_boneGlobalOffset = m_boneGlobalOffset;
        pToBone->m_boneFinalOffset = m_boneFinalOffset;
    }

    BoneManager::BoneManager(const XMFLOAT3* pModelPosition, const XMFLOAT4* pModelRotation, const XMFLOAT3* pModelScale)
        : m_armatureBone(Bone("Armature", UINT32_MAX, pModelPosition, pModelRotation, pModelScale))
        , m_pModelPosition(pModelPosition)
        , m_pModelRotation(pModelRotation)
        , m_pModelScale(pModelScale)
    {}

    Bone* BoneManager::AddBone(const std::string boneName, const uint32 boneIndex)
    {
        if (m_boneInfos.size() >= MAX_BONE_COUNT) {
            return nullptr;
        }

        Bone bone(boneName, boneIndex, m_pModelPosition, m_pModelRotation, m_pModelScale);

        //ボーン名とIndexを紐づけ
        m_boneMapping[boneName] = boneIndex;
        //配列に追加
        m_boneInfos.push_back(XMMatrixIdentity());
        m_bones.push_back(bone);

        return &m_bones[m_bones.size() - 1];
    }

    void BoneManager::UpdateBoneMatrix()
    {
        if (m_bones.size() <= 0) {
            return;
        }

        //ボーンの最終位置を決定
        UpdateBoneMatrix(0);
    }

    void BoneManager::UpdateBoneMatrix(uint32 boneIndex)
    {
        XMMATRIX parentTransform = XMMatrixIdentity();

        //親ボーンが存在する場合、そのワールド座標を適用
        if (m_bones[boneIndex].GetParentBone()) {
            //parentTransform = m_boneInfos[m_bones[boneIndex].GetParentBoneIndex()];
            parentTransform = m_bones[boneIndex].GetParentBone()->GetGlobalTransform();;
        }
        else {
            Bone& armatureBone = m_armatureBone;
            XMMATRIX armatureScale = XMMatrixScaling(armatureBone.m_scale.x, armatureBone.m_scale.y, armatureBone.m_scale.z);
            XMVECTOR armatureRotVec = XMVectorSet(-armatureBone.m_rotation.x, -armatureBone.m_rotation.z, -armatureBone.m_rotation.y, armatureBone.m_rotation.w);
            XMMATRIX armatureRot = XMMatrixRotationQuaternion(armatureRotVec);

            XMMATRIX armaturePos = XMMatrixTranslation(-armatureBone.m_position.x, -armatureBone.m_position.z, armatureBone.m_position.y);

            XMMATRIX boneTransform = armatureScale * armatureRot * armaturePos;

            //親のワールド変換とローカル変換を合成
            parentTransform = boneTransform;
        }

        //ボーンの最終的な位置、回転、スケールを決定

        Bone& bone = m_bones[boneIndex];

        XMMATRIX scale = XMMatrixScaling(bone.m_scale.x, bone.m_scale.y, bone.m_scale.z);
        XMVECTOR rotVec = XMVectorSet(-bone.m_rotation.x, -bone.m_rotation.z, bone.m_rotation.y, bone.m_rotation.w);

        XMMATRIX rot = XMMatrixRotationQuaternion(rotVec);

        XMMATRIX pos = XMMatrixTranslation(bone.m_position.x, bone.m_position.z, bone.m_position.y);

        XMMATRIX offsetBack2 = bone.GetBoneOffset();
        XMMATRIX offsetBack = XMMatrixIdentity();

        offsetBack.r[3].m128_f32[0] = offsetBack2.r[3].m128_f32[0];
        offsetBack.r[3].m128_f32[1] = offsetBack2.r[3].m128_f32[1];
        offsetBack.r[3].m128_f32[2] = offsetBack2.r[3].m128_f32[2];

        XMVECTOR tScale, tRotationQuat, tTranslation;
        XMMatrixDecompose(&tScale, &tRotationQuat, &tTranslation, bone.m_boneGlobalOffset);
        tRotationQuat = XMVectorSet(XMVectorGetX(tRotationQuat), XMVectorGetY(tRotationQuat), XMVectorGetZ(tRotationQuat), XMVectorGetW(tRotationQuat));
        XMMATRIX rot2 = XMMatrixRotationQuaternion(tRotationQuat);
        XMMatrixDecompose(&tScale, &tRotationQuat, &tTranslation, bone.m_boneLocalOffset);
        tRotationQuat = XMVectorSet(XMVectorGetX(tRotationQuat), XMVectorGetY(tRotationQuat), XMVectorGetZ(tRotationQuat), XMVectorGetW(tRotationQuat));
        XMMATRIX rot3 = XMMatrixRotationQuaternion(tRotationQuat);

        XMMATRIX boneTransform = scale * XMMatrixInverse(nullptr, offsetBack) * rot * offsetBack * pos;
        XMMATRIX boneTransformWithoutRotate = scale * pos;
        XMMATRIX boneTransformWithoutLocal = scale * XMMatrixInverse(nullptr, offsetBack) * offsetBack;

        //親のワールド変換とローカル変換を合成
        XMMATRIX finalTransform = boneTransform * parentTransform;

        bone.m_boneFinalOffset = finalTransform;

        XMMATRIX finalTransformWithoutRotate = parentTransform * boneTransformWithoutRotate;
        XMMATRIX finalTransformWithoutLocal = parentTransform * XMMatrixTranspose(boneTransformWithoutLocal);
        XMMATRIX finalTransformNonTranspose = XMMatrixTranspose(finalTransform);
        XMMATRIX finalTransformNonTransposeWidthoutLocal = XMMatrixTranspose(finalTransformWithoutLocal);

        //シェーダーに送るワールド座標に代入
        m_boneInfos[boneIndex] = XMMatrixTranspose(finalTransform);
        bone.UpdateGlobalMatix(finalTransform);
        bone.UpdateGlobalMatrixWithoutLocal(finalTransformNonTransposeWidthoutLocal);

        //子ボーンにも変換を伝播
        for (uint32 i = 0; i < bone.GetChildBoneCount(); i++) {
            UpdateBoneMatrix(bone.GetChildBoneIndex(i));
        }
    }
    void BoneManager::ResetBoneManager()
    {
        m_bones.clear();
        m_boneInfos.clear();
        m_boneMapping.clear();
    }
}