#pragma once
#include "Utils/CommonUtils.h"
#include "VulkanRHI.h"

namespace MetaInit
{
	class VulkanCmdPool
	{
	public:
		VulkanCmdPool() = default;
		DISALLOW_COPY_AND_ASSIGN(VulkanCmdPool);
		void Submit();
		void SubmitAsync();
		virtual ~VulkanCmdPool();
	private:
		VulkanDevice					device_;
		VkCommandPool				pool_ = VK_NULL_HANDLE;
		Vector<VkCommandBuffer>		buffers_;
	};

	class VulkanRenderPass;
	class VulkanCmdBuffer
	{
	public:
		struct VulkanCmdBufferParameters
		{

		};
		using Parameters = VulkanCmdBufferParameters;
		using Ptr = std::unique_ptr<VulkanCmdBuffer>;
		static Ptr Create(const Parameters& params, VulkanCmdPool& cmd_pool);
		~VulkanCmdBuffer();
		const Parameters& get_params()const { return parameters_; };
		VkCommandBuffer Get() {
			return buffer_;
		}
		/*cmd buffer state*/
		enum class State:uint32_t
		{
			INITIAL,
			RECORDING,
			EXECUTABLE,
			PENDING,
			INVALID,
			COUNT
		};
		void Begin();
		void End();
		void BeginPass(VulkanRenderPass& render_pass);
		void EndPass(VulkanRenderPass& render_pass);
		void Dispatch();
		void Draw();
		void Draw();
		void TraceRay();
		void Copy();
		void Submit(VulkanQueue& queue);
		void Reset();
	private:
		friend class VulkanCmdPool;
		VkCommandBuffer		buffer_ = VK_NULL_HANDLE;
		Parameters			parameters_;
		State				state_{State::INITIAL};
	};


}
