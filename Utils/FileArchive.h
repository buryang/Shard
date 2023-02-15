#pragma once
#include "Utils/CommonUtils.h"
#include <fstream>

namespace MetaInit::Utils {

	class MINIT_API FileArchive {
	public:
		enum EArchiveMode {
			eUndefined = 0,
			eRead = 1 << 0,
			eWrite = 1 << 1,
			eAppend = eWrite | 0x100,
		};
		FileArchive(const String& file_path, EArchiveMode mode);
		void Open(const String& file_path, EArchiveMode mode);
		void Close();
		bool IsReading()const { return archive_mode_ & eRead; }
		FileArchive& Serialize(void* data, uint32_t size);
		template <typename T>
		friend FileArchive& operator << (FileArchive& archive, T&& value) {
			assert(archive.archive_mode_ != FileArchive::eUndefined);
			if (archive.IsReading()) {
				archive.archive_stream_ >> value;
			}
			else {
				archive.archive_stream_ << value;
			}
			return archive;
		}
	private:
		EArchiveMode	archive_mode_{ eUndefined };
		std::fstream	archive_stream_;
	};

	struct FileImageSectionDescriptor {
		using Ptr = FileImageSectionDescriptor*;
		uint64_t	offset_{ 0 };
		uint64_t	size_{ 0 };
		Ptr	parent_{ nullptr };
		SmallVector<Ptr>	children_;
		FORCE_INLINE bool IsParent()const { return !children_.empty(); }
		FORCE_INLINE bool IsChild()const { return parent_ != nullptr; }
	};

	class MINIT_API FileImageSection {
	public:
		friend class FileImageWriter;
		FileImageSection() = default;
		FileImageSection(const FileImageSectionDescriptor& desc);
		FileImageSection& Load(FileArchive& archive);
		FileImageSection& Save(FileArchive& archive);
		void Init(const FileImageSectionDescriptor& desc);
		/*
		bool operator==(const FileImageSection& rhs) {
			return 
		}
		*/
	private:
		FileImageSectionDescriptor	desc_;
		Vector<uint8_t>	image_;
	};

	class MINIT_API FileImage {
	public:
		FileImage& Load(FileArchive& archive);
		FileImage& Save(FileArchive& archive);
		FileImageSection& AddEmptySection();
		FileImageSection& AddSection(FileImageSection&& section);
	private:
		SmallVector<FileImageSection>	image_sections_;
	};


	class MINIT_API FileImageWriter {
	public:
		FileImageWriter(FileImageSection& section):section_(section) {}
		FileImageWriter(FileImage& image):section_(image.AddEmptySection()) {}
		FileImageWriter& Write(const void* data, FileImageSectionDescriptor& desc);
	private:
		FileImageSection& section_;
	};
}