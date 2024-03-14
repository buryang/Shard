#include "Render/RenderShaderArchive.h"

namespace Shard::Render {
    void RenderShaderArchive::Serialize(FileArchive& ar, bool use_compress)
    {
        ar << ir_;
        ar << shader_model_;
        ar << shader_codes_.size();
        for (auto& code : shader_codes_) {
            if (use_compress) {
                code.Compress();
            }
            ar << code;
        }
    }
    void RenderShaderArchive::Unserialize(FileArchive& ar)
    {
        ar << ir_;
        ar << shader_model_;
        size_t shader_code_size{ 0u };
        ar << shader_code_size;
        for (auto n = 0; n < shader_code_size; ++n) {
            RenderShaderCode shader_code;
            ar << shader_code;
            shader_codes_.emplace_back(shader_code);
        }
    }
    void RenderShaderArchive::AddShaderCode(const RenderShaderCode& shader_code)
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

    const RenderShaderCode& RenderShaderArchive::GetShaderCode(uint32_t shader_index) const
    {
        return shader_codes_[shader_index];
    }

    const RenderShaderCode& RenderShaderArchive::GetShaderCode(const HashType& hash) const
    {
        std::shared_lock<std::shared_mutex> lock(shader_rw_mutex_);
        if (auto iter = eastl::find(shader_hashes_.begin(), shader_hashes_.end(), hash); iter != shader_hashes_.end()) {
            return GetShaderCode(static_cast<uint32_t>(iter-shader_hashes_.begin()));
        }
        return RenderShaderCode();
    }

    const uint64_t RenderShaderArchive::GetArchiveSize() const
    {
        return uint64_t();
    }

    const uint32_t RenderShaderArchive::GetShadersCount() const
    {
        std::shared_lock<std::shared_mutex> lock(shader_rw_mutex_);
        return shader_hashes_.size();
    }

    void RenderShaderArchive::Clear()
    {
        std::unique_lock<std::shared_mutex> lock(shader_rw_mutex_);
        shader_hashes_.clear();
        shader_codes_.clear();
    }

    bool RenderShaderArchive::IsEmpty() const
    {
        std::shared_lock<std::shared_mutex> lock(shader_rw_mutex_);
        return shader_hashes_.empty();
    }

    void RenderShaderArchive::RemoveShaderCode(const HashType& hash)
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
    
    FileArchive& operator<<(FileArchive& ar, PipelineStateObjectDesc& pso)
    {
        // TODO: 在此处插入 return 语句
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