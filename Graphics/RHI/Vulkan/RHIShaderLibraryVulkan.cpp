#include "Core/EngineGlobalParams.h"
#include "Utils/CommonUtils.h"
#include "RHI/Vulkan/RHIShaderVulkan.h"
#include "RHI/Vulkan/RHIShaderLibraryVulkan.h"
#include "RHI/Vulkan/API/VulkanRenderPipeline.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using Path = std::filesystem::path;

namespace Shard::RHI::Vulkan {

	REGIST_PARAM_TYPE(STRING, VULKAN_PSO_CACHE_PATH, ""); //todo

	class PipelineStateObjectCacheVulkan
	{
	public:
		enum {
			eLow = 0x0,
			eMedium = 0x1,
			eHigh = 0x2,
			eNum,
			eMask = eLow | eMedium | eHigh,
		};
		void Serialize() {
			VkPipelineCacheCreateInfo create_info{}; //zero it here ?? fixme
			eastl::unique_ptr<char[]> buffer;
			if (fs::exists(path_.c_str()))
			{
				std::ifstream cache_file(path_.c_str(), std::ios::binary);
				cache_file.seekg(0, std::ios::end);
				const auto size = cache_file.tellg();
				buffer.reset(new char[size]);
				cache_file.seekg(0, std::ios::beg);
				create_info.initialDataSize = size;
				create_info.pInitialData = buffer.get();
			}
			auto ret = vkCreatePipelineCache(GetGlobalDevice(), &create_info, g_host_alloc, &rhi_cache_);
			PCHECK(ret == VK_SUCCESS);
		}
		void UnSerialize() {
			if (!rhi_cache_) {
				return;
			}
			size_t cache_size{ 0 };
			vkGetPipelineCacheData(GetGlobalDevice(), rhi_cache_, &cache_size, nullptr);
			if (cache_size > 0) {
				eastl::unique_ptr<char[]> buffer{ new char[cache_size] };
				vkGetPipelineCacheData(GetGlobalDevice(), rhi_cache_, &cache_size, buffer.get());
				std::ofstream cache_file(path_.c_str(), std::ios::binary);
				cache_file.write(buffer.get(), cache_size);
			}

		}
		explicit PipelineStateObjectCacheVulkan(uint32_t flags=eLow) : flags_(flags) {
			const auto& dir = GET_PARAM_TYPE_VAL(STRING, VULKAN_PSO_CACHE_PATH);
			const String pso_names[] = { "Low", "Medium", "High" };
			const String pso_file = pso_names[flags & eMask];
			path_ = String((Path(dir) / fmt::format("{}_Vulkan_Cache.psocache", pso_file.c_str())).c_str());
		}
		~PipelineStateObjectCacheVulkan() {
			if (rhi_cache_ != nullptr) {
				vkDestroyPipelineCache(GetGlobalDevice(), rhi_cache_, g_host_alloc);
			}
		}
		RHIPipelineStateObject::Ptr CreatePipelineStateObject(const RHIPipelineStateObjectInitializer& initializer) {
			return new RHIPipelineStateObjectVulkan(initializer);
		}
		uint32_t GetFlags() const { 
			return flags_;
		}
	private:
		String	path_;
		uint32_t	flags_{ eLow };
		VkPipelineCache rhi_cache_{ VK_NULL_HANDLE };
	};

	void RHIPipelineStateObjectLibraryVulkan::Init()
	{
		pso_caches_.clear();
		for (auto n = 0; n < PipelineStateObjectCacheVulkan::eNum; ++n) {
			auto* new_cache = new PipelineStateObjectCacheVulkan(n);
			new_cache->Serialize();
			pso_caches_.push_back(new_cache);
		}
	}

	RHIPipelineStateObjectLibraryVulkan::~RHIPipelineStateObjectLibraryVulkan()
	{
		for (auto cache : pso_caches_) {
			cache->UnSerialize();
			delete cache;
		}
	}

