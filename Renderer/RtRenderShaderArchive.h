#pragma once
#include "Utils/Hash.h"
#include "Utils/FileArchive.h"
#include "RHI/RHIGlobalEntity.h"
#include "Renderer/RtRenderShader.h"
#include <shared_mutex>

namespace MetaInit::Renderer {
	using Utils::FileArchive;
	struct PipelineStateObjectDesc {
		using HashVal = RtRenderShader::HashType;
		union {
			struct ComputePipelineObject {
				HashVal	compute_shader_;
			} compute_desc_;
			struct GFXPipelineObject {
				HashVal stage_shaders[EShaderFrequency::eNum];
			} gfx_desc_;
			struct RayTracingPipelinObject {

			} raytracing_desc_;
		};
		xxx	type_;
	};

	FileArchive& operator<<(FileArchive& ar, PipelineStateObjectDesc& pso);

	enum class EShaderCompressMode : uint8_t
	{
		eNone = 0x0,
		eCompressed = 0x1,
	};

	struct ShaderSectionDesc {
		uint32_t	offset_{ 0u };
		uint32_t	size_{ 0u };
		EShaderCompressMode	compress_{ EShaderCompressMode::eNone };
		Utils::Blake3Hash64	hash_;
	};
	
	enum class EIRLanguage {
		eUnkown,
		eSPIRV,
	};

	struct ShaderArchiveHeader {
		static constexpr const uint32_t	SHADER_HEADER_SIZE = 1000u;
		static constexpr const uint32_t SHADER_HEADER_MAGIC_NUM = ;
		uint32_t	magic_{ SHADER_HEADER_MAGIC_NUM };
		EIRLanguage	ir_{ EIRLanguage::eSPIRV };
		EShaderModel	shader_model_;
		//platfom
		SmallVector<ShaderSectionDesc>	sections_;
		FORCE_INLINE bool IsHeaderFull() const {
			return sizeof(magic_) + sizeof(sections_.size() * sizeof(ShaderSectionDesc)) >= SHADER_HEADER_SIZE;
		}
	};

	FileArchive& operator<<(FileArchive& ar, ShaderArchiveHeader& archive_header);

	class RtRenderShaderArchive {
	public:
		using Ptr = RtRenderShaderArchive*;
		using HashType = RtRenderShaderCode::HashType;
		void Serialize(FileArchive& ar, bool use_compress=false);
		void Unserialize(FileArchive& ar);
		/*how to deal with shader code update*/
		void AddShaderCode(const RtRenderShaderCode& shader_code);
		void RemoveShaderCode(const HashType& hash);
		const RtRenderShaderCode& GetShaderCode(uint32_t shader_index) const;
		const RtRenderShaderCode& GetShaderCode(const HashType& hash) const;
		const uint64_t GetArchiveSize() const;
		const uint32_t GetShadersCount() const;
		void Empty();
		bool IsEmpty() const;
	private:
		EIRLanguage	ir_{ EIRLanguage::eSPIRV };
		EShaderModel	shader_model_;
		Vector<HashType>	shader_hashes_;
		Vector<RtRenderShaderCode>	shader_codes_;
		mutable std::shared_mutex	shader_rw_mutex_;
	};

}
