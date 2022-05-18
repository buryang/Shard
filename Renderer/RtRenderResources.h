#pragma once
#include <unordered_map>
#include "Utils/CommonUtils.h"
#include "RHI/RHI.h"

namespace MetaInit
{
	namespace Renderer
	{

		class RtField
		{
		public:
			enum class EType : uint8_t
			{
				eBuffer,
				eTexture,
			};

			enum class EState : uint32_t
			{
				eCommon = 0x0,
				eVertex = 0x1,
				eIndexBuffer = 0x2,
				eCopySrc = 0x4,
				eCopyDst = 0x8,
				eResolveDst = 0x9,
				eSRV = 0x10,
				eRTV = 0x11,
				eUAV = 0x12,
				ePresent = 0x13
			};

			static bool IsStateMergeAble(EState state, EState other);
			enum class EUsage
			{
				eInput,
				eOutput,
				eInternal,
			};

			enum class EFlags : uint8_t
			{
				eNone,
				eOptional,
				ePresent,
			};

			using Ptr = std::shared_ptr<RtField>;
			using Handle = std::pair<Ptr, uint32_t>;
			uint32_t GetFLags()const;
			bool IsWholeResource()const { return sub_resources_.size() == 1; }
			bool IsMergeAble(const RtField& other)const;
			bool IsTransiantAble(const RtField& dst)const;
			RtField& Name(const std::string& name);
			RtField& Width(const uint32_t width);
			RtField& Height(const uint32_t height);
			RtField& Depth(const uint32_t depth);
			RtField& MipSlices(const uint32_t mip_slices);
			RtField& ArraySlices(const uint32_t array_slices);
			RtField& PlaneSlices(const uint32_t plane_slices);
			uint32_t GetSubFieldCount()const;
			friend bool operator==(const RtField& lhs, const RtField& rhs);
			friend bool operator!=(const RtField& lhs, const RtField& rhs) { return !(lhs == rhs); }
		private:
			friend class RtRendererPass;
			std::string					name_;
			EType						type_;
			EUsage						usage_;
			EState						state_;
			uint32_t					sample_count_{ 1 };
			uint32_t					width_{ 0 };
			uint32_t					height_{ 0 };
			uint32_t					depth_{ 0 };
			uint32_t					mip_slices_{ 1 };
			uint32_t					array_slices_{ 1 };
			uint32_t					plane_slices_{ 1 };
			struct SubTextureFieldRange
			{
				uint32_t	base_mip_{ 0 };
				uint32_t	mip_count_{ 1 };
				uint32_t	base_layer_{ 0 };
				uint32_t	layer_count_{ 1 };
				uint32_t	base_plane_{ 0 };
				uint32_t	plane_count_{ 1 };
			};
			SmallVector<SubTextureFieldRange>	sub_resources_;
		};


		class RtBarrierBatch
		{
		public:
			using Ptr = std::shared_ptr<RtBarrierBatch>;
			void Submit(RHI::RHICommandContext& context);
		private:
			SmallVector<RHI::RHIBarrier>	barriers_;
		};

		/*warper for rhi resource*/
		template <class ResourceHandle>
		class RtRenderResource
		{
		public:
			using Ptr = std::shared_ptr<RtRenderResource<ResourceHandle> >;
			enum class _InitialState
			{
				eUnkown,
				eClear,
				eLoad,
				eCount,
			}InitState;

			class RtSubResourceState
			{

			};

			RtRenderResource() = default;
			RtRenderResource(const RtField& field);
			~RtRenderResource();
			bool IsTransient()const;
		private:
			ResourceHandle						handle_;
			std::pair<uint32_t, uint32_t>		life_time_;
			bool								imported_{ false };
			bool								is_transient_{ true };
			InitState							init_state_;
		};

		//resource manage handle
		using RtBufferHandle = std::pair<RtRenderResource<void>::Ptr, SmallVector<RtField> >;
		using RtTextureHandle = std::pair<RtRenderResource<void>::Ptr, SmallVector<RtField> >;

		template <class ResourceHandle>
		class RtRenderResourcePool
		{
		public:
			void CollectAllResources();
			void RegistResource(const RtRenderResource<ResourceHandle>& resource);
			ResourceHandle GetResource(RtRenderResource<ResourceHandle>& resource);
			void Clear(uint32_t curr_frame_index);
		private:
			void DoAliasingProcess();
		private:
			std::unordered_map<uint32_t, ResourceHandle>	resource_repo_;
			//resource regist
			Vector<RtRenderResource<ResourceHandle> >		resource_all_;
			//resource map
			std::unordered_map<RtField, uint32_t>			resource_map_;
		};



		/*begin every pass, deal resource's lift, new or del one*/
		class RtTransiantResourceWatchDog
		{
		public:
			void RegistResource();
			//do the resource life time opt
			void CollectAllResources();
			void ExecutePass(const uint32_t pass_index);
			~RtTransiantResourceWatchDog();
		private:
			std::unordered_map<uint32_t, RtRenderResource> transiants_;
		};


		class RtRenderResourceManager
		{
		public:
			struct ResourceProxy
			{

			};
			template<typename T>
			struct ResourceData : public ResourceProxy
			{
				T* resource_{ nullptr };
			};
			ResourceProxy CreateOrGet();
		private:
			std::unordered_map<uint32_t, ResourceProxy>	resources_;
		};
	}
}

#include "Renderer/RTRenderResources.inl"
