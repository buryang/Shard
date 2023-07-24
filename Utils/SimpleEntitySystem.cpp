#include "Utils/SimpleEntitySystem.h"

namespace Shard
{
	namespace Utils
	{
		const Entity Entity::Null{ 0xFFFFF, 0u };

		Entity EntityManager::Generate()
		{
			if (free_indices_.size() > 0u)
			{
				auto index = free_indices_.front();
				free_indices_.pop();
				++generation_[index];
				return {index, generation_[index]};
			}
			else
			{
				auto index = generation_.size();
				generation_.push_back(0u);
				return { index, 0u };
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
			if (entity.Index() < generation_.size()) {
				return entity.Generation() == generation_[entity.Index()];
			}
			return false;
		}

		const Vector<Entity>& EntityManager::GetAliveEntities() const
		{
			Vector<Entity> alives;
			//todo 
			return alives;
		}

		EntityManager::VersionType EntityManager::Version(const Entity& entity) const
		{
			if (entity.Index() < generation_.size()) {
				return generation_[entity.Index()];
			}
			return VersionType(std::numeric_limits<VersionType>::max);
		}

		ComponentInterface::Instance ComponentInterface::Insert(Entity entity)
		{
			auto iter = id_map_.find(entity);
			if (iter != id_map_.end())
			{
				auto index = id_map_.size();
				id_map_.insert(eastl::make_pair(entity, index));
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
