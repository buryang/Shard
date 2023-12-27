#include "Utils/Reflection.h"

namespace Shard::Utils {

        static Map<Blake3Hash64, TypeLayoutDesc> g_type_layout_map;

        //fixme think more about endianness
        static inline uint32_t ExtractBitField(const uint8_t* src_field, uint32_t src_bit_beg, uint32_t src_bit_end, uint64_t& dst_field, uint32_t dst_bit_beg) {
                auto dst_bit_offset = dst_bit_beg;
                uint64_t src_field64bit{ 0 };//overflow ?
                memcpy(&src_field64bit, src_field, sizeof(src_field64bit));
                for (auto n = src_bit_beg; n < src_bit_end; ++n) {
                        const auto src_bit_value = (src_field64bit >> n) & 0x1;
                        dst_field |= (src_bit_value << dst_bit_offset);
                        assert(dst_bit_offset < sizeof(uint64_t));
                        ++dst_bit_offset;
                }
                return dst_bit_offset;
        }

        bool FieldLayoutDesc::IsFieldIncluded(const FieldLayoutDesc& desc, const PlatformTypeLayoutParameters& platform) {
                if (platform.IsRayTracingEnable() && (desc.flags_ & ELayoutFlags::eRayTracing)) {
                        return false;
                }
                return true;
        }

        void DefaultLayoutDestroy(void* object, const TypeLayoutDesc& desc, bool is_frozen)
        {
                if (desc.destroy_func_) {
                        desc.destroy_func_();
                }
                if (!is_frozen) {
                        ::operator delete(object);
                }
        }

        uint32_t DefaultLayoutWrite(ImageSectionWriter& writer, void* object, const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc)
        {
                const auto& platform = writer.GetPlatform();
                if (!IsNonVirtualClass(desc.interface_)&&!desc.num_vbases_) {
                        //wrtite vtbl
                        writer.WriteVTble(desc, derived_desc);
                }

                const TypeLayoutDesc* prev_bit_field_t = nullptr;
                uint64_t curr_bit_field_value = 0u;
                uint32_t curr_src_bit_field_bits = 0u;
                uint32_t curr_dst_bit_field_bits = 0u;
                uint32_t num_empty_bases = 0u;

                for (const auto& field : desc.fields_) {
                        const uint8_t* field_object = static_cast<uint8_t*>(object) + 0u;
                        if (field.bit_size_ == 0u) {
                                if (curr_dst_bit_field_bits) {
                                        writer.WriteBytes(reinterpret_cast<uint8_t*>(&curr_bit_field_value), prev_bit_field_t->size_);
                                        prev_bit_field_t = nullptr;
                                        curr_src_bit_field_bits = 0u;
                                        curr_dst_bit_field_bits = 0u;
                                }
                                const auto is_base = field.index_ < desc.num_bases_;
                                assert(field.type_.size_from_fields_ != ~0u);
                                auto padded_field_size = field.type_.size_from_fields_;
                                if (padded_field_size == 0u && is_base) {
                                        if (num_empty_bases > 0u) {
                                                padded_field_size = 1u;
                                        }
                                        else
                                        {
                                                ++num_empty_bases;
                                        }
                                }
                                else
                                {
                                        if (padded_field_size == 0u) {
                                                padded_field_size = 1u;
                                        }
                                }

                                if (padded_field_size > 0u) {
                                        if (!is_base || platform.IsAlignBasesEnable())
                                        {
                                                const auto field_alignment = std::min(0u, field.type_.align_); //todo alignment
                                                writer.WriteAlignment(field_alignment);
                                                for (auto n = 0; n < field.array_size_; ++n) {
                                                        const auto prev_offset = writer.GetOffset();
                                                        field.type_.write_func(writer, field_object, field.type_, field.type_);
                                                        const auto field_size = writer.GetOffset() - prev_offset;
                                                        const auto align_field_size = AlignUp(field_size, field_alignment);
                                                        writer.WritePadSize(align_field_size - field_size);
                                                }
                                        }
                                        else
                                        {
                                                writer.WritePadSize(1u);
                                        }
                                }
                        }
                        else {
                                if (field.type_ != *prev_bit_field_t || curr_dst_bit_field_bits + field.bit_size_ > field.type_.size_*8u) {
                                        //write alignment
                                        const auto field_alignment = std::min(0u, prev_bit_field_t->align_);
                                        writer.WriteAlignment(prev_bit_field_t->align_);
                                        if (curr_dst_bit_field_bits != 0) {
                                                writer.WriteBytes(reinterpret_cast<uint8_t*>(&curr_bit_field_value), prev_bit_field_t->size_);
                                        }
                                        curr_bit_field_value = 0u;
                                        curr_src_bit_field_bits = 0u;
                                        curr_dst_bit_field_bits = 0u;
                                        prev_bit_field_t = &field.type_;
                                }
                                ExtractBitField(field_object, curr_src_bit_field_bits, curr_src_bit_field_bits + field.bit_size_, curr_bit_field_value, curr_dst_bit_field_bits);
                                curr_dst_bit_field_bits += field.bit_size_;
                        }
                }
        }

