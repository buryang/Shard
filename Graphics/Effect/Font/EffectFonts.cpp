#include "Core/RenderGlobalParams.h"
#include "Graphics/RHI/RHIGlobalEntity.h"
#include "Graphics/Effect/Font/EffectFonts.h"
#include "stb/stb_rect_pack.h"
#include <filesystem>

namespace fs =  std::filesystem;

namespace Shard::Effect
{
	REGIST_PARAM_TYPE(STRING, FONT_ROOT_PATH, "");
	REGIST_PARAM_TYPE(UINT, FONT_DRAW_ALGO, EDrawAlgo::eFreeType);
	REGIST_PARAM_TYPE(UINT, FONT_SDF_PADDING_VAL, 5);
	REGIST_PARAM_TYPE(UINT, FONT_SDF_ONEDGE_VAL, 180);
	REGIST_PARAM_TYPE(UINT, FONT_ATLAS_SIZE_W, 4096);
	REGIST_PARAM_TYPE(UINT, FONT_ATLAS_STZE_H, 4096);

	struct QuadVertex
	{
		vec2	pos_;
		vec2	uv_;
	};
	struct DrawFreeTypeInfo
	{
		uint32_t	instance_count_{ 0u };
		Cursor	cursor_;
		bool	new_word_flag_{ true };
		Vector<QuadVertex> quad_vertex_;
		size_type	last_word_vertex_begin_{ 0u };
	};

	static inline bool IsDrawAlgoSupported(EDrawAlgo algo) {
		return GET_PARAM_TYPE_VAL(UINT, FONT_DRAW_ALGO) & Utils::EnumToInteger(algo);
	}

	void EffectDrawText::Init()
	{
		auto* rhi_entity = RHI::RHIGlobalEntity::Instance();
		auto* shader_library = rhi_entity->GetOrCreateShaderLibrary();
		RHI::RHIPipelineStateObjectInitializer pso_initializer;

		if (IsDrawAlgoSupported(EDrawAlgo::eFreeType)) {
			//init freetype shader
		}
		else
		{
			//init glyoutlines shader
		}

		pso_initializer.gfx_.vertex_shader_ = static_cast<RHI::RHIVertexShader::Ptr>(shader_library->GetRHIShader(vertex_shader_.GetShaderHash()));
		pso_initializer.gfx_.pixel_shader_ = static_cast<RHI::RHIPixelShader::Ptr>(shader_library->GetRHIShader(pixel_shader_.GetShaderHash()));
		//initialize pso
		rhi_entity->GetOrCreatePSOLibrary()->GetOrCreatePipeline(pso_initializer);
	}

	void EffectDrawText::Unit()
	{

	}

	//...maybe too slow
	static HashType CalcGlyphHash(uint32_t char_code, uint32_t size, const String& style) {
		HashType hash;
		const auto seed = folly::hash::SpookyHashV1::Hash32(&style, sizeof(style), 0u);
		auto hash32 = folly::hash::SpookyHashV1::Hash32(&char_code, sizeof(char_code), seed);
		hash32 = folly::hash::SpookyHashV1::Hash32(&size, sizeof(size), hash32);
		hash32 = folly::hash::SpookyHashV1::Hash32(style.c_str(), style.size(), hash32);
		hash.FromBinary({ reinterpret_cast<uint8_t*>(&hash32), sizeof(hash32) });
		return hash;
	}

	void EffectDrawText::AddFontStyle(const String& name, const Span<uint8_t>& bin)
	{
		if (font_styleLUT_.find(name) == font_styleLUT_.end()) {
			FontStyleInfo style_info;
			style_info.name_ = name;
			const auto error = stbtt_InitFont(&style_info.stb_info_, bin.data(), stbtt_GetFontOffsetForIndex(bin.data(), 0u));
			PCHECK(error != 0) << fmt::format("Load Font {} failed", name);
			stbtt_GetFontVMetrics(&style_info.stb_info_, &style_info.ascent_, nullptr, nullptr);
			font_styleLUT_.insert({name, style_info});
		}
	}

