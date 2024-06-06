#include "HAL/HALTypeTraits.h"
#include "HALCommandVulkan.h"
#include "HALResourcesVulkan.h"
#include "HALRenderPassVulkan.h"
#include "HALGlobalEntityVulkan.h"
#include "HALShaderLibraryVulkan.h"
#include "HALResourceBindingVulkan.h"
#include "HALMemoryResidencyVulkan.h"

#define ADD_EXT_IF(CONDITION, EXT_NAME) if (CONDITION) { extensions.emplace_back(EXT_NAME); }
#define ADD_LAYER_IF(CONDITION, LAYER_NAME) if (CONDITION) { layers.emplace_back(LAYER_NAME); }

namespace Shard::HAL::Vulkan {
    
    REGIST_GLOBAL_ENTITY(HALGlobalEntityVulkan, EHALBackEnd::eVulkan, HALGlobalEntityVulkan::Instance);

    //instance and device extension config
    REGIST_PARAM_TYPE(BOOL, VULKAN_INSTANCE_DEBUG_REPORT_EXT, false);
    REGIST_PARAM_TYPE(BOOL, VULKAN_INSTANCE_DEBUG_UTILS_EXT, false);
    REGIST_PARAM_TYPE(BOOL, VULKAN_INSTANCE_MEMORY_BUDGET_EXT, false);
    //whether use ASYNC COMPUTE
    REGIST_PARAM_TYPE(BOOL, VULKAN_DEVICE_ASYNC_COMPUTE, true);
    REGIST_PARAM_TYPE(UINT, VULKAN_DEVICE_GFX_QUEUE_COUNT, 0);
    REGIST_PARAM_TYPE(UINT, VULKAN_DEVICE_COMPUTE_QUEUE_COUNT, 1);
    REGIST_PARAM_TYPE(UINT, VULKAN_DEVICE_TRANSFER_QUEUE_COUNT, 0);
    REGIST_PARAM_TYPE(UINT, VULKAN_DEVICE_UBER_QUEUE_COUNT, 1);
    REGIST_PARAM_TYPE(STRING, VULKAN_DEVICE_GFX_QUEUE_PRIOR, "");
    REGIST_PARAM_TYPE(STRING, VULKAN_DEVICE_COMPUTE_QUEUE_PRIOR, "");
    REGIST_PARAM_TYPE(STRING, VULKAN_DEVICE_TRANSFER_QUEUE_PRIOR, "");
    REGIST_PARAM_TYPE(STRING, VULKAN_DEVICE_UBER_QUEUE_PRIOR, "");
    REGIST_PARAM_TYPE(BOOL, VULKAN_DEVICE_DESCRIPTOR_INDEX_EXT, false);
    REGIST_PARAM_TYPE(BOOL, VULKAN_DEVICE_BUFFER_ADDRESS_EXT, false);
    REGIST_PARAM_TYPE(BOOL, VULKAN_DEVICE_MEMORY_BUDGET, true);
    REGIST_PARAM_TYPE(BOOL, VULKAN_DEVICE_MEMORY_REQUIRE, true);
    REGIST_PARAM_TYPE(BOOL, VULKAN_DEVICE_DEDICATED_ALLOC, true);
    //todo other config params REGIST_PARAM_TYPE(BOOL, DEVICE_, true);

    HALGlobalEntityVulkan* HALGlobalEntityVulkan::Instance()
    {
        static HALGlobalEntityVulkan global_entity;
        return &global_entity;
    }

    void HALGlobalEntityVulkan::Init()
    {
        //initialize volk
        if (volkInitialize()==VK_SUCCESS) {

            instance_ = VulkanInstance::Create();
            device_ = VulkanDevice::Create(instance_);

            //create bindless heap 
                if (GET_PARAM_TYPE_VAL(BOOL, BINDLESS_TABLE_ENABLE))
                {
                    bindless_heap_.reset(new HALResourceBindlessSetVulkan);
                    const auto& bindless_initializer = HALBindLessTableInitializer::GetBindlessTableInitializer();
                    bindless_heap_->Init(bindless_initializer);
                }
        }
        else
        {
            LOG(ERROR) << "failed to load vulkan library";
        }
    }

    void HALGlobalEntityVulkan::UnInit()
    {
       
    }