	RHIPipelineStateObject::Ptr RHIPipelineStateObjectLibraryVulkan::CreatePipelineImpl(const RHIPipelineStateObjectInitializer& initializer)
	{
		auto iter = eastl::find(pso_caches_.begin(), pso_caches_.end(), [initializer](auto cache) {const auto flags = cache->GetFLags(); return initializer.reserved_flags_&flags == flags });
		assert(iter != pso_caches_.end());
		return (*iter)->CreatePipelineStateObject(initializer);
	}
	
	VkPipelineColorBlendStateCreateInfo* BlendStateVulkanCreator::Create(const RHIBlendStateInitializer& initializer)
	{
		auto* blend_state = new VkPipelineColorBlendStateCreateInfo;
		memset(blend_state, 0, sizeof(*blend_state));
		blend_state->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		const auto attach_num = initializer.attachments_.size();
		blend_state->attachmentCount = attach_num;
		auto* rhi_attachs = new VkPipelineColorBlendAttachmentState[attach_num];
		const auto trans_blend_factor = [](RHIBlendStateInitializer::BlendAttachment::EBlendFactor factor) { return VkBlendFactor(); };
		const auto trans_blend_op = [](RHIBlendStateInitializer::BlendAttachment::EBlendOperation op) { return VkBlendOp(); };
		const auto trans_color_wmask = [](RHIBlendStateInitializer::BlendAttachment::EBlendColorMask mask) { return VkColorComponentFlags(); };

		for (auto n = 0; n < attach_num; ++n) {
			auto& dst_attach = rhi_attachs[n];
			const auto src_attach = initializer.attachments_[n];
			dst_attach.srcAlphaBlendFactor = trans_blend_factor(src_attach.alpha_src_factor_);
			dst_attach.dstAlphaBlendFactor = trans_blend_factor(src_attach.alpha_dst_factor_);
			dst_attach.srcColorBlendFactor = trans_blend_factor(src_attach.color_src_factor_);
			dst_attach.dstColorBlendFactor = trans_blend_factor(src_attach.color_dst_factor_);
			dst_attach.alphaBlendOp = trans_blend_op(src_attach.alpha_blend_op_);
			dst_attach.colorBlendOp = trans_blend_op(src_attach.color_blend_op_);
			
			dst_attach.colorWriteMask = trans_color_wmask(src_attach.color_write_mask_);
		}
		blend_state->pAttachments = rhi_attachs;
		return blend_state;
	}
	void BlendStateVulkanCreator::Destroy(VkPipelineColorBlendStateCreateInfo* blend_state)
	{
		delete blend_state->pAttachments;
		delete blend_state;
	}
	VkPipelineRasterizationStateCreateInfo* RasterizationStateVulkanCreator::Create(const RHIRasterizationStateInitializer& initializer)
	{
		auto* raster_state = new VkPipelineRasterizationStateCreateInfo;
		memset(raster_state, 0, sizeof(*raster_state));
		raster_state->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		//todo
		return raster_state;
	}
	void RasterizationStateVulkanCreator::Destroy(VkPipelineRasterizationStateCreateInfo* raster_state)
	{
		delete raster_state;
	}
	VkPipelineDepthStencilStateCreateInfo* DepthStencilStateVulkanCreator::Create(const RHIDepthStencilStateInitializer& initializer)
	{
		auto* ds_state = new VkPipelineDepthStencilStateCreateInfo;
		const auto trans_compare_op = [](RHIDepthStencilStateInitializer::EDepthCompareOp op) { return VkCompareOp(); };
		const auto trans_stentil_op = [](RHIDepthStencilStateInitializer::EStencilOp op) { return VkStencilOpState(); };
		memset(ds_state, 0, sizeof(*ds_state));

		return ds_state;
	}
	void DepthStencilStateVulkanCreator::Destroy(VkPipelineDepthStencilStateCreateInfo* ds_state)
	{
		delete ds_state;
	}
}