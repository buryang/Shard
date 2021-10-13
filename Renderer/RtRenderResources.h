#pragma once
#include <unordered_map>

namespace MetaInit
{
	namespace Render
	{

		class RtField;
		/*warper for rhi resource*/
		template <class ResourceHandle>
		class RtRenderResource
		{
		public:
			enum class _InitialState
			{
				eUnkown,
				eClear,
				eLoad,
				eCount,
			}InitState;
			RtRenderResource() = default;
			~RtRenderResource();
			bool IsTransient()const;
			RtRenderResource& Merge(const RtRenderResource& other);
		private:
			ResourceHandle						handle_;
			std::pair<uint32_t, uint32_t>		life_time_;
			EResourceState						state_;
			bool								imported_{ false };
			bool								is_transient_{ true };
			InitState							init_state_;
		};

		template <class ResourceHandle>
		class RtRenderResourcePool
		{
		public:
			void AllocResources();
			void RegistResource(const RtRenderResource<ResourceHandle>& resource);
			ResourceHandle GetResource(RtRenderResouce<ResourceHandle>& resource);
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


		class RtRenderResourceBlackboard
		{
		public:
			//UE only use this for immutable resource
			template<typename ResourceType>
			const ResourceType& Get()const;
			template<typename ResourceType>
			ResourceType& Add();
			template<typename ResourceType>
			void Remove();
			template<typename ResourceType>
			static uint32_t TypeToIndex();
		private:
			std::unordered_map<uint32_t, void*>		blackboard_;
		};
	}
}

#include "Renderer/RTRenderResources.inl"
