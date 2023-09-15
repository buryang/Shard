#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/hash.h"
#include "Graphics/RHI/RHIResources.h"
#include "Graphics/Renderer/RtRenderShader.h"
#include <mutex>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

namespace Shard::Effect
{

	class MINIT_API RtFreeTypeFontVS : public Renderer::RtRenderShader
	{

	};

	class MINIT_API RtFreeTypeFontPS : public Renderer::RtRenderShader
	{

	};

	REGIST_SHADER_IMPL(FreeTypeFontVS,"FontsShader.hlsl", ,EShaderFrequency::eVertex);
	REGIST_SHADER_IMPL(FreeTypeFontPS, "FontsSHader.hlsl", ,EShaderFrequency::eFrag);
	REGIST_SHADER_IMPL(GlyphOutlinesFontVS, "REGIST_SHADER_IMPL",,EShaderFrequency::eVertex);
	REGIST_SHADER_IMPL(GlyphOutlinesFontPS, "REGIST_SHADER_IMPL",,EShaderFrequency::eFrag);

	enum class EAlignmentOp
	{
		eLeftAlign,
		eCenterAlign,
		eRightAlign,
		eTopAlign,
		eBottomAlign,
	};

	using HashType = Utils::SpookyV2Hash32;

	struct GlyphBitmap
	{
		HashType	hash_;
		ivec2	pos_{0u, 0u};
		ivec2	size_{0u, 0u};
		Vector<uint8_t>	data_;
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
		int32_t	ascent_;
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

	class EffectDrawText
	{
	public:
		static void Init();
		static void Unit();
		static void Draw(const String& text, const TextDrawParams& draw_params = TextDrawParams::GetDefaultTextDrawParams());
		static void Draw(const WString& wtext, const TextDrawParams& draw_params = TextDrawParams::GetDefaultTextDrawParams());
	private:
		static void AddFontStyle(const String& name, const Span<uint8_t>& bin);
		static void DrawExecuteFreeType(const WString& wtext, const TextDrawParams& draw_params);
		static void DrawExecuteGlyphOutlines(const WString& wtext, const TextDrawParams& draw_params);
		static void UpdateAtlas(); //todo
	private:
		static Map<String, FontStyleInfo>	font_styleLUT_;
		static Map<HashType, GlyphAtlas>	glyphs_atlasLUT_;
		static Map<HashType, GlyphBitmap>	glyphs_neededLUT_;

		static std::mutex	atlas_mutex_;
		static RHI::RHITexture::Ptr	atlas_{ nullptr };
		static RtFreeTypeFontVS	vertex_shader_;
		static RtFreeTypeFontPS	pixel_shader_;
	};
}
