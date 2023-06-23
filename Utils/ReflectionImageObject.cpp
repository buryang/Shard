#include "Utils/ReflectionImageObject.h"
#include "Utils/Reflection.h"
#include "eastl/sort.h"

namespace Shard::Utils {

	static inline void ApplyVTblValue(void* frozen_obj, const TypeLayoutDesc& derived_desc, uint32_t vtbl_offset, uint32_t offset) {
		//all instance of a class vtbl pointer to a constant address
		const auto** vtbl_src = (const void**)(reinterpret_cast<uint8_t*>(derived_desc.default_instance_func_()) + vtbl_offset);
		auto** vtbl_dst = (void const**)(reinterpret_cast<uint8_t*>(frozen_obj) + offset);
		vtbl_dst = vtbl_src;
	}

	static inline void ApplyFrozenPointerValue(void* frozen_obj, uint32_t offset, uint64_t pointer /*,bool is_32bit*/) {
		auto** pointer_dst = (void const**)(reinterpret_cast<uint8_t>(frozen_obj) + offset);
		pointer_dst = (void const**)pointer; //fixme
	}

	uint32_t ImageObjectSection::Flatten(ImageFreezeObject& object, bool to_merge_sections = true) const
	{
		const auto offset = object.AppendData(data_.data(), data_.size(), max_align_);
		//merge section pointers to object
		object.vtbls_.resize(object.vtbls_.size() + vtbls_.size());
		for (auto src_iter = vtbls_.rbegin(), dst_iter = object.vtbls_.rbegin();
			src_iter != vtbls_.rend(); ++src_iter, ++dst_iter) {
			*dst_iter = *src_iter;
			dst_iter->offset_ += offset;
		}
		return offset;
	}

	uint32_t ImageObjectSection::WriteAlignment(uint32_t alignment)
	{
		const auto prev_size = data_.size();
		const auto offset = AlignUp(prev_size, alignment);
		WritePadSize(offset - prev_size);
		max_align_ = std::max(max_align_, alignment); //change maxalignment for section
		return offset;
	}

	uint32_t ImageObjectSection::WritePadSize(uint32_t pad)
	{
		const auto offset = pad + data_.size();
		data_.resize(offset, 0u);
		return offset;
	}

	uint32_t ImageObjectSection::WriteArray(const void* object, size_t size, const TypeLayoutDesc& desc)
	{
		const auto& write_func = desc.write_func_;
		const auto alignment = std::min(desc.alignment_func_(), 0u);
		const auto element_size = desc.size_;
		for (auto n = 0; n < size; ++n) {
			WriteAlignment(alignment);
			write_func(*this, reinterpret_cast<const uint8_t*>(object) + element_size * n, desc);
		}
	}

	uint32_t ImageObjectSection::WriteRawPointerSizedBytes(uint64_t pointer)
	{
		if (PlatformTypeLayoutParameters::CurrentParameters().Is32bit()) {
			const auto pointer32bit = (uint32_t)pointer;
			return WriteBytes(&pointer32bit, sizeof(pointer32bit));
		}
		return WriteBytes(&pointer, sizeof(pointer));
	}

	uint32_t ImageObjectSection::WriteVTble(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc)
	{
		const auto offset = WriteRawPointerSizedBytes(-1); //dummy value to vtbl
		VTblPointer vtbl_pointer{ .hash_name_ = derived_desc.hash_name_, .offset_ = offset,
								.derived_offset_ = derived_desc.offset_to_base_func_(desc) };
		vtbls_.emplace_back(vtbl_pointer);
		return offset;
	}

	ImageObjectSection& ImageObjectSection::WritePointer(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc, uint32_t& offset_to_base)
	{
		auto section = parent_->NewObjectSection();
		const auto section_index = parent_->GetSectionCount() - 1;
		uint32_t offset_to_base{ 0 };
		TryToGetOffsetFromBase(desc, derived_desc, offset_to_base);
		//write dummy pointer
		const bool is_derived_matched = (desc == derived_desc);
		const auto offset = WriteRawPointerSizedBytes((size_t)-1); //todo
		
		SectionPointer section_pointer{ .section_index_ = section_index, .offset_ = offset, .dervied_offset_ = offset_to_base };
		pointers_.emplace_back(section_pointer);
		//vtbls_.emplace_back({.hash=desc.hash_name_, .derived_offset_ =offset_to_base});
		return section;
	}

