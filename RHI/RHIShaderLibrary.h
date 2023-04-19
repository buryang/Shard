#pragma once
#include "Renderer/RtRenderShader.h"
#include "Renderer/RtRenderShaderArchive.h"
#include "RHI/RHIShader.h"

namespace MetaInit::RHI {

	using MetaInit::Renderer::EShaderModel;
	class RHIShaderLibraryInterface
	{
	public:
		using Ptr = RHIShaderLibraryInterface*;
		using SharedPtr = std::shared_ptr<RHIShaderLibraryInterface>;
		enum class ELibrayType {
			eNative,
			ePseudo,
		};
		virtual void AddRHIShader(RHIShader::Ptr shader) { LOG(ERROR) << "add shader not implemented"; };
		virtual RHIShader::Ptr GetRHIShader(uint32_t shader_index) { return nullptr; }
		virtual RHIShader::Ptr GetRHIShader(RHIShader::HashVal shader_hash) { return nullptr; }
		virtual uint32_t GetRHIShaderIndex(RHIShader::HashVal shader_hash) { return -1; }
		FORCE_INLINE ELibrayType GetLibrayType() const { return library_model_; }
		FORCE_INLINE EShaderModel GetShaderModel() const { return shader_model_; }
		virtual ~RHIShaderLibraryInterface() {}
	protected:
		EShaderModel	shader_model_{ EShaderModel::eUnkown };
		ELibrayType	library_model_{ ELibrayType::eNative };
	};

	template<typename RHI, typename=void>
	struct HasCreateShaderFunc :public std::false_type {};

	template<typename RHI>
	struct HasCreateShaderFunc<RHI, std::void_t<typename RHI::CreateShader>> : public std::true_type {};

	template<typename RHIEntity>
	requires HasCreateShaderFunc<RHIEntity>::value
	class RHIPseudoShaderLibrary : public RHIShaderLibraryInterface 
	{
	public:
		RHIPseudoShaderLibrary() :library_model_(ELibrayType::ePseudo) {}
		explicit RHIPseudoShaderLibrary(const RtRenderShaderArchive::Ptr archive) :library_model_(ELibrayType::ePseudo) {
			BindArchive(archive);
		}
		void BindArchive(const RtRenderShaderArchive::Ptr archive) {
			shader_archive_ = archive;
			//todo whether create rhi shaders
		}
		RHIShader::Ptr GetRHIShader(uint32_t shader_index)override {
			if (auto iter = shader_cache_.find(shader_index); iter != shader_cache_.end()) {
				return iter->second;
			}

			auto shader_code = shader_archive_->GetShaderCode(shader_index);
			auto* shader_ptr = RHIEntity::CreateShader(shader_code); // create shader
			assert(shader_ptr != nullptr);
			shader_cache_.insert({ shader_index, shader_ptr });
			return shader_ptr;
		}
		RHIShader::Ptr GetRHIShader(RHIShader::HashVal shader_hash) override {

		}
		
		uint32_t GetRHIShaderIndex(RHIShader::HashVal shader_hash) override {
			shader_archive_.
		}
	private:
		RtRenderShaderArchive::Ptr shader_archive_{ nullptr };
		Map<uint32_t, RHIShader::Ptr>	shader_cache_;
	};

	class RHIPipelineStateObjectLibraryInterface
	{
	public:
		enum EPSOType {
			e
		};
		virtual Get() = 0;
	};
}
