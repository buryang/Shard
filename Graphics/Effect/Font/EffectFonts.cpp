#include "Core/EngineGlobalParams.h"
#include "Graphics/RHI/RHIGlobalEntity.h"
#include "Graphics/Renderer/RtRenderGraph.h"
#include "Graphics/Effect/Font/EffectFonts.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb/stb_rect_pack.h"
#include <filesystem>

namespace fs =  std::filesystem;

namespace Shard::Effect
{
     REGIST_PARAM_TYPE(STRING, FONT_ROOT_PATH, "");
     REGIST_PARAM_TYPE(UINT, FONT_DRAW_ALGO, EDrawAlgo::eFreeType);
     REGIST_PARAM_TYPE(UINT, FONT_SDF_GLYPH_SIZE, 128); //if use sdf, all glyph share same size
     REGIST_PARAM_TYPE(UINT, FONT_SDF_PADDING_VAL, 5);
     REGIST_PARAM_TYPE(UINT, FONT_SDF_ONEDGE_VAL, 180);
     REGIST_PARAM_TYPE(UINT, FONT_ATLAS_SIZE_W, 4096);
     REGIST_PARAM_TYPE(UINT, FONT_ATLAS_STZE_H, 4096);

     static constexpr auto DRAW_FONT_PASS = "DRAW_FONT_PASS";

     struct QuadVertex
     {
          vec2     pos_;
          vec2     uv_;
     };

     struct DrawFreeTypeInfo
     {
          uint32_t     instance_count_{ 0u };
          Cursor     cursor_;
          bool     new_word_flag_{ true };
          Vector<QuadVertex> quad_vertex_;
          size_type     last_word_vertex_begin_{ 0u };
     };

     //shader const buffer
     struct FontConstBuffer
     {
          vec2     sdf_minmax_;
          uint32_t     color_;
          int     buffer_index_;
          int     buffer_offset_;
          int     texture_index_;
          mat4     transform_;
     };

     static inline bool IsDrawAlgoSupported(EDrawAlgo algo) {
          return GET_PARAM_TYPE_VAL(UINT, FONT_DRAW_ALGO) & Utils::EnumToInteger(algo);
     }

