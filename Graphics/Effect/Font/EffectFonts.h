#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/hash.h"
#include "Graphics/RHI/RHIResources.h"
#include "Graphics/Renderer/RtRenderShader.h"
#include "Graphics/Renderer/RtRenderGraphBuilder.h"
#include <mutex>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

namespace Shard::Effect
{
	using namespace Renderer;

	class MINIT_API RtFreeTypeFontVS : public Renderer::RtRenderShader
	{
	public:
		bool IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation) override;
		RtRenderShaderParametersMeta* GetShaderParametersMeta()override;
	};

	class MINIT_API RtFreeTypeFontPS : public Renderer::RtRenderShader
	{
	public:
		bool IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation) override;
		RtRenderShaderParametersMeta* GetShaderParametersMeta()override;
		
	};

	class MINIT_API RtGlyphOutlinesFontVS : public Renderer::RtRenderShader
	{
	public:
		bool IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation) override;
		RtRenderShaderParametersMeta* GetShaderParametersMeta()override;
	};

	class MINIT_API RtGlyphOutlinesFontPS : public Renderer::RtRenderShader
	{
	public:
		bool IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation) override;
		RtRenderShaderParametersMeta* GetShaderParametersMeta()override;
	};

	REGIST_SHADER_IMPL(FreeTypeFontVS,"Fonts/VSFontsShader.hlsl", "main", EShaderFrequency::eVertex, RtFreeTypeFontVS);
	REGIST_SHADER_IMPL(FreeTypeFontPS, "Fonts/PSFontsSHader.hlsl", "main", EShaderFrequency::eFrag, RtFreeTypeFontPS);
	REGIST_SHADER_IMPL(GlyphOutlinesFontVS, "Fonts/VSFontsShader.hlsl", "main", EShaderFrequency::eVertex, RtGlyphOutlinesFontVS);
	REGIST_SHADER_IMPL(GlyphOutlinesFontPS, "Fonts/PSFontsSHader.hlsl", "main", EShaderFrequency::eFrag, RtGlyphOutlinesFontPS);

	enum class EAlignmentOp
	{
		eLeftAlign,
		eCenterAlign,
		eRightAlign,
		eTopAlign,
		eBottomAlign,
	};

	struct GlpyHash
	{
		uint32_t	code_ : 16;
		uint32_t	sdf_ : 1;
		uint32_t	font_style_ : 5;
		uint32_t	size_ : 10;

		GlpyHash(uint32_t packed) {
			*reinterpret_cast<uint32_t*>(this) = packed;
		}
		operator uint32_t() const {
			return *reinterpret_cast<const uint32_t*>(this);
		}
	};

	using HashType = GlpyHash;

	struct GlyphBitmap
	{
		HashType	hash_;
		ivec2	size_{0u, 0u}; //bitmap size
		//#if
		Vector<uint8_t>	data_;
		GlyphBitmap() = default;
	};

	struct GlyphAtlas
	{
		HashType	hash_;
		ivec2	pos_;
		ivec2	size_;
	};

	struct FontStyleInfo
	{
		String	name_;
		stbtt_fontinfo	stb_info_;
		int32_t	ascent_{ 0u };
		int32_t	descent_{ 0u };
		int32_t	linegap_{ 0u };
	};

	enum class EDrawAlgo
	{
		eFreeType = 0x1,
		eSDF = 0x2, //[Green 2007]
		ePreRender = eFreeType | eSDF,
		eGlyphOutlines = 0x4, //GPU-Centered Font Rendering Directly from Glyph Outlines
	};

	//cursor: point to current input position
	struct Cursor
	{
		vec2	pos_;
		vec2	size_;
	};

	struct TextDrawParams
	{
		String	style_name_;
		EAlignmentOp	h_align_op_{ EAlignmentOp::eLeftAlign };
		EAlignmentOp	v_align_op_{ EAlignmentOp::eTopAlign };
		vec2	pos_{ 0, 0 };
		uint32_t	size_{ 32u };
		uint32_t	paddingX_{ 0u };//padding for charactor horizontal
		uint32_t	paddingY_{ 0u };//padding for charactor vertical

		float rotation_{ 0.f };
		Cursor	cursor_; //textline current cursor
		
		vec4 color_{255, 255, 255, 255}; //you set color for text
		vec4 shadow_color_{0, 0, 0, 0};//background color
		float h_wrap_ = -1;
		//SDF special params
		float bolden_{ 0.f }; 
		float softness_{ 0.1f };  

		static const TextDrawParams& GetDefaultTextDrawParams();

	};

	//todo refactor like debugview
	class EffectDrawText
	{
	public:
		static void Init();
		static void Unit();
		static void Draw(Renderer::RtRendererGraph& graph, const String& text, const TextDrawParams& draw_params = TextDrawParams::GetDefaultTextDrawParams());
		static void Draw(Renderer::RtRendererGraph& graph, const WString& wtext, const TextDrawParams& draw_params = TextDrawParams::GetDefaultTextDrawParams());
	private:
		static uint32_t AddFontStyle(const String& name, const Span<uint8_t>& bin);
		static void DrawExecuteFreeType(Renderer::RtRenderGraphBuilder& builder, const WString& wtext, const TextDrawParams& draw_params);
		static void DrawExecuteGlyphOutlines(Renderer::RtRenderGraphBuilder& builder, const WString& wtext, const TextDrawParams& draw_params);
		static void UpdateAtlas(float scale = 1.f); //todo
	private:
		static Vector<FontStyleInfo>	font_styleLUT_;
		static Map<HashType, GlyphAtlas>	glyphs_atlasLUT_;
		static Map<HashType, GlyphBitmap>	glyphs_neededLUT_;

		static std::mutex	atlas_mutex_;
		static RHI::RHIBuffer::Ptr	vertex_buffer_;
		static RHI::RHIBuffer::Ptr	index_buffer_;
		static RHI::RHITexture::Ptr	atlas_{ nullptr };
		static RHI::RHIPipelineStateObject::Ptr pso_{ nullptr };
	};
}