    HALCommandContext* HALGlobalEntityVulkan::CreateCommandBuffer()
    {
        return static_cast<HALCommandContextVulkan*>(nullptr);
    }

    void HALGlobalEntityVulkan::Execute(Span<HALCommandContext*> cmd_buffers)
    {
        SmallVector<HALCommandContextVulkan*> gfx_cmds;
        SmallVector<HALCommandContextVulkan*> compute_cmds;
        for (auto& cmd : cmd_buffers) {

        }
        if (gfx_cmds.size()) {
            auto gfx_queue = device_->GetQueue(VK_QUEUE_GRAPHICS_BIT);
            gfx_queue->Submit(gfx_cmds);
        }
        if (compute_cmds.size()) {
            auto compute_queue = device_->GetQueue(VK_QUEUE_COMPUTE_BIT);
            compute_queue->Submit(compute_cmds);
        }
    }

    bool HALGlobalEntityVulkan::SetUpTexture(HALTextureVulkan* texture)
    {
        if (texture->GetImpl().get()) {
            return true;
        }

        const auto& texture_desc = texture->GetTextureDesc();

        if (texture_desc.is_transiant_) {
            assert(transient_repo_.get() != nullptr);
            bool ret = transient_repo_->AllocTexture(texture_desc, texture);
            return ret;
        }
        else
        {
            bool ret = pooled_repo_.AllocTexture(texture_desc, texture);
            return ret;
        }

        return true;

    }

    bool HALGlobalEntityVulkan::SetUpBuffer(HALBufferVulkan* buffer)
    {
        if (buffer->GetImpl().get()) {
            return true;
        }

        const auto& buffer_desc = buffer->GetBufferDesc();
        if (buffer_desc.is_transiant_) {
            assert(transient_repo_.get() != nullptr);
            return transient_repo_->AllocBuffer(buffer_desc, buffer);
        }
        else
        {
            return pooled_repo_.AllocBuffer(buffer_desc, buffer);
        }
        return true;
    }

    HALGlobalEntityVulkan::HALGlobalEntityVulkan() {

    }

    HALRenderPass::Handle HALGlobalEntityVulkan::CreatePass(const HALRenderPassInitializer& pass_initializer)
    {
        return HALRenderPassRepoVulkan::Alloc(pass_initializer); //todo 
    }

    HALShaderLibraryInterface* HALGlobalEntityVulkan::GetOrCreateShaderLibrary()
    {
        static HALTypeConcreteTraits<EnumToInteger(EHALBackEnd::eVulkan)>::HALShaderLibraryInterface shader_library;
        return static_cast<HALShaderLibraryInterface*>(&shader_library);
    }

    HALPipelineStateObjectLibraryInterface* HALGlobalEntityVulkan::GetOrCreatePSOLibrary()
    {
        static HALTypeConcreteTraits<EnumToInteger(EHALBackEnd::eVulkan)>::HALPipelineStateObjectLibraryInterface pso_library;
        static std::atomic_bool is_inited{ false }; //todo
        if (!is_inited) {
            pso_library.Init();
            is_inited.exchange(true);
        }
        return static_cast<HALPipelineStateObjectLibraryInterface*>(&pso_library);
    }

    HALMemoryResidencyManager* HALGlobalEntityVulkan::GetOrCreateMemoryResidencyManager()
    {
        static HALTypeConcreteTraits<EnumToInteger(EHALBackEnd::eVulkan)>::HALMemoryResidencyManager residency_manager{ instance_, device_ };
        return static_cast<HALMemoryResidencyManager*>(&residency_manager);
    }

#if defined(DEVELOP_DEBUG_TOOLS) && defined(ENABLE_IMGUI)
    HALImGuiLayerWrapper* HALGlobalEntityVulkan::GetImGuiLayerWrapper()
    {
        return static_cast<HALImGuiLayerWrapper*>(&imgui_wrapper_);
    }
#endif

    HALResourceBindlessHeap* HALGlobalEntityVulkan::GetResourceBindlessHeap()
    {
        //init heap in global entity's init function 
        if (GET_PARAM_TYPE_VAL(BOOL, BINDLESS_TABLE_ENABLE)) {
            assert(bindless_heap_.get() != nullptr);
            return bindless_heap_.get();
        }
        LOG(ERROR) << "bindless table not enabled";
    }


}