        uint32_t DefaultLayoutHashAppend(const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform, blake3_hasher& hasher)
        {
                //unreal use name, here use hashname
                blake3_hasher_update(&hasher, &desc.hash_name_, sizeof(desc.hash_name_));
                uint32_t offset{ 0u };
                uint32_t num_empty_bases{ 0u };
                uint32_t curr_bit_field_bits{ 0u };
                const TypeLayoutDesc* prev_bit_field_t{ nullptr };
                if (IsNonVirtualClass(desc.interface_) && desc.num_vbases_ == 0u) {
                        offset += 0u;
                }
                for (auto field : desc.fields_) {
                        if (FieldLayoutDesc::IsFieldIncluded(field, platform)) {
                                prev_bit_field_t = nullptr;
                                curr_bit_field_bits = 0u;
                                const auto& field_type = field.type_;
                                const auto is_base = field.index_ < desc.num_bases_;
                                if (field.bit_size_ == 0u) {
                                        curr_bit_field_bits = 0u;
                                        prev_bit_field_t = nullptr;
                                        const auto field_alignment = std::min(0, 0);
                                        offset = AlignUp(offset, field_alignment);
                                        blake3_hasher_update(&hasher, &offset, sizeof(offset));
                                        blake3_hasher_update(&hasher, &field.array_size_, sizeof(field.array_size_));
                                        auto padded_field_size = field_type.hash_func_(field_type, hasher);
                                        if (padded_field_size == 0u && is_base) {
                                                if (num_empty_bases) {
                                                        padded_field_size = 1u;
                                                }
                                                ++num_empty_bases;
                                        }
                                        else {
                                                padded_field_size = std::max(padded_field_size, 1u);
                                        }

                                        if (padded_field_size > 0u) {
                                                if (!is_base || platform.IsAlignBasesEnable()) {
                                                        const auto field_size = AlignUp(padded_field_size, field_alignment);
                                                        offset += field_size * field.array_size_;
                                                }
                                                else
                                                {
                                                        offset += padded_field_size;
                                                }
                                        }
                                }
                                else {
                                        if (*prev_bit_field_t == field_type && curr_bit_field_bits + field.bit_size_ <= field_type.size_ * 8u) {
                                                curr_bit_field_bits += field.bit_size_;
                                        }
                                        else
                                        {
                                                const auto field_size = field_type.hash_func_(field_type, hasher);
                                                prev_bit_field_t = &field_type;
                                                curr_bit_field_bits = field.bit_size_;
                                                offset += field_size;
                                        }
                                        blake3_hasher_update(&hasher, &field.bit_size_, sizeof(field.bit_size_));
                                }
                        }
                }
                return offset;
        }

