#include "Utils/FileArchive.h"

namespace MetaInit::Utils {
	
	FileArchive::FileArchive(const String& file_path, EArchiveMode mode) {
		Open(file_path, mode);
	}

	void FileArchive::Open(const String& file_path, EArchiveMode mode)
	{
		archive_mode_ = mode;
		if (archive_stream_.is_open()) {
			Close();
		}
		
		uint32_t open_mode = std::ios::binary;
		if (mode == eRead) {
			open_mode |= std::ios::in;
		}
		else if (mode & eWrite) {
			open_mode |= std::ios::out;
			if ((mode & eAppend) == eAppend)
				open_mode |= std::ios::app;
		}
		archive_stream_.open(file_path.c_str(), open_mode);
		if(!archive_stream_.is_open())
			LOG(ERROR) << "open archive file " << file_path.c_str() << "failed";
	}

	void FileArchive::Close()
	{
		archive_mode_ = eUndefined;
		archive_stream_.close();
	}
	
	FileArchive& FileArchive::Serialize(void* data, uint32_t size)
	{
		if (IsReading()) {
			archive_stream_.read(reinterpret_cast<char*>(data), size);
		}
		else {
			archive_stream_.write(reinterpret_cast<char*>(data), size);
		}
		return *this;
	}

	FileImageSection& FileImage::AddEmptySection()
	{
		static std::atomic_bool section_atomic{ false };
		SpinLock spin_lock(section_atomic);
		image_sections_.emplace_back(FileImageSection());
		return image_sections_.back();
	}
	FileImageSection& FileImage::AddSection(FileImageSection&& section)
	{
		auto& zero_section = AddEmptySection();
		zero_section = section;
		return zero_section;
	}
}