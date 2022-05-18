#pragma once
#include <unordered_set>
#include "Renderer/RtRenderGraph.h"
#include "Renderer/RtRenderResources.h"
#include "RHI/"

namespace MetaInit
{
	namespace Renderer
	{

		//frame graph render pass
		class RtRendererPass
		{
		public:
			using Ptr = std::shared_ptr<RtRendererPass>;
			//like ue save handle 
			using Handle = std::pair<Ptr, uint32_t>;

			enum class EPipeLine : uint32_t
			{
				eNone			= 0x0,
				eGraphics		= 0x1,
				eAsyncCompute   = 0x2,
				eAll			= eGraphics | eAsyncCompute,
			};

			enum class EFlags : uint8_t
			{
				eNone	= 0,
				eGFX	= 1,
				eCompute	= 2,
				eAsync		= 4,
				eNerverCull	= 8,
			};

			//how to bind resource and field FIXME
			class RtPassParameters
			{
			public:
				void AddField(RtField&& field);
				RtField& operator[](const uint32_t index);
				template <typename Function>
				void Enumerate(Function func) {
					for (auto& p : fields_) {
						func(p);
					}
				}
			private:
				SmallVector<RtField>	fields_;
			};
			using Parameters = RtPassParameters;

			explicit RtRendererPass(const std::string& name, const EPipeLine pipeline, uint32_t index);
			virtual ~RtRendererPass();
			virtual void Execute() = 0;
			RtRendererPass& AddField(RtField::Ptr field);
			RtRendererPass& AddParameters(Parameters&& params);
			bool IsOutput()const { return is_output_; }
			bool IsAysnc()const { return is_async_; }
			bool IsCullAble()const { return is_culling_able_; }
			const std::string& GetName()const { return name_; }
			EPipeLine GetPipeLine()const { return pipeline_; }

		private:
			const std::string					name_;
			const EPipeLine						pipeline_;
			EFlags								flags_ = EFlags::eNone;
			//const uint32_t					pass_id_;
			//todo
			std::unordered_set<Handle>			producers_;
			std::unordered_set<Handle>			consumers_;
			Parameters							parameters_;

			//render barrier
			RtBarrierBatch::Ptr					barriers_prologue_;
			RtBarrierBatch::Ptr					barriers_epilogure_;
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
		template <typename LAMBDA>
		class RtLambdaRendererPass : public RtRendererPass
		{
		public:
			RtLambdaRendererPass(Lambda&& lamda);
			void Execute() override;
		private:
			LAMBDA& pass_;
		};

		template<class GELE>
		static inline GELE::Handle MakePair(GELE::Ptr ptr, uint32_t index) {
			return std::make_pair<GELE::Ptr, uint32_t>(ptr, index);
		}
	}
}