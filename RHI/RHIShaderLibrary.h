#pragma once
#include "Scene/Primitive.h"
#include "Renderer/RtRenderShader.h"
#include "Renderer/RtRenderShaderFactory.h"
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
		virtual RHIShader::Ptr GetRHIShader(RHIShader::HashType shader_hash) { return nullptr; }
		virtual uint32_t GetRHIShaderIndex(RHIShader::HashType shader_hash) { return -1; }
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

	enum
	{
		MAX_RENDER_TARGETS_NUM = 8,
		MAX_VERTEX_DECLARATION_NUM = 17,
	};

	struct RHIBlendStateInitializer
	{
		using HashType = Utils::Blake3Hash64;
		struct BlendAttachment
		{
			enum class EBlendOperation
			{
				eAdd,
				eSub,
				eMin,
				eMax,
				eNum,
			};
			enum class EBlendFactor
			{
				eZero,
				eOne,
				eSrcColor,
				eInvSrcColor,
				eSrcAlpha,
				eInvSrcAlpha,
				eDstColor,
				eInvDstColor,
				eDstAlpha,
				eInvDstAlpha,
			};
			enum class EBlendColorMask
			{
				eNone = 0,
				eRed = 1<<0,
				eGreen = 1<<1,
				eBlue = 1<<2,
				eAlpha = 1<<3,

				eRGB = eRed | eGreen | eBlue,
				eRGBA = eRGB | eAlpha,
				//todo
			};
			EBlendFactor	color_src_factor_{};
			EBlendFactor	color_dst_factor_{};
			EBlendOperation	color_blend_op_{};

			EBlendFactor	alpha_src_factor_{};
			EBlendFactor	alpha_dst_factor_{};
			EBlendOperation	alpha_blend_op_{};
			EBlendColorMask	color_write_mask_{};
		};
		SmallVector<BlendAttachment, MAX_RENDER_TARGETS_NUM, false>	attachments_;
		static HashType ComputeHash(const RHIBlendStateInitializer& initializer);
	};

	struct RHIDepthStencilStateInitializer
	{
		using HashType = Utils::Blake3Hash64;
		enum class EDepthCompareOp
		{
			eNever,
			eLess,
			eEqual,
			eLessOrEqual,
			eGreater,
			eNotEqual,
			eGreaterOrEqual,
			eAlways,
		};
		enum class EStencilOp
		{
			eKeep,
			eZero,
			eReplace,
			eIncrAndClamp,
			eDecrAndClamp,
			eInvert,
			eIncrAndWarp,
			eDecrAndWarp,
		};
		bool	depth_test_enable_{};
		bool	depth_write_enable_{};
		EDepthCompareOp	depth_compare_op_{};
		bool	depth_bound_test_enable_{};
		bool	stencil_test_enable_{};
		EStencilOp	front_op_;
		EStencilOp	back_op_;
		vec2	depth_bounds_;
		static HashType ComputeHash(const RHIDepthStencilStateInitializer& initializer);
	};

	struct RHIRasterizationStateInitializer
	{
		using HashType = Utils::Blake3Hash64;
		enum class EPolygonMode
		{
			eFill, //d3d solid?
			eLine, //d3d wireframe ?
			ePoint,
		};
		enum class EFrontFace
		{
			eCClk, //counter clock
			eClk,
		};
		enum class ECullingMode
		{
			eNone = 0x0,
			eFront = 0x1,
			eBack = 0x2,
			eAll = eFront | eBack,
		};
		EPolygonMode	poly_mode_{ EPolygonMode::eFill };
		EFrontFace	front_face_{ EFrontFace::eClk };
		ECullingMode	culling_mode_{ ECullingMode::eNone };
		float	depth_bias_;
		static HashType ComputeHash(const RHIRasterizationStateInitializer& initializer);
	};

	struct RHIVertexInputStateInitializer
	{
		using HashType = Utils::Blake3Hash64;
		struct Element
		{
			uint32_t	binding_{ 0u };
			uint32_t	stride_{ 0u };
			uint32_t	index_{ 0u };
			uint32_t	offset_{ 0u };
			EPixFormat	format_{ EPixFormat::eUnkown };
		};
		SmallVector<Element, MAX_VERTEX_DECLARATION_NUM>	declarations_;
		static HashType ComputeHash(const RHIVertexDeclarationElementInitializer& initializer);
	};

	struct RHIPipelineStateObjectInitializer
	{
		enum class EType
		{
			eGFX,
			eCompute,
			eRayTrace,
			eNum,
		};
		EType	type_;
		union {
			struct GFXInitializer
			{
				RHIVertexShader::Ptr	vertex_shader_{ nullptr };
				RHIHullShader::Ptr	hull_shader_{ nullptr };
				RHIDomainShader::Ptr	domain_shader_{ nullptr };
				RHIGeometryShader::Ptr	geometry_shader_{ nullptr };
				RHIPixelShader::Ptr	pixel_shader_{ nullptr };
				RHIVertexInputStateInitializer	vertex_input_state_;
				RHIBlendStateInitializer	blend_state_;
				RHIDepthStencilStateInitializer	depth_stencil_state_;
				RHIRasterizationStateInitializer	rasterization_state_;
				//assembly state only has a topology flags
				EInputTopoType	primitive_topology_{ EInputTopoType::eUnkown };
			}gfx_;
			struct ComputeInitializer
			{
				RHIComputeShader::Ptr	compute_shader_{ nullptr };
			}compute_;
			struct RayTraceInitializer
			{

			}raytrace_;
		};
	};


	//to do redesign this interface
	class RHIPipelineStateObjectLibraryInterface
	{
	public:
		using Ptr = RHIPipelineStateObjectLibraryInterface;
		RHIPipelineStateObject::Ptr GetOrCreatePipeline(const RHIPipelineStateObjectInitializer& initializer);
		virtual ~RHIPipelineStateObjectLibraryInterface() {}
	protected:
		virtual RHIPipelineStateObject::Ptr CreatePipelineImpl(const RHIPipelineStateObjectInitializer& initializer) = 0;
	protected:
		Map<RHIPipelineStateObjectInitializer, RHIPipelineStateObject::Ptr>	pso_cache_;
		std::shared_mutex	cache_mutex_;
	};
}