        uint32_t DefaultLayoutGetAlignment(const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform)
        {
                //search the maxalignment for every field
                uint32_t alignment{ 0u };
                const auto max_alignment = platform.GetMaxAlignment();
                if (IsNonVirtualClass(desc.interface_)) {
                        alignment = std::min(platform.GetPointerSize(), max_alignment);
                }

                if (alignment < max_alignment) {
                        for (const auto& field : desc.fields_) {
                                if (field.IsFieldIncluded(field, platform)) {
                                        const auto field_alignment = field.type_.alignment_func_();
                                        alignment = std::max(field_alignment, alignment);
                                        if (alignment >= max_alignment) {
                                                return max_alignment;
                                        }
                                        
                                }
                        }
                }
                return alignment;
        }

        uint32_t DefaultUnfreezeCopy(const void* frozen_object, const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& frozen_platform, void* dst_obj)
        {
                uint32_t frozen_offset{ 0u };
                uint32_t num_empty_bases{ 0u };
                uint32_t prev_offset{ ~0u };
                if (!IsNonVirtualClass(desc.interface_)) {
                        memcpy(dst_obj, frozen_object, frozen_platform.GetPointerSize()); //todo 
                        frozen_offset += frozen_platform.GetPointerSize();
                }

                for (auto field : desc.fields_) {
                        const auto& field_type = field.type_;
                        const auto is_base = field.index_ < desc.num_bases_;
                        uint8_t* dst_field = static_cast<uint8_t*>(dst_obj) + field.offset_;
                        if (field.IsFieldIncluded(field, frozen_platform)) {
                                if (field.bit_size_ == 0u || prev_offset != field.offset_)//not a bit field or the first bit-field
                                {
                                        const auto field_alignment = std::min(frozen_platform.GetMaxAlignment(),
                                                                                                        field_type.alignment_func_(field_type));
                                        frozen_offset = AlignUp(frozen_offset, field_alignment);
                                        const auto* frozen_field = static_cast<const uint8_t*>(frozen_object) + frozen_offset;
                                        auto padded_field_size = field_type.unfreeze_copy_func_(frozen_field, field_type, dst_field);
                                        if (padded_field_size == 0u && is_base) {
                                                if (num_empty_bases) {
                                                        padded_field_size = 1u;
                                                }
                                                ++num_empty_bases;
                                        }
                                        else {
                                                padded_field_size = std::min(padded_field_size, 1u);
                                        }

                                        if (padded_field_size > 0u) {
                                                if (!is_base ||frozen_platform.IsAlignBasesEnable()) {
                                                        padded_field_size = AlignUp(padded_field_size, field_alignment);
                                                }
                                        }

                                        frozen_field += padded_field_size;
                                        dst_field += field_type.size_;
                                        frozen_offset += padded_field_size;
                                        for (auto n = 1; n < field.array_size_; ++n) {
                                                field_type.unfreeze_copy_func_(frozen_field, field_type, dst_field);
                                                frozen_field += padded_field_size;
                                                dst_field += field_type.size_;
                                                frozen_offset += padded_field_size;
                                        }

                                }
                        }
                        else
                        {
                                const auto* default_obj = field_type.default_instance_func_ ? field_type.default_instance_func_() : nullptr;
                                if (default_obj) {
                                        memcpy(dst_field, default_obj, field_type.size_);
                                }
                                else {
                                        memset(dst_field, 0u, field_type.size_);
                                }
                        }
                        prev_offset = field.offset_;
                }
                return frozen_offset;
        }

        uint32_t DefaultLayoutFreeze(const void* object, const TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& frozen_platform)
        {
                ImageObject image_obj;
                image_obj.SetPlatform(frozen_platform);
                ImageSectionWriter image_writer(image_obj);
                image_writer.WriteRootObject(object, desc);

                ImageFreezeObject frozen_obj;
                image_obj.Flatten(frozen_obj);
                //to do
                return 0u;
        }

