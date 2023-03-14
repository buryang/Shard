#include "Renderer/RtRenderShaderArchive.h"

namespace MetaInit::Renderer {
	void RtRenderShaderArchive::Serialize(FileArchive& ar, bool use_compress)
	{
	}
	void RtRenderShaderArchive::Unserialize(FileArchive& ar)
	{
	}
	void RtRenderShaderArchive::AddShaderCode(const RtRenderShaderCode& shader_code)
	{
		std::unique_lock<std::shared_mutex> lock(shader_rw_mutex_);
		if (auto iter = eastl::find(shader_hashes_.begin(), shader_hashes_.end(), shader_code.hash_);
			iter != shader_hashes_.end()) {
			const auto shader_index = static_cast<uint32_t>(iter - shader_hashes_.begin());
			if (shader_code.time_tag_ > shader_codes_[shader_index].time_tag_) {
				//update old code
				lock.unlock();
				RemoveShaderCode(shader_code.hash_);
				lock.lock();
			}
			else {
				return;
			}
		}

		shader_hashes_.emplace_back(shader_code.hash_);
		shader_codes_.emplace_back(shader_code);
	}

	const RtRenderShaderCode& RtRenderShaderArchive::GetShaderCode(uint32_t shader_index) const
	{
		return shader_codes_[shader_index];
	}

	const RtRenderShaderCode& RtRenderShaderArchive::GetShaderCode(const HashType& hash) const
	{
		std::shared_lock<std::shared_mutex> lock(shader_rw_mutex_);
		if (auto iter = eastl::find(shader_hashes_.begin(), shader_hashes_.end(), hash); iter != shader_hashes_.end()) {
			return GetShaderCode(static_cast<uint32_t>(iter-shader_hashes_.begin()));
		}
		return RtRenderShaderCode();
	}

	const uint64_t RtRenderShaderArchive::GetArchiveSize() const
	{
		return uint64_t();
	}

	const uint32_t RtRenderShaderArchive::GetShadersCount() const
	{
		std::shared_lock<std::shared_mutex> lock(shader_rw_mutex_);
		return shader_hashes_.size();
	}

	void RtRenderShaderArchive::Empty()
	{
		std::unique_lock<std::shared_mutex> lock(shader_rw_mutex_);
		shader_hashes_.clear();
		shader_codes_.clear();
	}

	bool RtRenderShaderArchive::IsEmpty() const
	{
		std::shared_lock<std::shared_mutex> lock(shader_rw_mutex_);
		return shader_hashes_.empty();
	}

	void RtRenderShaderArchive::RemoveShaderCode(const HashType& hash)
	{
		std::unique_lock<std::shared_mutex> lock(shader_rw_mutex_);
		if (auto iter = std::find(shader_hashes_.begin(), shader_hashes_.end(), hash);
			iter != shader_hashes_.end()) {
			const auto index = static_cast<size_t>(iter - shader_hashes_.begin());
			std::swap(shader_hashes_[index], shader_hashes_.back());
			std::swap(shader_codes_[index], shader_codes_.back());
			shader_hashes_.pop_back();
			shader_codes_.pop_back();
		}
	}
	
	FileArchive& operator<<(FileArchive& ar, ShaderArchiveHeader& archive_header)
	{
		PCHECK(!archive_header.IsHeaderFull()) << "shader archive header overflow";
		ar << archive_header.magic_;
		if (ar.IsReading()) {
			decltype(archive_header.sections_)::size_type size{ 0u };
			ar << size;
			archive_header.sections_.resize(size);
		}
		else {
			ar << archive_header.sections_.size();
		}
		for (auto& sec : archive_header.sections_) {
			ar << sec;
		}
	}
}
}