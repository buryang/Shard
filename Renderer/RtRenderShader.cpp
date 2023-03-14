#include "Renderer/RtRenderShader.h"
#include <folly/Hash.h>
#include <zlib.h>
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
        archive.Serialize(const_cast<uint8_t*>(code_binary_.data()), code_binary_.size());
        auto hash_value = GenerateBinaryHash(code_binary_);
        archive << hash_value;
    }

    void RtRenderShaderCode::UnSerialize(Utils::FileArchive& archive)
    {
        uint32_t size = 0;
        archive << magic_num_;
        archive << size;
        code_binary_.resize(size);
        archive.Serialize(code_binary_.data(), size);
        //verify data check code
        uint64_t hash_value{ 0 };
        archive << hash_value;
        auto hash_gen = GenerateBinaryHash(code_binary_);
        if (hash_gen != hash_value) {
            LOG(FATAL) << "shader code is not correct";
        }
    }
    
    HashType RtRenderShader::MakeShaderHash(RtRenderShader::Ptr shader, uint32_t permutation_id)
    {
        HashType hash;
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        auto shader_hash = shader->GetShaderHash();
        blake3_hasher_update(&hasher, shader.GetBytes(), shader.GetHashSize());
        assert(permutation_id < shader->GetShaderPermutationCount());
        blake3_hasher_update(&hasher, &permutation_id, sizeof(permution_id));
        return hash;
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
    RtRenderShaderCode& RtRenderShaderCode::Compress()
    {
        if (/*need compress*/0)
            const auto budget_size = uint32_t(code_binary_.size() * 1.01 + 12);
            Vector<uint8_t> compress(budget_size);
            uint32_t compressed_size{ 0u };
            auto ret = compress(compress.data(), &compressed_size, code_binary_.data(), code_binary_.size());
            PCHEK(Z_OK == ret) << "failed to compress shader code";
            compress.resize(compressed_size);
            std::swap(compress, code_binary_);
            //change magic num to compress todo 
        }
        return *this;
    }

    RtRenderShaderCode& RtRenderShaderCode::DeCompress()
    {
        if (/*need decompress*/0) {
            const auto budget_size = uint32_t(code_binary_.size() * 1.01 + 12);
            Vector<uint8_t> uncompress(budget_size);
            uint32_t decompressed_size{ 0u };
            auto ret = uncompress(compress.data(), &decompressed_size, code_binary_.data(), code_binary_.size());
            PCHEK(Z_OK == ret) << "failed to uncompress shader code";
            uncompress.resize(decompressed_size);
            std::swap(uncompress, code_binary_);
            //change magic num to uncompress todo
        }
        return *this;
    }
}