     bool RtFreeTypeFontVS::IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation)
     {
          return true;
     }

     RtRenderShaderParametersMeta* RtFreeTypeFontVS::GetShaderParametersMeta()
     {
          return nullptr;
     }

     bool RtFreeTypeFontPS::IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation)
     {
          return true;
     }

     RtRenderShaderParametersMeta* RtFreeTypeFontPS::GetShaderParametersMeta()
     {
          return nullptr;
     }

     bool RtGlyphOutlinesFontVS::IsCompileNeedFor(const ShaderPlatform& platform, const uint32_t permutation)
     {
          return false;
     }


     void EffectDrawText::Init()
     {
          auto* rhi_entity = RHI::RHIGlobalEntity::Instance();
          auto* shader_library = rhi_entity->GetOrCreateShaderLibrary();
          RHI::RHIPipelineStateObjectInitializer pso_initializer;

          if (IsDrawAlgoSupported(EDrawAlgo::eFreeType)) {
               //init freetype shader
               pso_initializer.gfx_.vertex_shader_ = static_cast<RHI::RHIVertexShader::Ptr>(shader_library->GetRHIShader(vertex_shader.GetShaderHash()));
               pso_initializer.gfx_.pixel_shader_ = static_cast<RHI::RHIPixelShader::Ptr>(shader_library->GetRHIShader(pixel_shader.GetShaderHash()));
          }
          else
          {
               //init glyoutlines shader
               pso_initializer.gfx_.vertex_shader_ = static_cast<RHI::RHIVertexShader::Ptr>(shader_library->GetRHIShader(vertex_shader.GetShaderHash()));
               pso_initializer.gfx_.pixel_shader_ = static_cast<RHI::RHIPixelShader::Ptr>(shader_library->GetRHIShader(pixel_shader.GetShaderHash()));
          }
          pso_initializer.gfx_.blend_state_.attachments_[0].color_blend_op_ = RHI::RHIBlendStateInitializer::BlendAttachment::EBlendOperation::eAdd;
          pso_initializer.gfx_.blend_state_.attachments_[0].alpha_blend_op_ = RHI::RHIBlendStateInitializer::BlendAttachment::EBlendOperation::eAdd;
          pso_initializer.gfx_.blend_state_.attachments_[0].color_src_factor_ = RHI::RHIBlendStateInitializer::BlendAttachment::EBlendFactor::eSrcAlpha;
          pso_initializer.gfx_.blend_state_.attachments_[0].color_dst_factor_ = RHI::RHIBlendStateInitializer::BlendAttachment::EBlendFactor::eInvSrcAlpha;
          pso_initializer.gfx_.blend_state_.attachments_[0].alpha_src_factor_ = RHI::RHIBlendStateInitializer::BlendAttachment::EBlendFactor::eZero;
          pso_initializer.gfx_.blend_state_.attachments_[0].alpha_dst_factor_ = RHI::RHIBlendStateInitializer::BlendAttachment::EBlendFactor::eOne;
          pso_initializer.gfx_.blend_state_.attachments_[0].color_write_mask_ = RHI::RHIBlendStateInitializer::BlendAttachment::EBlendColorMask::eRGBA;
          pso_initializer.gfx_.rasterization_state_.front_face_ = RHI::RHIRasterizationStateInitializer::EFrontFace::eCClk;
          pso_initializer.gfx_.primitive_topology_ = EInputTopoType::eTriangleStrip;
          //initialize pso
          pso_ = rhi_entity->GetOrCreatePSOLibrary()->GetOrCreatePipeline(pso_initializer);
          PCHECK(pso_ != nullptr) << "create text draw pso failed";
     }

     void EffectDrawText::Unit()
     {

     }

     static HashType FORCE_INLINE CalcGlyphHash(uint32_t char_code, uint32_t size, uint32_t style) {
          HashType hash{ .code_ = char_code, .sdf_ = IsDrawAlgoSupported(EDrawAlgo::eSDF), .font_style_ = style, .size_ = size };
          return hash;
     }

     uint32_t EffectDrawText::AddFontStyle(const String& name, const Span<uint8_t>& bin)
     {
          if (std::find_if(font_styleLUT_.cbegin(), font_styleLUT_.cend(), [name](const auto& style) { return style.name_ == name; }) == font_styleLUT_.end()) {
               FontStyleInfo style_info;
               style_info.name_ = name;
               const auto error = stbtt_InitFont(&style_info.stb_info_, bin.data(), stbtt_GetFontOffsetForIndex(bin.data(), 0u));
               PCHECK(error != 0) << fmt::format("Load Font {} failed", name);
               stbtt_GetFontVMetrics(&style_info.stb_info_, &style_info.ascent_, &style_info.descent_, &style_info.linegap_); //why not use linegap ?
               font_styleLUT_.emplace_back(style_info);
               return font_styleLUT_.size() - 1;
          }

          return -1;
     }

     void EffectDrawText::DrawExecuteFreeType(Renderer::RtRendererGraphBuilder& builder, const WString& wtext, const TextDrawParams& draw_params) {
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
                         info.quad_vertex_[n].pos_ += vec2{ -word_offset, linebreak_size };
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

               const FontStyleInfo* font_info{ nullptr };
               uint32_t font_info_index{ 0u };
               if (auto iter = eastl::find_if(font_styleLUT_.begin(), font_styleLUT_.end(), [](const auto& sytle) { return style.name_ == draw_params.style_name_; }); iter == font_styleLUT_.end()) {
                    AddFontStyle(draw_params.style_name_, );
                    font_info = &font_styleLUT_.back();
                    font_info_index = font_styleLUT_.size();
               }
               else
               {
                    font_info = iter;
                    font_info_index = uint32_t(iter - font_styleLUT_.begin());
               }
               assert(draw_params.size_ > font_info->ascent_ + font_info->descent_ + font_info->linegap_ && draw_params.pos_.y > font_info->ascent_ );
               const auto font_scale = stbtt_ScaleForPixelHeight(&font_info->stb_info_, draw_params.size_);

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
                         const auto char_hash = CalcGlyphHash(wtext[n], draw_params.size_, font_info_index);
                         if (auto iter = glyphs_atlasLUT_.find(char_hash); iter == glyphs_atlasLUT_.end()) {
                              const auto glyph = load_char_func(&font_info->stb_info_, wtext[n]);
                              glyphs_neededLUT_.insert({ char_hash, glyph });
                              continue; //not render this glyph
                         }

                         int advance, lsb;
                         stbtt_GetCodepointHMetrics(&font_info->stb_info_, wtext[n], &advance, &lsb); //why v metric not related to glyph

                         const auto quad_left = info.cursor_.pos_.x + lsb * font_scale;
                         const auto quad_top = info.cursor_.pos_.y - font_info->ascent_ * font_scale; //bearingY 
                         const auto quad_right = quad_left + draw_params.size_;
                         const auto quad_bottom = quad_top + draw_params.size_;

                         const auto& glyph_atlas = glyphs_atlasLUT_[char_hash];
                         info.quad_vertex_.emplace_back(QuadVertex{ {quad_left, quad_top}, {glyph_atlas.pos_} }); //left-top
                         info.quad_vertex_.emplace_back(QuadVertex{ {quad_right, quad_top},{glyph_atlas.pos_ + ivec2{glyph_atlas.size_.x, 0u } } }); //right-top
                         info.quad_vertex_.emplace_back(QuadVertex{ {quad_left, quad_bottom},{glyph_atlas.pos_ + ivec2{0u, glyph_atlas.size_.y }} }); //left-bottom
                         info.quad_vertex_.emplace_back(QuadVertex{ {quad_right, quad_bottom},{glyph_atlas.pos_ + glyph_atlas.size_ } }); //right-bottom
                         info.cursor_.pos_.x += advance * font_scale + draw_params.paddingX_;

                         info.instance_count_ += 1;
                         if (n < wtext.length() - 1 && wtext[n + 1]) {
                              info.cursor_.pos_.x += font_scale * stbtt_GetCodepointKernAdvance(&font_info->stb_info_, wtext[n], wtext[n + 1]);
                         }
                         if (info.new_word_flag_) {
                              info.last_word_vertex_begin_ = n * 4;
                              info.new_word_flag_ = false;
                         }
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

          //todo add pass
          auto graph = builder.GetRenderGraph();
          graph.AddPass([&](void) {
               //draw quad instance
               auto cmd_ctx = RHI::RHIGlobalEntity::Instance()->CreateCommandBuffer();
               RHI::RHIBindPSOPacket pso_cmd{ pso_, RHI::RHIBindPSOPacket::EBindPoint::eGFX };
               cmd_ctx->Enqueue(&pso_cmd);
               RHI::RHIBeginRenderPassPacket begin_pass_cmd;
               cmd_ctx->Enqueue(&begin_pass_cmd);
               if (IsDrawAlgoSupported(EDrawAlgo::eFreeType)) {
                    //bind free type draw resource
                  //set texture and buffers / bindless resource RHI::RHI 
                    const auto vertex_handle = RHI::RHIGlobalEntity::Instance()->GetResourceBindlessHeap()->WriteBuffer(vertex_buffer_);
                    const auto atlas_handle = RHI::RHIGlobalEntity::Instance()->GetResourceBindlessHeap()->WriteTexture(atlas_);
                    FontConstBuffer font_cbbuffer{ .buffer_index_ = vertex_handle.index_, .texture_index_ = atlas_handle.index_, };//todo
                    RHI::RHIPushConstantPacket push_cmd{ 0u, 0u, reinterpret_cast<uint8_t*>(&font_cbbuffer), sizeof(font_cbbuffer) };
                    cmd_ctx->Enqueue(&push_cmd);
               }
               else
               {
                    //cmd_ctx->Enqueue();
               }
               RHI::RHIDrawPacket draw_cmd{ draw_info.instance_count_ * 4, draw_info.instance_count_, 0, 0 };
               cmd_ctx->Enqueue(&draw_cmd);
               RHI::RHIEndRenderPassPacket end_pass_cmd;
               cmd_ctx->Enqueue(&end_pass_cmd);
               RHI::RHIGlobalEntity::Instance()->Execute({ cmd_ctx });
          }, EPipeLine::eGraphics, RtRendererPass::EFlags::eGFX, DRAW_FONT_PASS);
     }

     void EffectDrawText::DrawExecuteGlyphOutlines(Renderer::RtRendererGraph& graph, const WString& wtext, const TextDrawParams& draw_params) {
          const auto load_char_func = [&](const stbtt_fontinfo* fontinfo, wchar_t char_code) {
               const char* svg_data{ nullptr };
               const auto size = stbtt_GetGlyphSVG(fontinfo, char_code, &svg_data);

               if (size != 0u) {
                    //todo deal with svg info
               }
          };

          const auto word_wrap = [&]() {
          };

          const auto parse_func = [&](const WString& wtext, const TextDrawParams& draw_params) {
          };

          graph.AddPass([&](void) {}, EPipeLine::eGraphics, RtRendererPass::EFlags::eGFX, DRAW_FONT_PASS);
     }

     void EffectDrawText::Draw(Renderer::RtRenderGraphBuilder& builder, const String& text, const TextDrawParams& draw_params)
     {
          const auto& wtext_temp = Utils::StringConvertHelper::StringToWString(text);
          Draw(builder, text, draw_params);
     }

     void EffectDrawText::Draw(Renderer::RtRenderGraphBuilder& builder, const WString& wtext, const TextDrawParams& draw_params)
     {
          PCHECK(!wtext.empty());
          decltype(&DrawExecuteFreeType) draw_executor = IsDrawAlgoSupported(EDrawAlgo::eFreeType) ? DrawExecuteFreeType : DrawExecuteGlyphOutlines;
          draw_executor(builder, wtext, draw_params);
     }

     void EffectDrawText::UpdateAtlas(float scale)
     {
          if (glyphs_neededLUT_.empty()) {
               return;
          }
          {
               std::scoped_lock<std::mutex> lock(atlas_mutex_);
               const auto atlas_width = GET_PARAM_TYPE_VAL(UINT, FONT_ATLAS_SIZE_W);
               const auto atlas_height = GET_PARAM_TYPE_VAL(UINT, FONT_ATLAS_SIZE_H);

               stbrp_context pack_ctx;
               Vector<stbrp_rect> pack_rects;
               Vector<stbrp_node> node_storage{ atlas_width * 2 };//make sure 'num_nodes' >= 'width'
               stbrp_init_target(&pack_ctx, atlas_width, atlas_height, node_storage.data(), node_storage.size());
               stbrp_setup_allow_out_of_mem(&pack_ctx, 1);
               stbrp_setup_heuristic(&pack_ctx, STBRP_HEURISTIC_Skyline_BL_sortHeight);

               for (const auto& [rid, gly] : glyphs_neededLUT_) {
                    pack_rects.emplace_back(stbrp_rect{ uint32_t(rid), gly.size_.x, gly.size_.y });
               }
               const auto all_packed = stbrp_pack_rects(&pack_ctx, pack_rects.data(), pack_rects.size());
               if (!all_packed)
               {
                    LOG(ERROR) << "rect pack failed, try to enlarge atlas size";
               }
               else
               {
                    //generate atlas
                    Vector<uint8_t> atlas_cpu(atlas_width * atlas_height);
                    for (auto iter = pack_rects.begin(); iter != pack_rects.end(); ++iter) {
                         const auto rid = iter->id;
                         auto& gly_atlas = glyphs_atlasLUT_[rid];
                         const auto& gly_bitmap = glyphs_neededLUT_[rid];
                         gly_atlas.pos_ = ivec2{ iter->x, iter->y };
                         
                         if (GET_PARAM_TYPE_VAL(UINT, FONT_DRAW_ALGO) & Utils::EnumToInteger(EDrawAlgo::eSDF))
                         {
                              //https://steamcdn-a.akamaihd.net/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf
                              const auto max_radius = std::max(gly_bitmap.size_.x, gly_bitmap.size_.y);
                              for (auto row = 0; row < atlas_height; ++row)
                              {
                                   for(auto col = 0; col < atlas_width; ++col)
                                   {
                                        const auto xpos = 0u, ypos = 0u;
                                        const auto texel = *(gly_bitmap.data_.data() + xpos + ypos*gly_bitmap.size_.x);
                                        uint8_t min_dist = std::numeric_limits<uint8_t>::max();
                                        //begin search nearest opposite val
                                        bool found = false;
                                        int32_t xoffset = 0, yoffset = 0;
                                        for (auto r = 1; r < max_radius; ++r)
                                        {
                                             const auto check_func = [&]() {
                                                  if (xpos + xoffset < 0 || ypos + yoffset < 0 ||
                                                       xpos + xoffset >= gly_bitmap.size_.x || ypos + yoffset >= gly_bitmap.size_.y) //check whether x/y outof image 
                                                  {
                                                       return;
                                                  }
                                                  const auto curr_dist = (uint32_t)sqrtf((xpos + xoffset) * (xpos + xoffset) + (ypos + yoffset) * (ypos + yoffset));
                                                  if (curr_dist > std::numeric_limits<uint8_t>::max()) {
                                                       return;
                                                  }
                                                  const auto curr_texel = *(gly_bitmap.data_.data() + xpos + xoffset + (ypos + yoffset) * gly_bitmap.size_.x);
                                                  if (texel != curr_texel && curr_dist < min_dist) //opposite value found
                                                  {
                                                       min_dist = curr_dist;
                                                       found = true;
                                                  }

                                             };
                                             xoffset = -r;
                                             for (yoffset = -r; yoffset < r; ++yoffset) check_func();
                                             xoffset = r;
                                             for (yoffset = -r; yoffset < r; ++yoffset) check_func();
                                             yoffset = -r;
                                             for (xoffset = -r+1; xoffset < r-1; ++xoffset) check_func();
                                             yoffset = r;
                                             for (xoffset = -r+1; xoffset < r-1; ++xoffset) check_func();
                                             if (found) {
                                                  break;
                                             }
                                        }
                                        atlas_cpu[row * atlas_width + col] = (texel > 0 ? 1 : -1) * min_dist;
                                   }
                              }
                         }
                         else
                         {
                              //copy data to atlas
                              for (auto row = 0; row < gly_bitmap.size_.y; ++row) {
                                   std::copy(gly_bitmap.data_.data() + row * gly_bitmap.size_.x,
                                        gly_bitmap.data_.data() + (row + 1) * gly_bitmap.size_.x,
                                        atlas_cpu.data() + (gly_atlas.pos_.y + row) * atlas_width + gly_atlas.pos_.x);
                              }
                         }
                    }
                    RHI::RHITextureInitializer atlas_desc;
                    atlas_desc.layout_.width_ = atlas_width;
                    atlas_desc.layout_.height_ = atlas_height;
                    atlas_desc.format_ = EPixFormat::eR8Unorm;
                    atlas_desc.is_dedicated_ = false;//
                    atlas_desc.is_transiant_ = true;
                    atlas_desc.access_ = Renderer::EAccessFlags::eReadOnly;

                    atlas_ = RHI::RHIGlobalEntity::Instance()->CreateTexture(atlas_desc);
                    PCHECK(atlas_ != nullptr) << "update glyph atlas create texture failed";
                    //copy atlas from CPU to GPU error texture cannot alloc on host
                    auto* atlas_gpu = atlas_->MapBackMem();
                    std::copy(atlas_cpu.cbegin(), atlas_cpu.cend(), atlas_gpu);
                    atlas_->UnMapBackMem();
               }
               glyphs_neededLUT_.clear();
          }
     }

     const TextDrawParams& TextDrawParams::GetDefaultTextDrawParams()
     {
          static TextDrawParams default_params;
          default_params.style_name_ = "";
          return default_params;
     }
}

}