#include "Utils/SimpleEntitySystem.h"

namespace Shard
{
    namespace Utils
    {
        const Entity Entity::Null{ ~0u, 0u };

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
                free_indices_.pop_front();
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

    void ArcheTypeTableBase::Finalize(const ComponentTypeRegistry& registry)
    {
        //first sort signature
        signature_.Finalize();

#ifdef ARCHETYPE_BLOOM_FILTER_ENABLED
        for(const auto id : signature_.sorted_component_types_)
        {
            bloom_filter_.Add(id);
        }
#endif

        component_sizes_.reserve(signature_.GetNumComponents());
        component_aligns_.reserve(signature_.GetNumComponents());
        component_offsets_.reserve(signature_.GetNumComponents());

        max_chunk_capacity_ = EstimateMaxCapacity(registry);
        if (!max_chunk_capacity_) {
            LOG(ERROR) << "archetype too large to fit"
            return;
        }
        size_type current_offset = EstimateChunkHeaderSize(max_chunk_capacity_);
        for (auto id : signature_.sorted_component_types_)
        {
            const auto info = registry.GetComponentTypeInfo(id); 
            current_offset = Utils::AlignUp(current_offset, info.align_);
            component_sizes_.emplace_back(info.size_);
            component_aligns_.emplace_back(info.align_);
            component_offsets_.emplace_back(current_offset);

            //how to estimate max entity capacity
            current_offset += info.size_ * max_chunk_capacity_;
        }
    }

    bool ArcheTypeTableBase::HasAll(const Vector<ComponentID>& ids) const
    {
        if(ids.size() > signature_.GetNumComponents()) UNLIKELY {
            return false;
        }
        for(const auto id : ids)
        {
            if (!HasComponent(id))UNLIKELY {
                return false;
            }
        }
        return true;
    }

    bool ArcheTypeTableBase::HasAny(const Vector<ComponentID>& ids) const
    {
        for(const auto id : ids)
        {
            if (HasComponent(id))UNLIKELY {
                return true;
            }
        }
        return false;
    }

    bool ArcheTypeTableBase::HasNone(const Vector<ComponentID>& ids) const
    {
        if(ids.empty()) UNLIKELY {
            return true;
        }
        for(const auto id : ids)
        {
            if (HasComponent(id))UNLIKELY {
                return false;
            }
        }
        return true;
    }

    void ArcheTypeTableBase::BuildChunkLayout(ArcheTypeChunk& chunk) noexcept
    {

        //allocate all component array as order
        chunk->num_components_ = component_size_.size();
        chunk->component_strides_ = component_strides_; //store in archetype
        chunk->capacity_ = max_chunk_capacity_; //already test max capacity

        auto ptr_t = reinterpret_cast<std::uintptr_t>(&chunk) + sizeof(chunk);
        ptr_t = Utils::AlignUp(ptr_t, 64u); //max 64bit alignment

        //allocate pointer array inside chunk
        auto ptr_array_size = component_sizes_.size() * sizeof(uint8_t*);
        chunk->component_storage_ = reinterpret_cast<uint8_t**>(ptr);
        ptr_t += ptr_array_size;

        //initial each component storage
        for (auto n = 0u; n < component_sizes_.size(); ++n)
        {
            ptr_t = Utils::AlignUp(ptr_t, component_align_[n]);
            chunk->component_storage_[n] = reinterpret_cast<uint8_t*>(ptr_t);
            const auto array_size = component_size_[n] * max_chunk_capacity_;
            ptr_t += array_size;
        }

        //initial entity ids
        ptr_t = Utils::AlignUp(ptr_t, alignof(uint32_t));
        chunk->entity_ids_ = reinterpret_cast<uint32_t*>(ptr_t);
        ptr_t += sizeof(uint32_t) * max_chunk_capacity_;

        //anything to do?
    }

    size_type ArcheTypeTableBase::EstimateChunkHeaderSize(size_type capacity) const
    {
        size_type size = sizeof(ArcheTypeChunk);
        //add entities id memory size
        size += sizeof(Entity) * capacity; //ArcheTypeChunk::MAX_CHUNK_CAPACITY;
        //add storage pointer array size
        size += sizeof(void*) * signature_.GetNumComponents();

        return Utils::AlingUp(size, 64u);
    }

