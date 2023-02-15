#include "Renderer/RtRenderShader.h"
#include <folly/Hash.h>
#include <filesystem>

namespace MetaInit::Renderer
{
    const RtRenderShaderParametersMeta::Element& RtRenderShaderParametersMeta::operator[](uint32_t index) const
    {
        assert(index < members_.size() && "index overflow array members");
        return members_[index];
    }

    void RtRenderShaderParametersMeta::AddElement(RtRenderShaderParametersMeta::Element&& element)
    {
        members_.emplace_back(element);
    }

    static inline uint64_t GenerateBinaryHash(const Span<uint8_t>& bin) {
        return folly::hash::commutative_hash_combine_range_generic(bin.size(), folly::Hash{}, bin.begin(), bin.end());
    }

    void RtRenderShaderCode::Serialize(Utils::FileArchive& archive) const
    {
        archive << magic_num_;
        archive << code_binary_.size();
        auto hash_value = GenerateBinaryHash(code_binary_);
        archive << hash_value;
        archive.Serialize(const_cast<uint8_t*>(code_binary_.data()), code_binary_.size());
    }

    void RtRenderShaderCode::UnSerialize(Utils::FileArchive& archive)
    {
        uint32_t size = 0;
        archive << magic_num_;
        archive << size;
        code_binary_.resize(size);
        uint64_t hash_value{ 0 };
        archive << hash_value;
        archive.Serialize(code_binary_.data(), size);
        //verify data check code
        auto hash_gen = GenerateBinaryHash(code_binary_);
        if (hash_gen != hash_value) {
            LOG(FATAL) << "shader code is not correct";
        }
    }
    
    bool RtRenderShader::IsShaderSourceFileChanged(const String& meta_path) const
    {
        //if shader compiled, there must is a meta file 
        if (!std::filesystem::exists(meta_path.c_str())) {
            return false;
        }
        auto meta_file = Utils::FileArchive(meta_path, Utils::FileArchive::eRead);
        String include_path;
        while(std::getline(nullptr,include_path)) {
            const auto file_time = std::filesystem::last_write_time(include_path.c_str());
            const auto sys_time = std::chrono::system_clock::to_time_t(std::chrono::file_clock::to_sys(file_time));
            if (sys_time > compile_time_) {
                return true;
            }
        }
        return false;
    }
}
