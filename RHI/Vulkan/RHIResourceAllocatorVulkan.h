#pragma once
#include "Utils/CommonUtils.h"
#include "Core/RenderGlobalParams.h"
#include "RHI/RHIResources.h"
#include "RHI/Vulkan/API/VulkanRHI.h"

namespace MetaInit::RHI::Vulkan
{

	constexpr size_t RHI_PER_HEAP0_ALLOC_SIZE = 256;
	constexpr size_t RHI_PER_HEAP1_ALLOC_SIZE = 64; //total size limited on windows to 256MB
	constexpr size_t RHI_PER_HEAP2_ALLOC_SIZE = RHI_PER_HEAP0_ALLOC_SIZE;

	//set max pooled resource count
	REGIST_PARAM_TYPE(UINT, RHI_POOLED_MAX_RES, 256);
	REGIST_PARAM_TYPE(FLOAT, RHI_POOLED_TIME_GAP, 12);

	//only big texture need pooled big memory chunk
	class RHIPooledTextureAllocatorVulkan
	{
	public:
		RHIResource::Ptr Alloc(const RHITextureDesc& desc);
		void Free(RHIResource::Ptr resource);
		bool Tick(float time);
	private:
		struct PooledTexture
		{
			RHITextureDesc	texture_desc_;
			RHITexture::Ptr	handle_{ nullptr };
			uint32_t	last_stamp_{ 0 };
			operator RHIResource::Ptr() {
				return handle_;
			}
			uint32_t LastStamp()const {
				return last_stamp_;
			}
			~PooledTexture() {
				if (nullptr != handle_) {
					delete handle_;
				}
			}
		};
		List<PooledTexture>	pooled_repo_;
		float curr_time_stamp_{ 0 };
	private:
		bool FindSuitAbleResource(const RHITextureDesc& texture_desc, PooledTexture& ret);
	};

	enum class EHeapType : uint8_t
	{
		eUnkown,
		eHost,
		eHostVisible,
		
	};

	struct MemoryBulkDesc
	{
		uint32_t	size_{ 0 };
		EHeapType	type_{ EHeapType::eUnkown };
	};

	class RHIHeapMemoryBulkVulkan
	{
	public:
		struct Range {
			uint32_t	offset_: 16{0};
			uint32_t	size_ : 16{0};
			void*	meta_data_{ nullptr };
			FORCE_INLINE bool IsValid() const {
				return offset_ >= 0 && size_ > 0;
			}
			template<typename MetaType>
			MetaType* GetMeta() {
				return static_cast<MetaType*>(meta_data_);
			}
		};
		using BindCallBack = std::function<void(Range&)>;
		explicit RHIHeapMemoryBulkVulkan(const MemoryBulkDesc& desc);
		~RHIHeapMemoryBulkVulkan() { Release(); }
		bool Bind(RHIResource::Ptr resource, BindCallBack = nullptr);
		void Tick(float time);
		void Release();
		FORCE_INLINE size_t GetBulkSize() const {
			return capacity_;
		}
	private:
		bool FindSuitAbleFreeRange(uint32_t size, Range& range);
	private:
		const uint32_t	capacity_;
		/*<float:last_pass, Range:range>*/
		List<eastl::pair<float, Range>>	used_blks_;
		List<Range>	free_blks_;
		VkDeviceMemory	mem_bulk_{ VK_NULL_HANDLE };
	};

	class RHITransientResourceAllocatorVulkan
	{
	public:
		RHIResource::Ptr AllocTexture();
		RHIResource::Ptr AllocBuffer();
		void Tick(float time);
		~RHITransientResourceAllocatorVulkan();
	private:
		Map<EHeapType,SmallVector<RHIHeapMemoryBulkVulkan>>	mem_heaps_;
	};
}
