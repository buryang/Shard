#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Handle.h"
#include "Core/GFXDefinitions.h"
#include "HAL/HALResources.h"
#include <type_traits>

namespace Shard
{
    namespace Render
    {
        template<Utils::Integer T>
        struct HashName
        {
            T   name_;
            static HashName FromString(const String& name) {
                HashName hash_name{ static_cast<T>(InternBlake3HashForBytes(name.data(), name.size())) }; //slow?
                return hash_name;
            }
            bool operator==(const HashName& rhs) const noexcept {
                return name_ == rhs.name_;
            }
            bool operator!=(const HashName& rhs) const noexcept {
                return !(*this == rhs);
            }
        };

        class Field
        {
        public:
            enum class EType : uint8_t
            {
                eBuffer,
                eTexture,
                eNum,
            };

            enum class EUsage :uint8_t
            {
                eUnkown = 0x0,
                eInput  = 0x1,
                eOutput = 0x2,
                eExternal   = 0x100, //external resource
                eCulled = 0x200, //field deleted
            };

            enum class EAuxFlags : uint8_t
            {
                eAppendBuffer = 1 << 0,
                eUseCounter = 1 << 1,
                eStructuredBuffer = 1 << 2,
                eTypedBuffer    = 1 << 3,
                //todo 
            };
            DECLARE_ENUM_BIT_OPS(EAuxFlags);

            using Name = HashName<uint32_t>;
            static Field CreateDSVField(const String& name);
            static Field CreateSRVField(const String& name);
            static Field CreateRTVField(const String& name);
            static Field CreateUAVField(const String& name);
            Field() { texture_sub_range_ = {}; }
            FORCE_INLINE bool IsWholeResource()const { return type_ == EType::eBuffer || texture_sub_range_.IsWholeRange(dims_); }
            FORCE_INLINE bool IsConnectAble(const Field& other)const;
            FORCE_INLINE bool IsExternal()const { return sub_state_.access_ == EAccessFlags::eExternal; }
            FORCE_INLINE bool IsOutput()const { return Utils::HasAnyFlags(usage_, EUsage::eOutput); }
            FORCE_INLINE bool IsInput()const { return !IsOutput(); }
            FORCE_INLINE bool IsInputConnected() const { return IsInput() && GetHashName() != GetParentName(); }
            Field& NameAs(const String& name);
            Field& Description(const String& description);
            Field& ParentName(const Name& name);
            Field& Width(const uint32_t width);
            Field& Height(const uint32_t height);
            Field& Depth(const uint32_t depth);
            Field& MipSlices(const uint32_t mip_slices);
            Field& ArraySlices(const uint32_t array_slices);
            Field& PlaneSlices(const uint32_t plane_slices);
            Field& SampleCount(const uint32_t sample_count);
            Field& StageFlags(EPipelineStageFlags stage);
            Field& Type(EType type);
            Field& State(TextureState state);
            Field& Usage(EUsage usage);
            Field& AuxFlags(EAuxFlags flags);
            EType GetType()const;
            const String& GetName()const;
            const Name& GetHashName()const;
            const Name& GetParentName()const;
            const String& GetDescription()const;
            const TextureDim& GetDimension()const;
            const uint32_t GetSampleCount()const;
            const TextureState& GetSubFieldState()const;
            const TextureSubRangeRegion& GetTextureSubRange()const;
            const BufferSubRangeRegion& GetBufferSubRange()const;
            const uint32_t GetSampleCount()const;
            EPixFormat GetPixFormat()const;
            EPipelineStageFlags GetPipelineStage()const;
            EAuxFlags GetAuxFlags()const;
            friend bool operator==(const Field& lhs, const Field& rhs) {
                return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
            }
            friend bool operator!=(const Field& lhs, const Field& rhs) { return !(lhs == rhs); }
        private:
            friend class RenderPass;
            String  name_;
            String  description_;
            Name    hash_name_;
            //name for producer
            Name    parent_name_;
            EType   type_;
            EUsage  usage_{ EUsage::eUnkown };
            EAuxFlags   aux_flags_{ EAuxFlags::eNone };
            EPixFormat  pix_fmt_{ EPixFormat::eUnkown };
            //which stage of pipeline use this Field
            EPipelineStageFlags stage_{ EPipelineStageFlags::eAll };
            //user force it to be transiant
            uint32_t    is_force_dedicated_:1;
            uint32_t    sample_count_:6;
            ETextureDimType dim_prop_{ ETextureDimType::eAbsolute };
            TextureDim  dims_;
            union {
                /*texture range current field used*/
                TextureSubRangeRegion texture_sub_range_;
                BufferSubRangeRegion  buffer_sub_range_;
            };
            TextureState    sub_state_;
        };

        static HAL::HALTextureInitializer CreateTextureInitializerForField(const Field& field) {
            HAL::HALTextureInitializer initializer{
                .dims_ = field.GetDimension(),
                .access_ = field.GetSubFieldState().access_,
                .layout_ = field.GetSubFieldState().layout_,
                .format_ = field.GetPixFormat(),
            };
            return initializer;
        }
        
        static HAL::HALBufferInitializer CreateBufferInitializerForField(const Field& field) {
            assert(field.GetType() == Render::Field::EType::eBuffer);
            HAL::HALBufferInitializer initializer{ .size_ = field.GetDimension().width_, .access_ = field.GetSubFieldState().access_ };
            const auto& aux_flags = field.GetAuxFlags();
            if (Utils::HasAnyFlags(aux_flags, Field::EAuxFlags::eTypedBuffer|Field::EAuxFlags::eTypedBuffer)) {
                initializer.type_ = HAL::HALBufferInitializer::EType::eRawBuffer;
            }
            else {
                initializer.type_ = HAL::HALBufferInitializer::EType::eStructedBuffer;
            }
            return initializer;
        }

