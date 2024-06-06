#pragma once
#include "HAL/Vulkan/API/VulkanHAL.h"
#include "HAL/Vulkan/API/VulkanResources.h"
#include "HAL/Vulkan/API/VulkanRenderShader.h"
#include "HAL/Vulkan/API/VulkanFrameBuffer.h"
#include <unordered_map>

namespace Shard
{
    class VulkanCmdBuffer;
    class VulkanDescriptorSet;
    class VulkanRenderPipeline
    {
    public:
        //todo: support task and mesh shader
        enum class EPipeType:uint8_t
        {
            eCompute,
            eGFX,
            eRayTrace,
            eNum,
        };
        typedef struct _PiplineSetLayoutCreateInfo
        {

        }RootSignature;

        typedef struct _PipelineCreateInfo
        {
            EPipeType    pipe_type_{ EPipeType::eGFX };
            union {
                VkGraphicsPipelineCreateInfo    gfx_;
                VkComputEPipelineCreateInfo    compute_;
                VkRayTracingPipelineCreateInfoKHR    raytrace_;
            };
            RootSignature    root_signatue_;
        }Desc;
        using Ptr = VulkanRenderPipeline*;
        VulkanRenderPipeline() = default;
        explicit VulkanRenderPipeline(VulkanDevice* device, const Desc& desc_params);
        virtual void Init(VulkanDevice* device, const Desc& desc_params) = 0;
        //pipeline resource 
        void Bind(VulkanCmdBuffer& cmd_buffer);
        //constant data 
        void PushConsts(VulkanCmdBuffer& cmd_buffer, const uint32_t stage, const uint32_t offset, const Span<uint8_t>& data);
        VkPipeline Get() { return handle_; }
        const VkPipelineLayout GetLayout()const { return layout_; }
        EPipeType Type()const { return pipe_type_; }
        VulkanDescriptorSet& operator[](const std::string& set_name); 
        VulkanRenderPipeline(VulkanDevice* device, const Desc& desc, EPipeType pipe_type);
        virtual ~VulkanRenderPipeline();
    private:
        void InitialDescSetLayouts(const RootSignature& root);
    protected:
        using ShaderList = Vector<VulkanShaderModule*>;
        using DescSetList = Vector<VulkanDescriptorSet>;
        VulkanDevice*                device_;
        VkPipeline                        handle_{ VK_NULL_HANDLE };
        EPipeType                        pipe_type_;
        VkPipelineLayout                layout_{ VK_NULL_HANDLE };
        Vector<VkDescriptorSetLayout>    ds_layouts_;
        VkDescriptorUpdateTemplate        desc_template_{ VK_NULL_HANDLE };
        DescSetList                        descs_;
        //ShaderList                        stages_;
    };

    
    class VulkanComputEPipeline : public VulkanRenderPipeline
    {
    public:
        VulkanComputEPipeline() = default;
        explicit VulkanComputEPipeline(VulkanDevice* device, const VulkanRenderPipeline::Desc& param);
        void Init(VulkanDevice* device, const VulkanRenderPipeline::Desc& param) override;
    };

    class VulkanGraphicsPipeline : public VulkanRenderPipeline
    {
    public:
        VulkanGraphicsPipeline() = default;
        explicit VulkanGraphicsPipeline(VulkanDevice* device, const VulkanRenderPipeline::Desc& param);
        void Init(VulkanDevice* device, const VulkanRenderPipeline::Desc& param) override;
        VulkanShaderModule* GetShader(VulkanShaderModule::EType shader_type);
        //bind vertex info
        void SetVAO(const VulkanVertexAttributes& vao);
        //dynamic state 
        void SetViewPoint(const VkViewport& view_point);
        void SetStencil(const uint32_t stentil_ref);
        void SetScissor(const VkRect2D& scissor);
        //init render pass and etc.
        void PrepareForDraw(VulkanCmdBuffer& cmd_buffer, VulkanFrameBuffer& frame_buffer);
        void EndForDraw(VulkanCmdBuffer& cmd_buffer);
    private:
        //only initial when pipeline has graph viewpoint info
        Optional<VkViewport>        view_point_;
        Optional<uint32_t>            stencil_ref_;
        Optional<VkRect2D>            scissor_;
        VulkanFrameBuffer            frame_buffer_;
        //fixed-function vertex process data

    };

    class VulkanRayTracingPipeline : public VulkanRenderPipeline
    {
    public:
        VulkanRayTracingPipeline() = default;
        explicit VulkanRayTracingPipeline(VulkanDevice* device, const VulkanRenderPipeline::Desc& param);
        void Init(VulkanDevice* device, const VulkanRenderPipeline::Desc& param) override;
    };

    VkPipelineCacheCreateInfo MakEPipelineCacheCreateInfo(const Span<uint8_t>& initial_data);
}