	void ImageObjectSection::ComputeHash()
	{
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);
		blake3_hasher_update(&hasher, data_.data(), data_.size());
		blake3_hasher_update(&hasher, pointers_.data(), pointers_.size() * sizeof(pointers_[0]));
		blake3_hasher_update(&hasher, vtbls_.data(), vtbls_.size() * sizeof(vtbls_[0]));
		blake3_hasher_finalize(&hasher, hash_.GetBytes(), hash_.GetHashSize());
	}

	uint32_t ImageObject::Flatten(ImageFreezeObject& object, bool to_merge_sections)const
	{
		SmallVector<ImageObjectSection::Ptr> unique_sections;
		Map<uint32_t, uint32_t> section_remap;
		
		eastl::for_each(sections_.begin(), sections_.end(), [&](auto& sec) { sec.ComputeHash() });

		if (to_merge_sections) {
			Map<Blake3Hash64, uint32_t> unique_hash;
			for (auto n = 0; n < sections_.size(); ++n) {
				auto& section = sections_[n];
				if (const auto iter = unique_hash.find(section.GetHash()); iter != unique_hash.end()) {
					section_remap.insert({ n, iter->second });
				}
				else {
					unique_sections.emplace_back(&section);
					unique_hash.insert({ section.GetHash(), unique_sections.size() });
				}
			}
		}
		else {
			for (auto n = 0; n < sections_.size(); ++n) {
				section_remap.insert({ n, n });
				unique_sections.emplace_back(&sections_[n]);
			}
		}

		SmallVector<uint32_t> unique_section_offset;
		for (auto section : unique_sections) {
			auto offset = section->Flatten(object);
			unique_section_offset.emplace_back(offset);
		}

		//write pointers to memory
		for (auto n = 0; n < unique_sections.size(); ++n) {
			auto section = unique_sections[n];
			for (const auto& section_pointer : section->pointers_) {
				const auto offset_to_pointer = unique_section_offset[n] + section_pointer.offset_;
				auto* frozen_ptr = reinterpret_cast<FrozenPointer*>(object.data_.data() + offset_to_pointer);
				const auto remap_section_index = section_remap[section_pointer.section_index_];
				//rewrite pointer value
				const auto offset_to_data = unique_section_offset[remap_section_index] + section_pointer.offset_;
				frozen_ptr->offset_from_this_ = ((offset_to_data - offset_to_pointer) << 1) << 1;
			}

		}

		//sort vtbl
		eastl::sort(object.vtbls_.begin(), object.vtbls_.end());
	}

	uint32_t ImageFreezeObject::AppendData(const void* data, const size_t size, const size_t alignment)
	{
		const uint32_t align_offset = AlignUp(data_.size(), alignment);
		data_.resize(align_offset + size, 0);
		const auto* src_ptr = reinterpret_cast<const uint8_t*>(data);
		auto* dst_ptr = data_.data() + align_offset;
		eastl::copy(src_ptr,  src_ptr + size, dst_ptr);
		return align_offset;
	}

	void ImageFreezeObject::Serialize(FileArchive& archive)
	{
		archive << platform_;
		archive << (uint64_t)data_.size();
		archive.Serialize(data_.data(), data_.size());
		//write vtbl

	}

	void ImageFreezeObject::UnSerialize(FileArchive& archive)
	{
		archive << platform_;
		uint64_t data_size{ 0u };
		archive << data_size;
		data_.resize(data_size, 0u);
		archive.Serialize(data_.data(), data_.size());
		//read vtbl
	}

	void ImageFreezeObject::ApplyPatches(void* frozen_obj)
	{
		for (auto vtbl : vtbls_) {
			auto type_desc = TypeLayoutDesc::GetTypeLayout(vtbl.hash_name_);
			ApplyVTblValue(frozen_obj, *type_desc, vtbl.derived_offset_, vtbl.offset_);
		}
	}

	void ImageWriter::WriteBytes(const uint8_t* object, size_t size)
	{
		section_.WriteBytes(object, size);
	}

	void ImageWriter::WriteObject(const void* object, const TypeLayoutDesc& desc)
	{
		desc.write_func_(*this, object, desc, desc);
	}

	void ImageWriter::WriteRootObject(const void* object, const TypeLayoutDesc& desc)
	{
		//other work
		desc.write_func_(*this, object, desc, desc);
	}

	void ImageWriter::WriteObjectArray(const void* object, const TypeLayoutDesc& desc, uint32_t num_array)
	{
		const auto write_func = desc.write_func_;
		const auto* curr_object = reinterpret_cast<const uint8_t*>(object);
		for (auto n = 0; n < num_array; ++n) {
			write_func(*this, curr_object+desc.size_*n, desc, desc);
		}
	}
	
	ImageWriter* ImageWriter::WritePointer(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc, uint32_t& offset_to_base)
	{
		return new ImageWriter(section_.WritePointer(desc, derived_desc, offset_to_base));
	}
	uint32_t ImageWriter::WriteAlignment(uint32_t alignment)
	{
		return section_.WriteAlignment(alignment);
	}
	uint32_t ImageWriter::WritePadSize(uint32_t pad_size)
	{
		return section_.WritePadSize(pad_size);
	}
	uint32_t ImageWriter::WriteVTble(const TypeLayoutDesc& desc, const TypeLayoutDesc& derived_desc)
	{
		return section_.WriteVTble(desc, derived_desc);
	}
	uint32_t ImageWriter::GetOffset() const
	{
		return section_.GetOffset();
	}
	const PlatformTypeLayoutParameters& ImageWriter::GetPlatform() const
	{
		return section_.GetParent()->GetPlatform();
	}
	void* UnfreezeMemoryImageObject(const void* frozen_object, const TypeLayoutDesc& desc)
	{
		void* unfreeze_memory = ::operator new(desc.size_);

		//todo unfreeze work
		return unfreeze_memory;
	}
	
	uint32_t ImageObject::Flatten(ImageFreezeObject& object)const
	{
		return uint32_t();
	}
}