        class RenderPass;
        /*parent resource class for texture and buffer*/
        class RenderResource
        {
        public:
            using ThisType = RenderResource;

            RenderResource() = default;
            explicit RenderResource(const Field& field);
            virtual ~RenderResource() {}
            bool IsExternal()const {
                return is_external_;
            }
            bool IsTransient()const {
                return is_transient_;
            }
            bool IsOutput()const {
                return is_output_;
            }
            bool IsManagedByUser()const {
                return IsExternal() || IsOutput();
            }
            void Read(RenderPass::Handle pass) {
                consumer_passes_.insert(pass);
                AddRef();
            }
            void Write(RenderPass::Handle pass) {
                producer_passes_.insert(pass);
                AddRef();
            }
            auto& GetAccessFlags() {
                return access_flags_;
            }
            auto& GetLayoutFlags() {
                return layout_flags_;
            }
           
            [[deprecated]]
            const auto& GetProducer() const {
                return producer_passes_;
            }
            [[deprecated]]
            const auto& GetConsumer() const {
                return consumer_passes_;
            }
            void AddRef() {
                ++ref_count_;
            }
            void DecrRef() {
                --ref_count_;
                if (ref_count_ == 0u) {
                    EndHAL();
                }
                assert(ref_count_ >= 0 && "decrease times more than add times");
            }
            bool CalcLifeTime(uint32_t& start, uint32_t& end);
            /*get memory requirement*/
            virtual uint64_t GetOccupySize()const = 0;
            /*set rhi backend for this resource*/
            virtual void SetHAL() = 0;
            /*get rhi backend handle*/
            virtual void* GetHAL() = 0; //todo
            /*release rhi to the backend*/
            virtual void EndHAL() = 0;
        private:
            uint32_t    is_external_:1{ 0u };
            uint32_t    is_transient_:1{ 1u };
            uint32_t    is_output_ : 1{ 0u };
            uint16_t    ref_count_{ 0u };
            EAccessFlags    access_flags_{ EAccessFlags::eNone };
            ELayoutFlags    layout_flags_{ ELayoutFlags::eUnkown };
            Set<RenderPass::Handle>   producer_passes_;
            Set<RenderPass::Handle>   consumer_passes_;
        };

        class RenderBuffer : public RenderResource
        {
        public:
            using BufferRepo = Utils::ResourceManager<RenderBuffer, Utils::ScalablePoolAllocator>;
            using Handle = BufferRepo::Handle;
            static BufferRepo& BufferRepoInstance();
            RenderBuffer(const Field& field);
            ~RenderBuffer();
            //RenderBuffer*& GetCounter() { return counter_resource_; }
        public:
            void SetHAL()override;
            void EndHAL()override;
        private:
            HAL::HALBuffer* rhi_buffer_{ nullptr };
            //RenderBuffer*   counter_resource_{ nullptr }; //for uav counter
        };

        class RenderTexture : public RenderResource
        {
        public:
            using TextureRepo = Utils::ResourceManager<RenderTexture, Utils::ScalablePoolAllocator>;
            using Handle = TextureRepo::Handle;
            /*texture and buffer repo singleton instance*/
            static TextureRepo& TextureRepoInstance();
            RenderTexture(const Field& field);
            ~RenderTexture();
        public:
            void SetHAL()override;
            void EndHAL()override;
            bool IsWholeTexture()const;
            void MergeField(const Field& curr_field);
            SmallVector<TextureState>& GetState();
        private:
            uint32_t    sample_count_{ 1u }; //max sample count 64
            TextureDim  dims_;
            SmallVector<TextureState>   curr_state_;
            HAL::HALTexture*    rhi_texture_{ nullptr };
        };

        //framegraph blackboard for every frame
        class RenderResourceCache
        {
        public:
            RenderTexture::Handle GetOrCreateTexture(const Field& field);
            RenderBuffer::Handle GetOrCreateBuffer(const Field& field);
            Optional<RenderTexture::Handle> TryGetTexture(const Field::Name& field);
            Optional<RenderBuffer::Handle> TryGetBuffer(const Field::Name& field);
            //queue extracted output resource 
            bool QueueExtractedTexture(const Field::Name& field, RenderTexture::Handle& texture);
            bool QueueExtractedBuffer(const Field::Name& field, RenderBuffer::Handle& buffer);
            bool RegistExternalBuffer(const Field::Name& external_field, RenderBuffer::Handle buffer);
            bool RegistExternalTexture(const Field::Name& external_field, RenderTexture::Handle texture);
            void Clear(bool non_extract);
            template<typename Function>
            void EnumerateTexture(Function&& func) {
                for (auto texture : texture_repos_) {
                    func(texture);
                }
            }
            template<typename Function>
            void EnumerateBuffer(Function&& func) {
                for (auto buffer : buffer_repos_) {
                    func(buffer);
                }
            }
        private:
            struct CacheNode {
                uint32_t index_ : 30 { 0u };
                uint32_t is_external_ : 1;
                uint32_t is_extacted_ : 1;
            };
            CacheNode& NewTextureCacheNode(const Field::Name& field, RenderTexture::Handle texture);
            CacheNode& NewBufferCacheNode(const Field::Name& field, RenderBuffer::Handle buffer);
        private:
            Map<const Field::Name, CacheNode>  texture_map_;
            Map<const Field::Name, CacheNode>  buffer_map_;
            Vector<RenderBuffer::Handle>    buffer_repos_;
            Vector<RenderTexture::Handle>   texture_repos_;
        };
    }
}