        bool TryToGetOffsetFromBase(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc, uint32_t& offset)
        {
                if (desc == derived_desc) {
                        offset = 0u;
                        return true;
                }
                for (auto n = 0; n < derived_desc.num_bases_; ++n) {
                        const auto& field = derived_desc.fields_[n];
                        assert(field.name_ == "BASE");
                        const auto ret = TryToGetOffsetFromBase(desc, field.type_, offset);
                        if (ret) {
                                offset += field.offset_;
                                return true;
                        }
                }
                return false;
        }

        bool TypeLayoutDesc::InitializeTypeLayout(TypeLayoutDesc& desc, const PlatformTypeLayoutParameters& platform)
        {
                if (desc.fields_.empty()&&IsNonVirtualClass(desc.interface_)) {
                        PCHECK(desc.size_ != 1u);
                        desc.size_from_fields_ = 1u;
                        return false;
                }

                uint32_t offset{ 0u };
                const TypeLayoutDesc* prev_type_layout_t{ nullptr };
                uint32_t curr_bit_field_bits{ 0u };
                if (!IsNonVirtualClass(desc.interface_) && desc.num_vbases_) {
                        offset += platform.GetPointerSize();
                }
                //initialize offset for bit-fields
                for (auto field : desc.fields_) {
                        if (field.bit_size_ == 0u) {
                                if (prev_type_layout_t) {
                                        offset += prev_type_layout_t->size_;
                                        prev_type_layout_t = nullptr;
                                        curr_bit_field_bits = 0u;
                                }
                                const auto alignment = std::min(field.type_.align_, platform.GetMaxAlignment());
                                offset = AlignUp(offset, alignment);
                                PCHECK(offset != field.offset_) << "type layout offset not correct";
                                offset += field.type_.size_; //todo
                        }
                        else {
                                assert(field.offset_ == -1);
                                if (&field.type_ != prev_type_layout_t || field.bit_size_ + curr_bit_field_bits > prev_type_layout_t->size_ * 8) {
                                        const auto alignment = std::min(field.type_.align_, platform.GetMaxAlignment());
                                        offset = AlignUp(offset, alignment);
                                        offset += field.type_.size_;
                                        prev_type_layout_t = &field.type_;
                                }
                                field.offset_ = offset;
                                curr_bit_field_bits += field.bit_size_;
                        }
                }
                //if last field is bit-field
                if (curr_bit_field_bits) {
                        offset += prev_type_layout_t->size_;
                }
                desc.size_from_fields_ = offset;
                return true;
        }

        bool TypeLayoutDesc::RegistTypeLayout(const TypeLayoutDesc& type_layout)
        {
                if (auto iter = g_type_layout_map.find(type_layout.hash_name_); iter == g_type_layout_map.end()) {
                        g_type_layout_map.insert({ type_layout.hash_name_, type_layout });
                        return true;
                }
                return false;
        }

        TypeLayoutDesc* TypeLayoutDesc::GetTypeLayout(Blake3Hash64 hash)
        {
                if (auto iter = g_type_layout_map.find(hash); iter != g_type_layout_map.end()) {
                        return &iter->second;
                }
                return nullptr;
        }

        uint32_t TypeLayoutDesc::GetOffsetFromBase(const TypeLayoutDesc& derived, const TypeLayoutDesc& base)
        {
                uint32_t offset{ 0u };
                auto ret = TryToGetOffsetFromBase(base, derived, offset);
                return ret ? offset : -1;//FIXME
        }

        bool TypeLayoutDesc::IsDerivedFrom(const TypeLayoutDesc& probe_derived, const TypeLayoutDesc& probe_base)
        {
                uint32_t offset{ 0u };
                auto ret = TryToGetOffsetFromBase(probe_base, probe_derived, offset);
                return ret;
        }
        
        bool TypeLayoutDesc::IsEmptyType(const TypeLayoutDesc& desc)
        {
                if (desc.fields_.empty() && IsNonVirtualClass(desc.interface_)) {
                        return true;
                }
                return false;
        }
}