#pragma once
#include "VirtualTexture.h"

namespace Shard::VT
{
	class VirtualShadowMapIndirectArgsShader : public Render::RenderShader
	{
	public:
		struct ShaderParameter
		{
			Render::Field    vsm_page_flags_;
			Render::Field    vsm_page_allocations_;
			Render::Field    vsm_indirect_args_;
			uint32_t    max_page_updates_{ 1024u * 1024u };
		};
	public:
		static bool ShouldCompile(const ShaderCompileEnv& env, uint32_t permutation_id);
		static void ModifyCompileEnv(ShaderCompileEnv& env);
	};

	class VirtualShadowMapCacheManager;
    class VirtualShadowMap : public VirtualTexture
    {
    public:
        using IDType = uint32_t;
        static constexpr IDType invalid_mapID = ~0u;
        static constexpr uint32_t page_size = 128u;
		void BuildVSMPageAllocations();
    protected:
        IDType id_{ invalid_mapID };
    };
}
