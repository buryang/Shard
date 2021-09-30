#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/VulkanPrimitive.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <memory>

namespace MetaInit
{
	namespace Renderer
	{
		class MINIT_API RtRenderResource
		{
		public:
			enum class EType
			{
				eBuffer,
				eTexture,
				eUnkown,
			};
			void AddReadPass(uint32_t pass);
			void AddWritePass(uint32_t pass);
			bool IsTransient()const;
			void SetTransient(bool value);
		private:
			std::unordered_set<uint32_t>	read_in_passes_;
			std::unordered_set<uint32_t>	write_in_passes_;
			bool							transient_;
			EType							type_;
		};

		class RtRenderPass;
		class MINIT_API RtRendererFrameContextGraph 
		{
		public:
			using Ptr = std::shared_ptr<RtRendererFrameContextGraph>;
			virtual ~RtRendererFrameContextGraph() = default;
			void LoadFromJson(const std::string& json);
			void ExportToJson(const std::string& json)const;
			void AddPass();
			RtRenderResource& GetTextureResource();
			RtRenderResource& GetBufferResource();
		private:
			DISALLOW_COPY_AND_ASSIGN(RtRendererFrameContextGraph);
			void InitRenderResource();
			void BuildSubTask();
		private:
			std::unordered_map<std::string, uint32_t>	resource_to_index_;
			std::unordered_map<std::string, uint32_t>	pass_to_index_;
			Vector<RtRenderResource>					resources_;
			SmallVector<RtRendererPass>					passes_;

			//physical resources
			Vector<Primitive::VulkanBuffer::Ptr>		physical_buffers_;
			Vector<Primitive::VulkanImage::Ptr>			physical_images_;
		};
	}
}
