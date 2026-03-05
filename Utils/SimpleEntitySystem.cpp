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

        void SystemGroup::Init()
        {
            Sort();//first sort system 
            for (auto* sys : sub_sys_) {
                sys->Init();
            }
        }

        void SystemGroup::UnInit()
        {
            for (auto* sys : sub_sys_) {
                sys->UnInit();
            }
        }

        void SystemGroup::Update(SystemUpdateContext& ctx)
        {
            //todo sort by each phase?
        }

        bool SystemGroup::IsPhaseUpdateBefore(const System& other, EUpdatePhase phase)const
        {
            if (IsPhaseUpdateEnabled(phase)) {
                return sub_sys_.back()->IsPhaseUpdateBefore(other, phase);
            }
            return false;
        }

        bool SystemGroup::IsPhaseUpdateEnabled(EUpdatePhase phase)const
        {
            for (auto sub_sys : sub_sys_) {
                if (sub_sys->IsPhaseUpdateEnabled(phase)) {
                    return true;
                }
            }
            return false;
        }
        void SystemGroup::AddSubSystem(System* sub_system)
        {
            sub_sys_.push_back(sub_system);
        }
        void SystemGroup::RemoveSubSystem(System* sub_system)
        {
            if (auto iter = eastl::find(sub_sys_.begin(), sub_sys_.end(), sub_system); iter != sub_sys_.end())
            {
                sub_sys_.erase(iter);
            }
        }
        SystemGroup::~SystemGroup()
        {
            for (auto* sys : sub_sys_) {
                delete sys;
            }
            sub_sys_.clear();
        }
        bool SystemGroup::IsSubSystemIncluded(System* sub_system)
        {
            if (auto iter = eastl::find(sub_sys_.begin(), sub_sys_.end(), sub_system); iter != sub_sys_.end()) {
                return true;
            }
            return false;
        }
        void SystemGroup::Sort()
        {
            //todo sort by what?
            eastl::sort(sub_sys_.begin(), sub_sys_.end(), [](auto* lhs, auto* rhs) { return true; });
        }
    }

    uint32_t ArcheTypeChunk::SwapAndRemove(uint32_t slot)
    {
        return 0;
    }

    bool ArcheTypeChunk::HasComponentChanged(uint32_t component_index)
    {
        if (component_index >= num_components_) {
            return false;
        }

        return (change_mask_[component_index / 64u] & (1ull << (component_index % 64u))) != 0;
    }

    bool ArcheTypeChunk::HasSlotChanged(uint32_t component_index, uint32_t slot)
    {
        if(!HasComponentChanged(component_index) || slot >= entity_count_){
            return false;
        }
        const auto byte_index = slot / 8u;
        const auto bit_index = slot % 8u;
        return (slot_change_masks_[component_index][byte_index] & (1u << bit_index)) != 0;  
    }

    void ArcheTypeChunk::MarkChanged(uint32_t component_index, uint32_t slot)
    {
        if (component_index >= num_components_ || slot >= entity_count_) {
            return;
        }
        change_mask_[component_index / 64u] |= (1ull << (component_index % 64u));
        const auto byte_index = slot / 8u;
        const auto bit_index = slot % 8u;
        slot_change_masks_[component_index][byte_index] |= (1u << bit_index); 
    }

    void ArcheTypeChunk::ResetComponentChanges(uint32_t component_index)
    {
        if (component_index >= num_components_)  {
            return;
        }
        change_mask_[component_index / 64] &= ~(1ull << (component_index % 64u));
        const auto mask_bytes = Utils::CeilDiv(entity_count_, 8u);
        memset(slot_change_masks_[component_index], 0u, mask_bytes);

    }

    void ArcheTypeChunk::ResetAllChanges()
    {
        memset(change_mask_, 0u, sizeof(change_mask_));

        const auto mask_bytes = Utils::CeilDiv(entity_count_, 8u);
        for (auto component_index = 0u; component_index < num_components_; ++component_index)                           
        {
            memset(slot_change_masks_[component_index], 0u, mask_bytes);
        }
    }

    ArcheTypeTable::ArcheTypeTable(ArcheTypeTable&& other)
        : entity_manager_(other.entity_manager)
        , archetype_graph_(other.archetype_graph_)
        , signature_(std::move(other.signature_))
        , component_sizes_(std::move(other.component_sizes_))
        , component_alignments_(std::move(other.component_alignments_)
        , component_offsets_(std::move(other.component_offsets_))
        , id_to_index_(std::move(other.id_to_index_))
        , chunks_(std::move(other.chunks_))
        , non_full_chunks_(std::move(other.non_full_chunks_))
        , free_head_(std::exchange(other.free_head_, nullptr))
        , entities_(std::move(other.entities_))
        , entity_count_(std::exchange(other.entity_count_, 0u))
        , free_chunk_count_(std::exchange(other.free_chunk_count_, 0u))
        , per_entity_bytes_(std::exchange(other.per_entity_bytes_, 0u))
        , max_chunk_capacity_(std::exchange(other.max_chunk_capacity_, 0u))
        , last_tick_(std::exchange(other.last_tick_, Ticks{}))
    {
    }

    ArcheTypeTable& ArcheTypeTable::operator=(ArcheTypeTable&& other)
    {
        if (this != &other) {
            this->~ArcheTypeTable();
            new(this) ArcheTypeTable(std::move(other));
        }
        return *this;
    }

    void ArcheTypeTable::Finalize(const ComponentTypeRegistry& registry)
    {
        //first sort signature
        signature_.Finalize();

        //build id_to_index_
        size_type index = 0u;
        for (const auto id : signature_.sorted_component_types_)
        {
            id_to_index[id] = index++;
        }

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

    void ArcheTypeTable::Release()
    {
        //todo
    }

    bool ArcheTypeTable::HasAll(const Vector<ComponentID>& ids) const
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

    bool ArcheTypeTable::HasAny(const Vector<ComponentID>& ids) const
    {
        for(const auto id : ids)
        {
            if (HasComponent(id))UNLIKELY {
                return true;
            }
        }
        return false;
    }

    bool ArcheTypeTable::HasNone(const Vector<ComponentID>& ids) const
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

    bool ArcheTypeTable::AddComponent(Entity entity, ComponentID component_id, const void* data, size_type data_size)
    {
        auto entity_loc = entity_manager_->GetEntityLocation(entity);

        if (!entity_loc.IsValid() || entity_loc.arche_type_ != this) UNLIKELY{
            return false;
        }
        
        //check wether already hs this component
        if (HasComponent(component_id)) {
            auto* chunk = GetChunk(entity_loc.chunk_index_);
            chunk->WriteComponent();
            return true;
        }

        //need move entity
        auto* target_archetype = archetype_graph_->GetOrCreateAddTarget(this, component_id);
        if (nullptr == target_archetype) {
            return false;
        }

        return MoveEntity(entity, *target_archetype);
        
    }

    bool ArcheTypeTable::RemoveComponent(Entity entity, ComponentID component_id)
    {
        auto entity_loc = world_->GetEntityLocation(entity);
        if(!entity_loc.IsValid() || entity_loc.arche_type_ != this) UNLIKELY{
            return false;
        }
        if (!HasComponent(component_id)) {
            return false;
        }

        auto* target_archetype = arche_graph_->GetOrCreateRemoveTarget(this, component_id);
        if (nullptr == target_archetype) {
            return false;
        }
        return MoveEntity(entity, *target_archetype);
    }

    void ArcheTypeTable::BuildChunkLayout(ArcheTypeChunk& chunk) noexcept
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

        for (auto n = 0u; n < component_sizes_.size(); ++n)
        {
            chunk->slot_change_masks_[n] = reinterpret_cast<uint8_t*>(ptr_t);
            const auto mask_bytes = Utils::CeilDiv(max_chunk_capacity_, 8u);
            ptr_t += mask_bytes;
        }

        //anything to do?
    }

    size_type ArcheTypeTable::EstimateChunkHeaderSize(size_type capacity) const
    {
        size_type size = sizeof(ArcheTypeChunk);
        //add entities id memory size
        size += sizeof(Entity) * capacity; //ArcheTypeChunk::MAX_CHUNK_CAPACITY;
        //add storage pointer array size
        size += sizeof(void*) * signature_.GetNumComponents();
        //add change detection for slot
        size += sizeof(uint8_t) * Utils::CeilDiv(capacity, 8u) * signature_.GetNumComponents();

        return Utils::AlingUp(size, 64u);
    }

    size_type ArcheTypeTable::EstimatePerEntitySize(const ComponentTypeRegistry& registry) const
    {
        size_type per_entity_rough{ 0u };
        for (const auto id : signature_.sorted_component_types_)
        {
            const auto* info = registry.GetComponentTypeInfo();
            per_entity_rough += info != nullptr ? info->size_ : 0u;
        }
        return perentity_rough;
    }

    size_type ArcheTypeTable::EstimateMaxCapacity(const ComponentTypeRegistry& registry) const
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

    bool ArcheTypeTable::TryFitWithCapacity(const ComponentTypeRegistry& registry, size_type capacity, size_type& mem_size)
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

        for (auto n = 0u; n < signature_.GetNumComponents(); ++n)   
        {
            const auto mask_bytes = Utils::CeilDiv(capacity, 8u);
            if (offset + mask_bytes > ArcheTypeChunk::CHUNK_SIZE)
            {
                return false;
            }
            offset += mask_bytes;
        }
        mem_size = offset;
        return true;
    }

    bool ArcheTypeTable::TryAddEntity(Entity new_entity, uint32_t& out_chunk_index, uint32_t& out_slot)
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
    void ArcheTypeTable::RemoveEntity(uint32_t chunk_index, uint32_t slot)
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

    bool ArcheTypeTable::MoveEntity(Entity entity, ArcheTypeTable& target_archetype)
    {
        auto entity_old_loc = entity_manager_->GetEntityLocation(entity);
        if (!entity_old_loc.IsValid() || entity_old_loc.arche_type_ != this) UNLIKELY{
            return false;
        }

        auto* old_chunk = GetChunk(entity_old_loc.chunk_index_);
        //remove entity from chunk
        auto swapped_slot = old_chunk->SwapAndRemove(entity_old_loc.slot_);

        //allocate in new archetype
        uint32_t new_chunk_index, new_slot;
        if (!target_archetype.TryAddEntity(entity, new_chunk_index, new_slot)) {
            //failed to move, roll back
            old_chunk->WriteComponent(entity_old_loc.slot_, 0, nullptr, 0); //todo need write back all component data?
            return false;
        }

        //transfer component data to new chunk
        auto* new_chunk = target_archetype.GetChunk(new_chunk_index);
        for (auto n = 0u; n < component_sizes_.size(); ++n)
        {
            const auto size = component_sizes_[n];
            const auto component_id = signature_.sorted_component_types_[n];
            assert(target_archetype.HasComponent(component_id) && "target archetype should has this component");
            
            const auto old_component_index = GetComponentIndex(component_id);
            const auto new_component_index = target_archetype.GetComponentIndex(component_id);
            assert(old_component_index != ~0u && new_component_index != ~0u && "component index should be valid")       ;

            auto* src = old_chunk->GetComponent(old_component_index, swapped_slot);
            auto* dst = new_chunk->GetComponent(new_component_index, new_slot);
            std::memcpy(dst, src, size);
        }

        entity_manager_->SetLocation(entity, { &target_archetype, new_chunk_index, new_slot });
    }



}
