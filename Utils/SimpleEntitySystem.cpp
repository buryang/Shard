#include "Utils/SimpleEntitySystem.h"

namespace MetaInit
{
	namespace Utils
	{
		Entity EntityManager::Generate()
		{
			if (free_indices_.size() > MIN_FREE_INDICES)
			{
				auto index = free_indices_.front();
				free_indices_.();
				return {};
			}
			else
			{
				auto index = generation_.size();
				generation_.push_back(0);
				return { index, 0 };
			}
		}

		void EntityManager::Release(Entity entity)
		{
			const auto index = entity.Index();
			++generation_[index];
			free_indices_.push_back(index);
		}
		
		bool EntityManager::IsAlive(const Entity& entity) const
		{
			return entity.Generation() == generation_[entity.Index()];
		}

		ComponentInterface::Instance ComponentInterface::Insert(Entity entity)
		{
			auto iter = id_map_.find(entity);
			if (iter != id_map_.end())
			{
				auto index = id_map_.size();
				id_map_.insert(std::make_pair(entity, index));
			}
			return { iter->second };
		}

		ComponentInterface::Instance ComponentInterface::LookUp(Entity entity) const
		{
			if( id_map_.find(entity) != id_map_.end())
				return { id_map_[entity] };
			return { -1 };
		}

		void ComponentInterface::GC(const EntityManager& entity_manager)
		{
			uint32_t alive_in_row = 0;
			while (entity_manager.free_indices_.size() > EntityManager::MIN_FREE_INDICES)
			{
				//todo 
			}
			Destroy();
		}

		void* ComponentInterface::Alloc(size_t size)
		{
			return allocator_->Alloc(size);
		}
	}
}
