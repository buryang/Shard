#pragma once
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "Graphics/HAL/HALResources.h"
#include "Graphics/HAL/HALRenderPass.h"
#include "Graphics/HAL/HALCommand.h"
#include "Graphics/HAL/HALResourceBinding.h"
#include "Graphics/HAL/HALShaderLibrary.h"
#include "Graphics/HAL/HALResourcePool.h"
#include "Graphics/HAL/HALMemoryResidency.h"

namespace Shard::HAL
{
    enum class EHALBackEnd : uint8_t
    {
        eNone,
        eVulkan, //now only support vulkan
        eD3D,
        eMetal,
    };

    static inline constexpr bool IsBackEndSupported(EHALBackEnd back_end) {
        LOG(INFO) << "current only support vulkan backend";
        return back_end == EHALBackEnd::eVulkan;
    }

    enum class EHALFeatureLevel : uint8_t
    {
        eS3_1,
        eSM5,
        eSM6,
        eNum,
    };

	/**group all resource bindless heap by life-time*/
	enum class EHALResourceGroup : uint8_t
	{
		eStatic,
		eFrame,
		ePass,
		eOther,
		eNum,
	};

    ///global entity parameters
    REGIST_PARAM_TYPE(UINT, HAL_ENTITY_BACKEND, EHALBackEnd::eVulkan);
    REGIST_PARAM_TYPE(UINT, HAL_FEATURE_LEVEL, EHALFeatureLevel::eSM5);
    REGIST_PARAM_TYPE(BOOL, HAL_ASYNC_COMPUTE, false);

    //rhi mulithread config
    REGIST_PARAM_TYPE(BOOL, HAL_EXECUTE_BYPSS, false); //whether generate immediate command buffer
    REGIST_PARAM_TYPE(BOOL, HAL_EXECUTE_PARALLEL, false); //whether generate command in parallel mode

    //whether enable VRS shading
    REGIST_PARAM_TYPE(BOOL, HAL_SHADING_VRS, false); 

    //whether use resource pool to recyle resource
    REGIST_PARAM_TYPE(BOOL, HAL_RESOURCE_POOL_ENABLE, false);
    REGIST_PARAM_TYPE(UINT, HAL_RESOURCE_POOL_FRAME_GAP, 6);
    REGIST_PARAM_TYPE(UINT64, HAL_RESOURCE_POOL_MAX_SIZE, 1024 * 1024 * 2048);

    //like unreal global dynamic HAL
    class MINIT_API HALGlobalEntity
    {
    public:
        using CreateFunc = std::function<HALGlobalEntity*(void)>;
        static HALGlobalEntity* Instance();
        static bool RegistCreateFunc(EHALBackEnd back_end, CreateFunc&& func);
        virtual ~HALGlobalEntity() {}
        virtual void Init();
        virtual void UnInit();
        virtual void CreateSampler();
        virtual void CreateViewPoint();
        virtual HALRenderPass::Handle CreatePass(const HALRenderPassInitializer& pass_initializer);
        //only metal support shader library
        virtual HALShaderLibraryInterface* GetOrCreateShaderLibrary();
        virtual HALPipelineStateObjectLibraryInterface* GetOrCreatePSOLibrary();
        /**
        * \brief get/create memory residency manager
        */
        virtual HALMemoryResidencyManager* GetOrCreateMemoryResidencyManager();
        //bindless heap interface, group different resources by group
        /**
         * group resource updates by frame or render pass to minimize state changes.
		 * use a versioning system to track which resources need updating.
		 * avoid updating the entire descriptor heap/set unless absolutely necessary.
         * @param resource_group 
         * @return 
         */
        virtual HALResourceBindlessHeap* GetResourceBindlessHeap(EHALResourceGroup resource_group);
#if defined(DEVELOP_DEBUG_TOOLS)&&defined(ENABLE_IMGUI)
        virtual HALImGuiLayerWrapper* GetImGuiLayerWrapper() { return nullptr; }
#endif
        HALBuffer* CreateBuffer(const HALBufferInitializer& desc);
        void ReleaseBuffer(HALBuffer* buffer);
        HALTexture* CreateTexture(const HALTextureInitializer& desc);
        void ReleaseTexture(HALTexture* texture);
        //virtual HALResource* CreateRayTracingAccelerateStruct();
        virtual HALCommandContext* CreateCommandBuffer();
        virtual void SetViewPoint();
        virtual void ResizeViewPoint();

        //queue logic
        virtual uint32_t GetQueueCount(EPipeline type) const = 0;
        virtual HALCommandQueue* GetQueue(EPipeline type, uint32_t index) const = 0; 

        //hardware property check
        virtual bool IsAsyncComputeSupported()const {
            LOG(ERROR) << "async compute check function not exsited";
        }
        virtual bool IsHWRayTraceSupported()const {
            LOG(ERROR) << "hardware ray tracing check function not exisited";
        }
    protected:
        virtual HALBuffer* CreateBufferImpl(const HALBufferInitializer& desc) = 0;
        virtual HALTexture* CreateTextureImpl(const HALTextureInitializer& desc) = 0;
        virtual void DestroyBufferImpl(HALBuffer* buffer) = 0;
        virtual void DestroyTextureImpl(HALTexture* texture) = 0;
    private:
        HALGlobalEntity() = default;
        DISALLOW_COPY_AND_ASSIGN(HALGlobalEntity);
    private:
        static Map<EHALBackEnd, CreateFunc> create_func_repo_;
        HALPooledResourceAllocator  pooled_allocator_;
#ifdef DEVELOP_DEBUG_TOOLS 
        SmallVector<RenderGPUEventPool> timestamp_query_pool_;
        SmallVector<RenderGPUEventPool> occlusion_query_pool_;
        SmallVector<RenderGPUEventPool> pipeline_stat_query_pool_;
#endif

    };

//regist global entity create func at compile time
#define REGIST_GLOBAL_ENTITY(name, back_end, creat_func) \
static constexpr bool gxxx_##name##_created = HALGlobalEntity::RegistCreateFunc(back_end, create_func);
}

