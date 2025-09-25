#pragma once
#include "Utils/CommonUtils.h"
#include "Graphics/Render/RenderGraph.h"
#include "Graphics/Render/RenderShader.h"
#include "HAL/HALResources.h"

//https://www.realtimerendering.com/advances/s2008/SIGGRAPH%202008%20-%20Advanced%20virtual%20texture%20topics.pdf


namespace Shard::VT
{

    class VirtualTexture : public Render::RenderResource
    {
    public:
        void Init(Render::RenderGraph& graphics);
        void UnInit();
        void PreRender();
        void PostRender();
        virtual ~VirtualTexture();
    protected:
        uint2 AddressVirtualToPhysX(uint2 virtual_address);
    private:
        Render::Field physX_tbl_; //table map virtual page to physX page
        Render::Field physX_page_pool_;
        Render::Field PhysX_page_flags_;

        uint2 physX_page_size_{ 0u, 0u };
        uint32_t frame_index_{ 0u };     
    };

    //virtual texture page management
    namespace PM
    {
        static constexpr uint32_t default_cs_groupXY = 8u;
        static constexpr uint32_t default_cs_groupX = 256u;

        class UpdatePhysXPageTblShader : public Render::RenderShader
        {
        public:
            struct ShaderParameter
            {

            };
		public:
			bool ShouldCompile(const ShaderCompileEnv& env, uint32_t permutation_id)
			{

			}

			void ModifyCompileEnv(ShaderCompileEnv& env)
			{
				env.AddDefine("USE_UPDATE_PAGETBL_CS", "1");
			}
        };

        static void AddUpatePageTblPass(Render::RenderGraph& graph)
        {
            //shader parameter group
            static const String tag = "UpdatePageTblPass";
            static const uint3 dispatch_groups{ 256, 8, 8 };
            UpdatePageTblShader::ShaderParameter& paramers = xx;
            //todo configure
            Render::RenderComputeShaderHelper::AddPass(graph, , dispatch_groups, tag);
        }

        class UpdatePhysXPagesShader : public Render::RenderShader
        {
        public:
            struct ShaderParameter
            {

            };
        };

        class GeneratePageFlagsShader : public Render::RenderShader
        {
        public:
            struct ShaderParameter
            {

            };
        };

        static void AddGeneratePageFlagsPass()
        {

        }
    }
}
