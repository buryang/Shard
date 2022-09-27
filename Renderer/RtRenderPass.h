#pragma once
#include <unordered_set>
#include "Utils/Handle.h"
#include "Utils/SimpleEntitySystem.h"
#include "Renderer/RtRenderBarrier.h"
#include "Renderer/RtRenderGraph.h"
#include "Renderer/RtRenderResources.h"

namespace MetaInit
{
	namespace Renderer
	{

		//frame graph render pass
		class RtRendererPass
		{
		public:
			using Ptr = RtRendererPass*;
			enum class EPipeLine : uint32_t
			{
				eNone			= 0x0,
				eGraphics		= 0x1,
				eAsyncCompute   = 0x2,
				eAll			= eGraphics | eAsyncCompute,
				eNum			= 0x4,
			};

			enum class EFlags : uint8_t
			{
				eNone	= 0x0,
				eGFX	= 0x1,
				eCompute	= 0x2,
				eAsync		= 0x4,
				eCopy		= 0x8,
				eNerverCull	= 0x10,
				eNerverMerge = 0x20,
			};

			//how to bind resource and field FIXME
			class RtPassParameters
			{
			public:
				RtPassParameters& AddField(const std::string& param_name, RtField&& field);
				bool IsEmpty()const { return fields_.size()==0; }
				RtField& operator[](const std::string& name);
				template <typename Function>
				void Enumerate(Function func) {
					for (auto& [k, p] : fields_) {
						func(p);
					}
				}
			private:
				std::unordered_map<std::string, RtField>	fields_;
			};
			using Parameters = RtPassParameters;

			explicit RtRendererPass(const std::string& name, EPipeLine pipeline, EFlags flags, uint32_t index);
			virtual ~RtRendererPass();
			virtual void Execute() = 0;
			RtRendererPass& SetParameters(Parameters&& params);
			bool IsOutput()const { return is_output_; }
			bool IsAysnc()const { return is_async_; }
			bool IsCullAble()const { return is_culling_able_; }
			const std::string& GetName()const { return name_; }
			EPipeLine GetPipeLine()const { return pipeline_; }
			static std::array<EPipeLine, 2>& GetSupportPipeLines() {
				static std::array<EPipeLine, 2> pipe_lines{ EPipeLine::eGraphics, EPipeLine::eAsyncCompute };
				return pipe_lines;
			}
			void IncrRef() { ++ref_count_; }
			void DecrRef() { --ref_count_; }
		private:
			const std::string					name_;
			const EPipeLine						pipeline_;
			uint32_t							ref_count_{ 0 };
			EFlags								flags_{ EFlags::eNone };
			
			std::unordered_set<PassHandle>		producers_;
			std::unordered_set<PassHandle>		consumers_;
			Parameters							parameters_;

			//render barrier
			RtBarrierBatch::Ptr					barriers_prologue_{ nullptr };
			RtBarrierBatch::Ptr					barriers_epilogure_{ nullptr };
			union
			{
				struct
				{
					//whether in a async task queue
					uint8_t								is_async_ : 1;
					//generate/wait sync fence for follow pass
					//whether gfx fork/join	
					uint8_t								is_gfx_fork_ : 1;
					uint8_t								is_gfx_join_ : 1;
					//whether async compute fork/join
					uint8_t								is_compute_begin_ : 1;
					uint8_t								is_compute_end_ : 1;
					//culling able
					uint8_t								is_culling_able_ : 1;
					uint8_t								is_output_ : 1;
				};
				uint8_t	pack_bits_{ 0 };
			};
			
		};

		//lambda render pass
		template <typename PARAM, typename LAMBDA>
		class RtLambdaRendererPass : public RtRendererPass
		{
		public:
			using Parameters = PARAM;
			RtLambdaRendererPass(const std::string& name, EFlags flags, 
									const PARAM* params, LAMBDA&& lamda) : RtRendererPass(), params_(params),pass_(lambda_) {}
			void Execute() override;
			const Parameters* GetParameters() const { return params_; }
			virtual ~RtLambdaRendererPass() {}
		private:
			LAMBDA& pass_;
			const Paramerters* params_;
		};

		template <typename LAMBDA>
		using RtDummyRendererPass = RtLambdaRendererPass<void, [] {} > ;
		using PassHandle = Utils::Handle<RtRendererPass>;
	}
}