#include "Utils/SimpleEntitySystem.h"

namespace Shard
{
    namespace Utils
    {
        const Entity Entity::Null{ 0xFFFFF, 0u };

        EntityManager::EntityManager(const EntityManager& other)
        {
            std::copy(other.generation_.cbegin(), other.generation_.cend(), generation_.begin());
            std::copy(other.free_indices_.cbegin(), other.free_indices_.cend(), free_indices_.begin());
        }

        EntityManager& EntityManager::operator=(const EntityManager& other)
        {
            std::copy(other.generation_.cbegin(), other.generation_.cend(), generation_.begin());
            std::copy(other.free_indices_.cbegin(), other.free_indices_.cend(), free_indices_.begin());
            return *this;
        }

        Entity EntityManager::Spawn()
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

        bool EntityManager::Insert(const Entity& entity)
        {
            const auto index = entity.Index();
            if (index >= generation_.size())
            {
                generation_.resize(index + 1u, 0u);
            }
            if (IsAlive(entity))
            {
                return false;
            }
            generation_[index] = entity.Generation();
            //remove from free list if in
            if (auto iter = eastl::find(free_indices_.begin(), free_indices_.end(), index); iter != free_indices_.end())
            {
                free_indices_.erase(iter);
            }
            return true;
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

        void ECSSystemGroup::Init()
        {
            Sort();//first sort system 
            for (auto* sys : sub_sys_) {
                sys->Init();
            }
        }

        void ECSSystemGroup::UnInit()
        {
            for (auto* sys : sub_sys_) {
                sys->UnInit();
            }
        }

        void ECSSystemGroup::Update(ECSSystemUpdateContext& ctx)
        {
            //todo sort by each phase?
        }

        bool ECSSystemGroup::IsPhaseUpdateBefore(const ECSSystem& other, EUpdatePhase phase)const
        {
            if (IsPhaseUpdateEnabled(phase)) {
                return sub_sys_.back()->IsPhaseUpdateBefore(other, phase);
            }
            return false;
        }

        bool ECSSystemGroup::IsPhaseUpdateEnabled(EUpdatePhase phase)const
        {
            for (auto sub_sys : sub_sys_) {
                if (sub_sys->IsPhaseUpdateEnabled(phase)) {
                    return true;
                }
            }
            return false;
        }
        void ECSSystemGroup::AddSubSystem(ECSSystem* sub_system)
        {
            sub_sys_.push_back(sub_system);
        }
        void ECSSystemGroup::RemoveSubSystem(ECSSystem* sub_system)
        {
            if (auto iter = eastl::find(sub_sys_.begin(), sub_sys_.end(), sub_system); iter != sub_sys_.end())
            {
                sub_sys_.erase(iter);
            }
        }
        ECSSystemGroup::~ECSSystemGroup()
        {
            for (auto* sys : sub_sys_) {
                delete sys;
            }
            sub_sys_.clear();
        }
        bool ECSSystemGroup::IsSubSystemIncluded(ECSSystem* sub_system)
        {
            if (auto iter = eastl::find(sub_sys_.begin(), sub_sys_.end(), sub_system); iter != sub_sys_.end()) {
                return true;
            }
            return false;
        }
        void ECSSystemGroup::Sort()
        {
            //todo sort by what?
            eastl::sort(sub_sys_.begin(), sub_sys_.end(), [](auto* lhs, auto* rhs) { return true; });
        }
    }
}
