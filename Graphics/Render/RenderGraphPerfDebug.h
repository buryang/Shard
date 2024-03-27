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

        namespace
        {
            struct TimeRange
            {
                uint32_t    tag_ : 16;
                uint32_t    generation_ : 16;
                uint64_t    clock_;
                uint32_t    index_;
                uint32_t    depth_;
            };
        };
        //https://debaetsd.github.io/posts/profiler/
        class RenderEventRegistry
        {
        public:
            static RenderEventRegistry& TLSInstance() {
                static thread_local RenderEventRegistry registry;
                return registry;
            }
            template<typename ...Args >
            void Push(const std::string& format, Args&&... args) {
                TimeRange new_range{
                        .tag_ = GenerateNewTag(),
                        .clock_ = std::chrono::high_resolution_clock::now(),
                        .depth_ = 0u
                };
                if (!ranges_.empty())
                {
                    auto& prev_range = ranges_.top();
                    new_range.depth_ = prev_range.depth_ + 1u;
                }
                ranges_.push(new_range);
            }
            TimeRange Pop() {
                assert(!ranges_.empty());
                auto range = ranges_.top();
                range.clock_ = std::chrono::high_resolution_clock::now() - range.clock_;
                LOG(INFO) << fmt::format("{} execute time: \t {}", range.tag_, range.clock_);
                ranges_.pop();
                return range;
            }
        private:
            DISALLOW_COPY_AND_ASSIGN(RenderEventRegistry);
            uint32_t GenerateNewTag() {
                return tag_++;
            }
        private:
            Stack<TimeRange>    ranges_;
            uint32_t    tag_{ 0u };
        };

        class RenderEvent
        {
        public:
            template<typename ...Args >
            RenderEvent(const std::string& format, Args&&... args) {
                RenderEventRegistry::TLSInstance().Push(format, args);
            }
            ~RenderEvent() {
                RenderEventRegistry::TLSInstance().Pop(); //todo
            }
        };
            
    }
}

#define RENDER_EVENT(fmt, ...) Shard::Render::RenderEvent event(fmt, __VAR_ARGS__);
#define RENDER_EVENT_BEGIN(fmt, ...) std::RenderEventRegistry::TlSIntance().Push(fmt, __VAR_ARGS__);
#define RENDER_EVENT_END() std::RenderEventRegistry::TLSInstance().Pop();
#define RENDER_GPU_EVENT_BEGIN(fmt, ...)
#define RENDER_GPU_EVENT_END()
#else
#define RENDER_EVENT(fmt, ...)
#define RENDER_EVENT_BEGIN(fmt, ...)
#define RENDER_EVENT_END()
#define RENDER_GPU_EVENT_BEGIN(fmt, ...)
#define RENDER_GPU_EVENT_END()
#endif

