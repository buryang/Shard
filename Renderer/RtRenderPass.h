#pragma once
#include <unordered_set>
#include "Renderer/RtRenderGraph.h"
#include "Renderer/RtRenderResources.h"

namespace MetaInit
{
	namespace Renderer
	{
		class RtField
		{
		public:
			enum class EFormat
			{

			};

			enum class EState
			{

			};
				
			using Ptr = std::shared_ptr<RtField>;
			using Handle = std::pair<Ptr, uint32_t>;
			uint32_t GetFLags()const;
		private:
			friend class RtRendererPass;
			std::string					name_;
			SmallVector<Handle>			producers_;
			SmallVector<Handle>			consumers_;
			RtRendererPass::Handle		pass_;
			struct SubFiledRange
			{
			};
			SmallVector<SubFiledRange>	sub_resources_;
		};


		//frame graph render pass
		class RtRendererPass
		{
		public:
			using Ptr = std::shared_ptr<RtRendererPass>;
			//like ue save handle 
			using Handle = std::pair<Ptr, uint32_t>;
			RtRendererPass(const std::string& name, uint32_t index);
			virtual ~RtRendererPass();
			virtual void Execute() = 0;
			virtual void Compile() = 0;
			void AddField(RtField::Ptr field);
			bool IsOutput()const { return is_output_; }
			bool IsAysnc()const { return is_async_; }
			const std::string& GetName()const;
		private:
			const std::string					name_;
			const uint32_t						pass_id_;
			//todo
			std::unordered_set<Handle>			producers_;
			std::unordered_set<Handle>			consumers_;
			SmallVector<RtField::Handle>		inputs_;
			SmallVector<RtField::Handle>		outputs_;
			SmallVector<RtField::Handle>		internals_;
			//whether in a async task queue
			uint8_t								is_async_:1;
			//generate/wait sync fence for follow pass
			//whether gfx fork/join	
			uint8_t								is_gfx_fork_:1;
			uint8_t								is_gfx_join_:1;
			//whether async compute fork/join
			uint8_t								is_compute_begin_:1;
			uint8_t								is_compute_end_:1;
			//culling able
			uint8_t								is_culling_able_:1;
			uint8_t								is_output_:1;
			
		};

		template<class GELE>
		static inline GELE::Handle MakePair(GELE::Ptr ptr, uint32_t index) {
			return std::make_pair<GELE::Ptr, uint32_t>(ptr, index);
		}
	}
}