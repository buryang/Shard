#pragma once
#include "Scene/Primitive.h"
#include "Render/RenderShader.h"
#include "Render/RenderShaderFactory.h"
#include "Render/RenderShaderArchive.h"
#include "HAL/HALShader.h"

namespace Shard::HAL {

    using Shard::Render::EShaderModel;
    class HALShaderLibraryInterface
    {
    public:
        using Ptr = HALShaderLibraryInterface*;
        using SharedPtr = std::shared_ptr<HALShaderLibraryInterface>;
        enum class ELibrayType {
            eNative,
            ePseudo,
        };
        virtual void AddHALShader(const HALShaderInitializer& initializer) { LOG(ERROR) << "add shader from initializer not implement"; }
        virtual void AddHALShader(HALShader* shader) { LOG(ERROR) << "add raw shader not implemented"; };
        virtual HALShader* GetHALShader(uint32_t shader_index) { return nullptr; }
        virtual HALShader* GetHALShader(HALShader::HashType shader_hash) {    return nullptr; }
        virtual uint32_t GetHALShaderIndex(HALShader::HashType shader_hash) { return -1; }
        virtual size_type GetShaderCount() const { return 0u; }
        FORCE_INLINE ELibrayType GetLibrayType() const { return library_model_; }
        FORCE_INLINE EShaderModel GetShaderModel() const { return shader_model_; }
        virtual ~HALShaderLibraryInterface() {}
    protected:
        EShaderModel    shader_model_{ EShaderModel::eUnkown };
        ELibrayType     library_model_{ ELibrayType::eNative };
    };

    template<typename HAL>
    concept RequiredHAL = requires { &HAL::CreateShader; };

    template<RequiredHAL HALEntity>
    class HALPseudoShaderLibrary : public HALShaderLibraryInterface 
    {
    public:
        HALPseudoShaderLibrary() :library_model_(ELibrayType::ePseudo) {}
        explicit HALPseudoShaderLibrary(const RenderShaderArchive* archive) :library_model_(ELibrayType::ePseudo) {
            BindArchive(archive);
        }
        void BindArchive(const RenderShaderArchive* archive) {
            shader_archive_ = archive;
            //todo whether create rhi shaders
        }
        HALShader* GetHALShader(uint32_t shader_index)override {
            if (auto iter = shader_cache_.find(shader_index); iter != shader_cache_.end()) {
                return iter->second;
            }

            auto shader_code = shader_archive_->GetShaderCode(shader_index);
            auto* shader_ptr = HALEntity::CreateShader(shader_code); // create shader
            assert(shader_ptr != nullptr);
            shader_cache_.insert({ shader_index, shader_ptr });
            return shader_ptr;
        }
        HALShader* GetHALShader(HALShader::HashVal shader_hash) override {

        }
        
        uint32_t GetHALShaderIndex(HALShader::HashVal shader_hash) override {
            shader_archive_.
        }
    private:
        RenderShaderArchive* shader_archive_{ nullptr };
        Map<uint32_t, HALShader*>    shader_cache_;
    };

    //to do redesign this interface
    //https://www.tomlooman.com/psocaching-unreal-engine/
    class HALPipelineStateObjectLibraryInterface
    {
    public:
        virtual void Init(void) {}
        virtual void UnInit(void);
        virtual HALPipelineStateObject* GetOrCreatEPipeline(const HALPipelineStateObjectInitializer& initializer);
        constexpr size_type GetPSOCount()const;
        virtual ~HALPipelineStateObjectLibraryInterface();
    protected:
        virtual HALPipelineStateObject* CreatEPipelineImpl(const HALPipelineStateObjectInitializer& initializer) = 0;
        virtual void DestoryPipelineImpl(HALPipelineStateObject*) = 0;
    protected:
        Map<HALPipelineStateObjectInitializer, HALPipelineStateObject*> pso_cache_;
        std::shared_mutex   cache_mutex_;
    };
}
