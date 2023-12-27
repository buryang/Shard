#pragma once
#include "Utils/FileArchive.h"

//mainly copy from unreal engine
namespace Shard::Utils {

    struct VTblPointer {
        Blake3Hash64 hash_name_; //hash for parent class
        uint32_t    derived_offset_{ 0u };
        uint32_t    offset_{ 0u };
    };

    struct FrozenPointer {
        Blake3Hash64 class_name; //hash for point to class type;
        uint32_t    offset_from_this_{ 0 }; //data pointed offset from pointer 
    };

    class ImageObject;
    class TypeLayoutDesc;
    class ImageObjectSection {
    public:
        using Ptr = ImageObjectSection*;
        struct SectionPointer {
            uint32_t    section_index_{ 0u };
            uint32_t    dervied_offset_{ 0u }; //pointer offset to parent class
            uint32_t    offset_{ 0u }; //pointer value in byte buffer
        };
        uint32_t WriteBytes(const uint8_t* bytes, size_t size);
        template<typename PlainStruct>
        uint32_t FORCE_INLINE WritePlainObject(PlainStruct&& object) {
            WriteBytes(&object, sizeof(object);
        }
        ImageObjectSection& WritePointer();
        uint32_t WriteAlignment(uint32_t alignment);
        uint32_t WritePadSize(uint32_t pad);
        uint32_t WriteArray(const void* object, size_t size, const TypeLayoutDesc& desc);
        uint32_t WriteRawPointerSizedBytes(uint64_t pointer);
        uint32_t WriteVTble(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc);
        ImageObjectSection& WritePointer(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc, uint32_t& offset_to_base);
        uint32_t Flatten(ImageFreezeObject& object, bool to_merge_sections = true)const;
        FORCE_INLINE uint32_t GetOffset()const { return data_.size(); }
        FORCE_INLINE ImageObject::Ptr GetParent() { return parent_; }
        void ComputeHash();
        const FORCE_INLINE Blake3Hash64 GetHash()const {
            return hash_;
        }
    private:
        friend class ImageObject;
        ImageObject::Ptr    parent_;
        Vector<uint8_t>    data_;
        Blake3Hash64    hash_;
        uint32_t    max_align_{ 0 };

        //pointers data caches
        SmallVector<SectionPointer>    pointers_;
        SmallVector<VTblPointer>    vtbls_;
    };

    class ImageFreezeObject {
    public:
        uint32_t AppendData(const void* data, const size_t size, const size_t alignment=0);
        void Serialize(FileArchive& archive);
        void UnSerialize(FileArchive& archive);
        //write vtbl pointer etc
        void ApplyPatches(void* frozen_obj);
        FORCE_INLINE size_t GetSize() const {
            return data_.size();
        }
    private:
        friend class ImageObjectSection;
        friend class ImageObject;
        Vector<uint8_t>    data_;
        SmallVector<VTblPointer>    vtbls_;
        PlatformTypeLayoutParameters    platform_{ PlatformTypeLayoutParameters::CurrentParameters()};
    };

    class ImageObject {
    public:
        using Ptr = ImageObject*;
        FORCE_INLINE ImageObjectSection& NewObjectSection() {
            sections_.emplace_back(ImageObjectSection());
            return sections_.back();
        }
        FORCE_INLINE size_t GetSectionCount()const {
            return sections_.size();
        }
        FORCE_INLINE const PlatformTypeLayoutParameters& GetPlatform()const {
            return platform_;
        }
        FORCE_INLINE void SetPlatform(const PlatformTypeLayoutParameters& platform) {
            platform_ = platform;
        }
        uint32_t Flatten(ImageFreezeObject& object)const;
    private:
        SmallVector<ImageObjectSection> sections_;
        PlatformTypeLayoutParameters    platform_{ PlatformTypeLayoutParameters::CurrentParameters() };
    };

    class ImageWriter {
    public:
        explicit ImageWriter(ImageObjectSection& section) :section_(section) {}
        explicit ImageWriter(ImageObject& object) :section_(object.NewObjectSection()) {}
        void WriteBytes(const uint8_t* object, size_t size);
        void WriteObject(const void* object, const TypeLayoutDesc& desc);
        void WriteRootObject(const void* object, const TypeLayoutDesc& desc);
        void WriteObjectArray(const void* object, const TypeLayoutDesc& desc, uint32_t num_array);
        ImageWriter* WritePointer(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc, uint32_t& offset_to_base);
        uint32_t WriteAlignment(uint32_t alignment);
        uint32_t WritePadSize(uint32_t pad_size);
        uint32_t WriteVTble(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc);
        uint32_t GetOffset()const;
        const PlatformTypeLayoutParameters& GetPlatform()const;
    private:
        ImageObjectSection& section_;
    };

    template<typename T>
    class ImageFreezeObjectContainer {
    public:
        using ValType = T;
        ImageFreezeObjectContainer(const TypeLayoutDesc& type, T* val, uint32_t frozen_size)
            :type_(&type), raw_ptr_(val), frozen_size_(frozen_size) {
        }
        template <typename TOther>
        ImageFreezeObjectContainer(TOther* object)
            : desc_(&TypeLayoutResolver::Get<TOther>()), raw_ptr_(object), frozen_size(0u) {
        }
        FORCE_INLINE bool IsFrozen()const {
            return frozen_size_ == 0u;
        }
        void Freeze();
        void UnFreeze();
        void Destroy();
        T* Get() {
            if (!IsFrozen()) {
                return raw_ptr_;
            }
            LOG(ERROR) << "frozen object should not change";
        }
        const T* Get()const {
            return raw_ptr_;
        }
        ~ImageFreezeObjectContainer();
    private:
        T*    raw_ptr_{ nullptr };
        uint32_t    frozen_size_{ 0u };
        const TypeLayoutDesc* desc_{ nullptr };
    };

    template<typename T>
    inline void ImageFreezeObjectContainer<T>::Freeze()
    {
        if (!frozen_size && raw_ptr_) {
            /*
            ImageObject image_obj;
            ImageWriter image_writer(image_obj);
            image_writer.WriteRootObject(raw_ptr_, raw_ptr_->GetLayoutDesc());
            ImageFreezeObject freeze_obj;
            image_obj.Flatten(freeze_obj);
            delete raw_ptr_;
            auto* ptr = ::operator::new(freeze_obj.data_.size());
            memcpy(ptr, freeze_obj.data_, freeze_obj.data_.size());
            image_obj.ApplyPatches(ptr);
            raw_ptr_ = reinterpret_cast<T*>(freeze_obj.data_);
            */
            frozen_size_ = 0u; //fixme
        }
    }
    template<typename T>
    inline void ImageFreezeObjectContainer<T>::UnFreeze()
    {
        if (frozen_size_) {

            auto* ptr = ::operator new(desc_->size_);
            const auto unfreeze_func = desc->unfreeze_copy_func_;
            unfreeze_func(raw_ptr_, *desc_, ptr);
            ;;operator delete(raw_ptr_);
            raw_ptr_ = reinterpret_cast<T*>(ptr);
            frozen_size_ = 0u;
        }
    }
    
    template<typename T>
    inline void ImageFreezeObjectContainer<T>::Destroy()
    {
        //fixme
        if (raw_ptr_ != nullptr) {
            if (desc_->detroy_func_) {
                desc_->detroy_func_(raw_ptr_);
            }
            ::operator delete(raw_ptr_);
            raw_ptr_ = nullptr;
            frozen_size_ = 0u;
        }
    }
    template<typename T>
    inline ImageFreezeObjectContainer<T>::~ImageFreezeObjectContainer()
    {
        Destroy();
    }
}