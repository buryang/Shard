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

	constexpr SizeType FileArchive::Tell()const
	{
		if (IsReading()) {
			return archive_stream_.tellg();
		}
		return archive_stream_.tellp();
	}

	FileArchive& FileArchive::Seek(PositionType pos)
	{
		if (IsReading()) {
			archive_stream_.seekg(pos);
		}
		archive_stream_.seekp(pos);
		return *this;
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

	constexpr uint64_t CalcFileArchiveCrc(const FileArchive& archive, FileArchive::PositionType beg, FileArchive::PositionType end) {
		uint64_t hash{ 0 };
		//only reading mode supoort hash
		if (archive.IsReading()) {
			assert(end > beg);
			constexpr const uint32_t BUFFER_SIZE = 1024;
			alignas(32) uint8_t bytes[BUFFER_SIZE];
			//const auto backup_pos = archive.Tell();
			archive.Seek(beg);
			auto curr_pos = beg;
			blake3_hasher hasher;
			blake3_hasher_init(&hasher);
			while (curr_pos + BUFFER_SIZE <= end) {
				archive.Serialize(bytes, BUFFER_SIZE);
				//update hash
				blake3_hasher_update(hasher, bytes, BUFFER_SIZE);
				curr_pos += BUFFER_SIZE;
			}
			const auto read_size = end - curr_pos;
			archive.Serialize(bytes, read_size);
			//update hash
			blake3_hasher_update(hasher, bytes, read_size);
			blake3_hasher_finalize(hasher, &hash, sizeof(hash));
			//archive.Seek(backup_pos);
		}

		return hash;
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