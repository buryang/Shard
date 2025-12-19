#pragma once
#include "Utils/SimpleEntitySystem.h"
#include "HAL/HALResources.h"
#include "SceneProxy/SceneProxy.h"

namespace Shard::Render
{
    class GPUDataUploader;
    
    enum class EUploaderType
    {
        eBulk,
        eSparse,
    };

    //todo I think should using render graph to schedule uploader
    class GPUUploadSystem : public Utils::ECSSystem
    {
    public:
        friend class RenderSystem;
        void Init() override;
        void UnInit() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
        GPUDataUploader* AccquireDataUploader(EUploaderType loader_type);
        void ReleaseDataUploader(GPUDataUploader* uploader);
    private:
        SmallVector<GPUDataUploader*>   uploaders_;
    };

    struct UploadOperation
    {
        enum class EOperation
        {
            eRaw,
            eTransform,
            eInverseTransform,
        };
        EOperation  ops_{ EOperation::eRaw };
        size_type   stage_data_offset_{ 0u };
        size_type   device_data_offset_{ 0u };
        size_type   data_size_{ 0u };
    };

    class GPUDataUploader
    {
    public:
        virtual void Begin(HAL::HALBuffer* stage_buffer, HAL::HALBuffer* device_buffer) = 0;
        //submit all write item to GPU
        virtual void EndAndSubmit(HAL::HALCommandContext& immediate_contnext) = 0;
        virtual ~GPUDataUploader() {}
        //manager GPU data slot range
        size_type AccquireDataSlot(size_type data_size);
        void ReleaseDataSlot(size_type data_offset, size_type data_size);
        //clear all allocate data slots
        bool ReWindDataSlots() { slot_allocator_.ReWind(); }
        size_type GetMaxOccupySize() const { return slot_allocator_.GetDataSlotMaxSize(); }
    protected:
        HAL::HALBuffer* stage_buffer_{ nullptr };
        HAL::HALBuffer* device_buffer_{ nullptr };
        class DataSlotLinearAllocator
        {
        public:
            void Init(size_type size) { capacity_ = size; curr_cursor_ = 0u; }
            void UnInit() {}
            size_type Allocate(size_type data_size);
            void DeAllocate(size_type data_offset, size_type data_size);
            //todo free slot merge work
            void Consolidate();
            void ReWind() { free_ranges_.clear(); curr_cursor_ = 0u; }
            auto GetDataSlotMaxSize() const { return Utils::Min(curr_cursor_, capacity_); } //todo peak size
        private:
            struct FreeSlot {
                size_type   offset_;
                size_type   size_;
            };
            List<FreeSlot>    pending_free_ranges_;
            List<FreeSlot>  free_ranges_;
            size_type   capacity_{ ~0u };
            size_type   curr_cursor_{ 0u };
        };
        DataSlotLinearAllocator slot_allocator_;
    };

    class GPUBulkUploader : public GPUDataUploader
    {
    public:
        void Begin(HAL::HALBuffer* stage_buffer, HAL::HALBuffer* device_buffer) override;
        //submit all write item to GPU
        void EndAndSubmit(HAL::HALCommandContext& immediate_contnext) override;
    };

    class GPUUploaderStageWriter
    {
    public:
        explicit GPUUploaderStageWriter(GPUSparseUploader* owner) :owner_(owner) {
            assert(owner != nullptr && "stage writer should bind to nullptr");
        }
        void Write(void* cpu_data, size_type size, UploadOperation::EOperation ops);
    private:
        GPUSparseUploader* owner_{ nullptr };
    };

    class GPUSparseUploader : public GPUDataUploader
    {
    public:
        friend class GPUUploaderStageWriter;
        explicit GPUSparseUploader(size_type);
        GPUUploaderStageWriter& TLSGetUploadWriter();
        void Begin(HAL::HALBuffer* stage_buffer, HAL::HALBuffer* device_buffer) override;
        //submit all write item to GPU
        void EndAndSubmit(HAL::HALCommandContext& immediate_contnext) override;
        FORCE_INLINE uint32_t GetUploadNum() const { return operation_offset_ / sizeof(UploadOperation); }
        FORCE_INLINE constexpr bool IsStageBufferFull() const { return operation_offset_ >= data_offset_; }
        ~GPUSparseUploader();
    private:
        bool TryAllocStageBuffer(size_type size, void*& dst, size_type& ops_offset, size_type& data_offset);
    private:
        HAL::HALShader* sparse_load_shader_{ nullptr }; //sparse load shader
        size_type   operation_offset_{ 0u };//operation data stack from stage buffer begin
        size_type   data_offset_{ ~0u }; // like unity data stack from stage buffer end
        std::atomic_bool    alloc_lock_{ false };
    };
}
