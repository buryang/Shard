#include "VirtualShadowMap.h"

REGIST_PARAM_TYPE(UINT, VSM_CLIPMAP_MAX_LOD, 100);
REGIST_PARAM_TYPE(FLOAT, VSM_CLIPMAP_LOD_BIAS, 0.5f);
REGIST_PARAM_TYPE(UINT, VSM_CLIPMAP_SIZE, 1 << 16);


namespace Shard::Effect
{

	bool VirtualShadowMapIndirectArgsShader::ShouldCompile(const ShaderCompileEnv& env, uint32_t permutation_id)
	{
		const auto& platform = env.platform_;
		if (platform.shader_model_ >= EShaderModel::eSM_6) //for using wave ops
		{
			return true;
		}
		return false;
	}

	void VirtualShadowMapIndirectArgsShader::ModifyCompileEnv(ShaderCompileEnv& env)
	{
		env.macros_.emplace_back(""); //todo
	}
}
