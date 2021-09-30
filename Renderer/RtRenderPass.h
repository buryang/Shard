#pragma once
#include "Renderer/RtRenderGraph.h"

namespace MetaInit
{
	namespace Renderer
	{

		class RtPassPin
		{
		public:
			enum class EType
			{
				eInput,
				eOutput
			};
			uint32_t GetFLags()const;
			void Conntect();
		private:
			friend class RtGraphEdge;
			std::string		name_;
		};

		class RtGraphEdge
		{
		public:
			RtGraphEdge(RtPassPin& input, RtPassPin& output) :input_(input), output_(output) {}
			~RtGraphEdge();
		private:
			RtPassPin&		input_;
			RtPassPin&		output_;
			std::string		name_;
		};

		//frame graph render pass
		class RtRendererPassNode
		{
		public:
			RtRendererPass(const std::string& name, uint32_t index);
			void AddPin();
			void Connect();
			const std::string& GetName()const;
		private:
			RtRendererFrameContextGraph::Ptr	graph_;
			const std::string					name_;
			const uint32_t						pass_id_;
			std::unordered_set<uint32_t>		producers_;
			SmallVector<RtPassPin>				pins_;
		};
	}
}