	void EffectDrawText::DrawExecuteFreeType(const WString& wtext, const TextDrawParams& draw_params) {
		
		const auto whitespace_size = 0.25f * (draw_params.size_ + draw_params.paddingX_);
		const auto tab_size = whitespace_size * 4;
		const auto linebreak_size = float(draw_params.size_ + draw_params.paddingY_);

		const auto word_wrap = [&](DrawFreeTypeInfo& info, float h_wrap) {
			info.new_word_flag_ = true;
			
			//todo wrap last word
			if (info.last_word_vertex_begin_ > 0u && h_wrap >= 0 && info.cursor_.pos_.x >= h_wrap - 1) {
				//push down last word to next line
				const auto word_offset = info.quad_vertex_[info.last_word_vertex_begin_].pos_.x + whitespace_size;
				for (auto n = info.last_word_vertex_begin_; n <= info.quad_vertex_.size(); ++n) {
					info.quad_vertex_[n].pos_ += vec2{-word_offset, linebreak_size};
				}
				info.cursor_.pos_ += vec2{ -word_offset, linebreak_size };
				info.cursor_.size_.x = std::max(info.cursor_.size_.x, info.cursor_.pos_.x);
				info.cursor_.size_.y = std::max(info.cursor_.size_.y, info.cursor_.pos_.y + linebreak_size);
			}
			info.last_word_vertex_begin_ = info.quad_vertex_.size();
		};

		const auto load_char_func = [&](const stbtt_fontinfo* font_info, wchar_t ch) {
			const auto glyph_index = stbtt_FindGlyphIndex(font_info, ch);
			const auto font_scale = stbtt_ScaleForPixelHeight(font_info, draw_params.size_);
			GlyphBitmap glyph_bitmap;
			if (IsDrawAlgoSupported(EDrawAlgo::eSDF)) {
				const int padding = GET_PARAM_TYPE_VAL(UINT, FONT_SDF_PADDING_VAL);
				const uint8_t onedge = GET_PARAM_TYPE_VAL(UINT, FONT_SDF_ONEDGE_VAL);
				const float pixel_dist_scale = float(padding) / onedge;
				auto* raw_data = stbtt_GetGlyphSDF(font_info, font_scale, glyph_index, padding, onedge, pixel_dist_scale, &glyph_bitmap.size_.x, &glyph_bitmap.size_.y, &glyph_bitmap.pos_.x, &glyph_bitmap.pos_.y);
				glyph_bitmap.data_.resize(glyph_bitmap.size_.x * glyph_bitmap.size_.y);
				std::copy(raw_data, raw_data + glyph_bitmap.data_.size(), glyph_bitmap.data_.data());
				stbtt_FreeSDF(raw_data, nullptr);
			}
			else
			{
				auto* raw_data = stbtt_GetGlyphBitmap(font_info, font_scale, font_scale, glyph_index, &glyph_bitmap.size_.x, &glyph_bitmap.size_.y, &glyph_bitmap.pos_.x, &glyph_bitmap.pos_.y);
				glyph_bitmap.data_.resize(glyph_bitmap.size_.x * glyph_bitmap.size_.y);
				std::copy(raw_data, raw_data + glyph_bitmap.data_.size(), glyph_bitmap.data_.data());
				stbtt_FreeBitmap(raw_data, nullptr);
			}
			return glyph_bitmap;
		};

		const auto parse_func = [&](const WString& wtext, const TextDrawParams& draw_params) {
			DrawFreeTypeInfo info;
			info.quad_vertex_.resize(wtext.length() * 4);
			info.cursor_ = draw_params.cursor_;
			
			if (auto iter = font_styleLUT_.find(draw_params.style_name_); iter == font_styleLUT_.end()) {
				AddFontStyle(draw_params.style_name_, );
			}
			auto font_info = font_styleLUT_[draw_params.style_name_].stb_info_;
			const auto font_scale = stbtt_ScaleForPixelHeight(&font_info, draw_params.size_);

			for (auto n = 0; n < wtext.length(); ++n) {
				if (wtext[n] == '\n') {
					word_wrap(info, draw_params.h_wrap_);
					info.cursor_.pos_.x = 0u;
					info.cursor_.pos_.y += linebreak_size;
				}
				else if (wtext[n] == '\t') {
					word_wrap(info, draw_params.h_wrap_);
					info.cursor_.pos_.x += tab_size;
				}
				else if (wtext[n] == ' ') {
					word_wrap(info, draw_params.h_wrap_);
					info.cursor_.pos_.x += whitespace_size;
				}
				else
				{
					const auto char_hash = CalcGlyphHash(wtext[n], draw_params.size_, draw_params.style_name_);
					if (auto iter = glyphs_atlasLUT_.find(char_hash); iter == glyphs_atlasLUT_.end()) {
						const auto glyph = load_char_func(&font_info, wtext[n]);
						glyphs_neededLUT_.insert({ char_hash, glyph });
					}
					const auto quad_left = info.cursor_.pos_.x + xx;
					const auto quad_top = info.cursor_.pos_.y + xx;
					const auto quad_right = quad_left + xx;
					const auto quad_bottom = quad_top + xx;

					info.quad_vertex_.emplace_back(QuadVertex{ {quad_left, quad_top}, {} }); //left-top
					info.quad_vertex_.emplace_back(QuadVertex{ {quad_right, quad_top},{} }); //right-top
					info.quad_vertex_.emplace_back(QuadVertex{ {quad_left, quad_bottom},{} }); //left-bottom
					info.quad_vertex_.emplace_back(QuadVertex{ {quad_right, quad_bottom},{} }); //right-bottom


					int advance, lsb;
					stbtt_GetCodepointHMetrics(&font_info, wtext[n], &advance, &lsb);
					info.cursor_.pos_.x += advance * font_scale + draw_params.paddingX_;

					info.instance_count_ += 1;
					if (n < wtext.length() - 1 && wtext[n + 1]) {
						info.cursor_.pos_.x += font_scale * stbtt_GetCodepointKernAdvance(&font_info, wtext[n], wtext[n + 1]);
					}
					info.new_word_flag_ = false;
				}
				info.cursor_.size_.x = std::max(info.cursor_.size_.x, info.cursor_.pos_.x);
				info.cursor_.size_.y = std::max(info.cursor_.size_.y, info.cursor_.pos_.y + linebreak_size);
				word_wrap(info, draw_params.h_wrap_);
			}
			return info;
		};
		const auto draw_info = parse_func(wtext, draw_params);
		if (!glyphs_neededLUT_.empty()) {
			UpdateAtlas();
			glyphs_neededLUT_.clear();
		}

		//draw quad instance
		auto cmd_ctx = RHI::RHIGlobalEntity::Instance()->CreateCommandBuffer();
		if (1) {
			//RHI:: //push const
			cmd_ctx->Enqueue();
		}
		RHI::RHIDrawPacket cmd{};
		cmd_ctx->Enqueue(&cmd);
		RHI::RHIGlobalEntity::Instance()->Execute({ cmd_ctx });
	}

