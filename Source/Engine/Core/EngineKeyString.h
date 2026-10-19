//*****************************************************************************
//
// エンジン内部で使用するパスや文字列を定義するヘッダ
//
// EngineKeyString.h
//
// T_Takeuchi
//
// 2026/8/24
//
//*****************************************************************************

#pragma once

#include "EngineDefine.h"

namespace KaguyaEngine::KeyString
{
	inline static const std::string SHADER_MAINRENDER_VS = GetShaderPath("MainRenderVS.cso");
	inline static const std::string SHADER_MAINRENDER_PS = GetShaderPath("MainRenderPS.cso");

	inline static const std::string SHADER_MODEL_PRIMITIVE_VS = GetShaderPath("PrimitiveVS.cso");
	inline static const std::string SHADER_MODEL_PRIMITIVE_PS = GetShaderPath("PrimitivePS.cso");
	inline static const std::string SHADER_MODEL_CHARACTER_VS = GetShaderPath("BoneVS.cso");
	inline static const std::string SHADER_MODEL_CHARACTER_PS = GetShaderPath("BonePS.cso");
	inline static const std::string SHADER_MODEL_MOVIE_PS = GetShaderPath("MoviePS.cso");

	inline static const std::string SHADER_SKYBOX_VS = GetShaderPath("SkyBoxVS.cso");
	inline static const std::string SHADER_SKYBOX_PS = GetShaderPath("SkyBoxPS.cso");

	inline static const std::string SHADER_UI_TEXTURE_VS = GetShaderPath("UITextureVS.cso");
	inline static const std::string SHADER_UI_TEXTURE_PS = GetShaderPath("UITexturePS.cso");

	inline static const std::string SHADER_POSTPROCESS_DOWNSAMPLER_PS = GetShaderPath("DownSamplePS.cso");
	inline static const std::string SHADER_POSTPROCESS_UPSAMPLER_PS = GetShaderPath("UpSamplePS.cso");
	inline static const std::string SHADER_POSTPROCESS_DOF_COC_PS = GetShaderPath("CircleOfConfusion.cso");
	inline static const std::string SHADER_POSTPROCESS_DOF_BLUR_PS = GetShaderPath("DoFBlur.cso");
	inline static const std::string SHADER_POSTPROCESS_DOF_MAIN_PS = GetShaderPath("DepthOfField.cso");
}