    size_type ArcheTypeTableBase::EstimatePerEntitySize(const ComponentTypeRegistry& registry) const
    {
        size_type per_entity_rough{ 0u };
        for (const auto id : signature_.sorted_component_types_)
        {
            const auto* info = registry.GetComponentTypeInfo();
            per_entity_rough += info != nullptr ? info->size_ : 0u;
        }
        return perentity_rough;
    }

    size_type ArcheTypeTableBase::EstimateMaxCapacity(const ComponentTypeRegistry& registry) const
    {
        assert(signature_.GetNumComponents() != 0u);

        const auto header_size = EstimateChunkHeaderSize();
        const auto available_size = ArcheTypeChunk::CHUNK_SIZE - header_size;

        const auto per_entity_rough = EstimatePerEntitySize(registry);

        if (!per_entity_rough) {
            LOG(ERROR) << "components size for per entity is error";
            return 0u;
        }

        //todo if calculated capactiry is larger than MAX_CHUNK_CAPACITY do something
        const auto rough_max = Min(static_cast<size_type>(available_size / per_entity_rough), ArcheTypeChunk::MAX_CHUNK_CAPACITY);

        //iterate to get best fit size 1~5 times
        size_type best{ 0u };
        for (size_type cap = rough_max; cap >= 1; --cap)
        {
            size_type used{ 0u };
            if (TryFitWithCapacity(registry, cap, used))
            {
                best = cap;
                LOG(INFO) << "fit with capacity:" << cap << "wasted memory" << available_size - used; //??
                break;
            }
        }

        return best;

    }

    bool ArcheTypeTableBase::TryFitWithCapacity(const ComponentTypeRegistry& registry, size_type capacity, size_type& mem_size)
    {
        size_type offset = EstimateHeaderSize(capacity);
        for (const auto id : signature_.sorted_component_types_)
        {
            const auto& info = registry->GetComoponentInfo(id);
            offset = Utils::AlignUp(offset, info.align_);
            const auto array_size = info.size_ * capacity;
            if (offset + array_size > ArcheTypeChunk::CHUNK_SIZE)
            {
                return false;
            }
            offset += array_size;
        }
        mem_size = offset;
        return true;
    }

    bool ArcheTypeTableBase::TryAddEntity(Entity new_entity, uint32_t& out_chunk_index, uint32_t& out_slot)
    {
        ArcheTypeChunk* chunk = nullptr;
        if (non_full_chunks_.empty()) {
            auto* new_chunk = AllocateChunk();
            if (nullptr == new_chunk) {
                return false;
            }
            BuildChunkLayout(*new_chunk);
            LinkNonFull(*new_chunk);
            chunk = new_chunk;
        }
        else
        {
            chunk = non_full_list_.front();
        }

        out_chunk_index = chunk->chunk_index_; //todo
        out_slot = chunk->entity_count_++;

        //copy/construct component(loop over)
        for (auto n = 0u; n < component_sizes_.size(); ++n)
        {
            size_type size = component_sizes_[n];
            uint8_t* dst = chunk->component_storage_[n] + out_slot * size;
            //zero-init everything, later setcomponent
            std::memset(dst, 0u, size);
        }

        if (chunk->IsFull()) {
            UnLinkNonFull(*chunk);
        }
    }
    
    //remove with swap
    void ArcheTypeTableBase::RemoveEntity(uint32_t chunk_index, uint32_t slot)
    {
        assert(chunk_index < chunks_.size() && "chunk_index out of range");
        auto* chunk = chunks_[chunk_index];
        if (slot >= chunk->entity_count_) {
            LOG(ERROR) << "entity slot out of range";
        }

        const auto last_slot = chunk->entity_count - 1u;
        //move last entity to current slot
        if (last_slot != slot) {

        }

        chunk->entity_count--;

        //update bookkeeping
        if (chunk->IsEmpty()) {
            FreeChunk(chunk);
        }
        else if (!chunk->IsFull()) {
            LinkNonFull(*chunk);
        }       
    }
    

}