	void EffectDrawText::DrawExecuteGlyphOutlines(const WString& wtext, const TextDrawParams& draw_params) {
		const auto load_char_func = [](stbtt_fontinfo fontinfo, uint32_t char_code) {

		};
	}

	void EffectDrawText::Draw(const String& text, const TextDrawParams& draw_params)
	{
		const auto& wtext_temp = Utils::StringConvertHelper::StringToWString(text);
		Draw(text, draw_params);
	}

	void EffectDrawText::Draw(const WString& wtext, const TextDrawParams& draw_params)
	{
		PCHECK(!wtext.empty());
		decltype(&DrawExecuteFreeType) draw_executor = IsDrawAlgoSupported(EDrawAlgo::eFreeType) ? DrawExecuteFreeType : DrawExecuteGlyphOutlines;
		draw_executor(wtext, draw_params);
	}

	void EffectDrawText::UpdateAtlas()
	{
		if (glyphs_neededLUT_.empty()) {
			return;
		}
		{
			std::scoped_lock<std::mutex> lock(atlas_mutex_);
			const auto atlas_width = GET_PARAM_TYPE_VAL(UINT, FONT_ATLAS_SIZE_W);
			const auto atlas_height = GET_PARAM_TYPE_VAL(UINT, FONT_ATLAS_SIZE_H);

			stbrp_context pack_ctx;
			Vector<stbrp_rect>	pack_rects; //todo 
			Vector<stbrp_node>	nodes_storage(4096); //todo use allocator
			stbrp_init_target(&pack_ctx, atlas_width, atlas_height, nodes_storage.data(), nodes_storage.size());

			for (const auto& [rid, gly] : glyphs_neededLUT_) {
				pack_rects.emplace_back(stbrp_rect{ rid, gly.size_.x, gly.size_.y });
			}

			if (stbrp_pack_rects(&pack_ctx, pack_rects.data(), pack_rects.size())) {
				//generate atlas
				Vector<uint8_t> atlas_cpu(atlas_width * atlas_height);
				for (const auto iter = pack_rects.begin(); iter != pack_rects.end(); ++iter) {
					const auto rid = iter->id;
					auto& gly = glyphs_atlasLUT_[rid];
					gly.pos_ = ivec2{ iter->x, iter->y };

					//fill atlas map todo
				}

				//copy atlas to GPU
				atlas_ = RHI::RHIGlobalEntity::Instance()->CreateTexture();

			}
			else
			{
				LOG(ERROR) << "rect pack failed, try to enlarge atlas size";
			}
			glyphs_neededLUT_.clear();
		}
	}

	const TextDrawParams& TextDrawParams::GetDefaultTextDrawParams()
	{
		static TextDrawParams default_params;
		//default_params.xx
		return default_params;
	}
}

}