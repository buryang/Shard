#pragma once
#include "Utils/CommonUtils.h"
#include <iostream>

#ifdef DEVELOP_DEBUG_TOOLS
namespace Shard
{
    namespace Render
    {
        class RenderGraph;
        class RenderGraphExecutor;

        class MINIT_API RenderVisualization
        {
        public:
            static void PrintRenderGraph(const RenderGraph& graph);
            static void PrintRenderGraphExe(const RenderGraphExecutor& executor);
        };

        class MINIT_API RenderEvent
        {
        public:
            template<typename ...Args >
            RenderEvent(const std::string& format, Args&&... args) {
                event_info_ = std::vformat(format, std::make_format_args(args...));
                start_time_ = std::chrono::high_resolution_clock::now();
            }
            ~RenderEvent() {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto time_gap = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
                LOG(INFO) << std::format("{} execute time: \t {}", event_info, time_gap);
            }
        private:
            std::string event_info_;
            std::chrono::time_point<std::chrono::steady_clock>    start_time_;
        };
            
    }
}

#define RENDER_EVENT(fmt, ...) Shard::Render::RenderEvent event(fnt, __VAR_ARGS__);
#else
#define RENDER_EVENT(fmt, ...)
#endif

