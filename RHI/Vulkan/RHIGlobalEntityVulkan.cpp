#include "RHI/Vulkan/RHICommandVulkan.h"
#include "RHI/Vulkan/RHIResourcesVulkan.h"
#include "RHI/Vulkan/RHIGlobalEntityVulkan.h"
#include "RHI/Vulkan/RHIShaderLibraryVulkan.h"
#include "RHI/RHITypeTraits.h"

#define ADD_EXT_IF(CONDITION, EXT_NAME) if (CONDITION) { extensions.emplace_back(EXT_NAME); }
#define ADD_LAYER_IF(CONDITION, LAYER_NAME) if (CONDITION) { layers.emplace_back(LAYER_NAME); }

namespace MetaInit::RHI::Vulkan {
	
	REGIST_GLOBAL_ENTITY(RHIGlobalEntityVulkan, ERHIBackEnd::eVulkan, [](void){ return static_cast<RHIGlobalEntity::Ptr>(RHIGlobalEntityVulkan::Instance()); });

	//instance and device extension config
	REGIST_PARAM_TYPE(BOOL, INSTANCE_DEBUG_REPROT_EXT, true);
	REGIST_PARAM_TYPE(BOOL, INSTANCE_DEBUG_UTILS_EXT,  true);
	REGIST_PARAM_TYPE(BOOL, DEVICE_MEMORY_BUDGET, true);
	REGIST_PARAM_TYPE(BOOL, DEVICE_MEMORY_REQUIRE, true);
	REGIST_PARAM_TYPE(BOOL, DEVICE_DEDICATED_ALLOC, true);

	RHIGlobalEntityVulkan::Ptr RHIGlobalEntityVulkan::Instance()
	{
		static RHIGlobalEntityVulkan global_entity;
		return &global_entity;
	}

	void RHIGlobalEntityVulkan::Init()
	{
		instance_ = VulkanInstance::Create();
		device_ = VulkanDevice::Create(instance_);
	}

	RHICommandContext::Ptr RHIGlobalEntityVulkan::CreateCommandBuffer()
	{
		return static_cast<RHICommandContextVulkan*>(nullptr);
	}

	void RHIGlobalEntityVulkan::Execute(Span<RHICommandContext::Ptr> cmd_buffers)
	{
		SmallVector<RHICommandContextVulkan::Ptr> gfx_cmds;
		SmallVector<RHICommandContextVulkan::Ptr> compute_cmds;
		for (auto& cmd : cmd_buffers) {

		}
		if (gfx_cmds.size()) {
			auto gfx_queue = GetVulkanDevice()->GetQueue();
			gfx_queue->Submit(gfx_cmds);
		}
		if (compute_cmds.size()) {
			auto compute_queue = GetVulkanDevice()->GetQueue();
			compute_queue->Submit(compute_cmds);
		}
	}

	bool RHIGlobalEntityVulkan::SetUpTexture(RHITextureVulkan::Ptr texture)
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

	bool RHIGlobalEntityVulkan::SetUpBuffer(RHIBufferVulkan::Ptr buffer)
	{
		if (buffer->GetImpl().get()) {
			return true;
		}

		const auto& buffer_desc = buffer->GetBufferDesc();
		if (buffer_desc.is_transiant_) {
			assert(transient_repo_.get() != nullptr);
			bool ret = transient_repo_->AllocBuffer(buffer_desc, buffer);
			return ret;
		}
		else
		{
			bool = pooled_repo_.AllocBuffer(buffer_desc, buffer);
			return ret;
		}
		return true;
	}

	RHIGlobalEntityVulkan::RHIGlobalEntityVulkan() {

	}

	void RHIGlobalEntityVulkan::InitInstance()
	{
	}

	RHIShaderLibraryInterface::Ptr RHIGlobalEntityVulkan::GetOrCreateShaderLibrary()
	{
		static RHITypeConcreteTraits<EnumToInteger(ERHIBackEnd::eVulkan)>::RHIShaderLibraryInterface shader_library;
		return static_cast<RHIShaderLibraryInterface::Ptr>(&shader_library);
	}

	RHIPipelineStateObjectLibraryInterface::Ptr RHIGlobalEntityVulkan::GetOrCreatePSOLibrary()
	{
		static RHITypeConcreteTraits<EnumToInteger(ERHIBackEnd::eVulkan)>::RHIPipelineStateObjectLibraryInterface pso_library;
		static std::atomic_bool is_inited{ false }; //todo
		if (!is_inited) {
			pso_library.Init();
			is_inited.exchange(true);
		}
		return static_cast<RHIPipelineStateObjectLibraryInterface::Ptr>(&pso_library);
	}

	RHIResourceBindlessHeap::SharedPtr RHIGlobalEntityVulkan::GetOrCreateResourceBindlessHeap()
	{
		return RHIResourceBindlessHeap::Ptr();
	}

	RHIResource::Ptr RHIGlobalEntityVulkan::CreateConstBuffer(const RHIBufferInitializer& desc)
	{
		RHIBufferVulkan::Ptr 
	}

}