#pragma once
#include "Utils/CommonUtils.h"
//#include "Utils/Timer.h"
#include "HAL/HALCommand.h"
#include "HAL/HALQuery.h"
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
                String      descrption_;
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
                        .depth_ = 0u,
                        .descrption_ = fmt::format(format, args);
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
                range.clock_ = (std::chrono::high_resolution_clock::now() - range.clock_).count();
                LOG(INFO) << fmt::format("{}[{}] execute time: \t {}", range.descrption_, range.tag_, range.clock_);
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

        struct GPUEventInfo
        {
            String  info_;
        };

        class RenderGPUEventPool
        {
        public:
            explicit RenderGPUEventPool(uint32_t count, EType type) {
                HAL::HALGlobalEntity::CreateQueryPool();
                time_stamps_info_.resize(count);
            }
            ~RenderGPUEventPool() {
                if (nullptr != time_stamps_pool_) {
                    delete time_stamps_pool_; //todo
                }
            }
            template<typename ...Args>
            uint32_t BeginEvent(HAL::HALCommandContext* cmd_context, const String& format, Args&&... args) {
                SpinLock lock(lock_);
                const auto event_id = curr_event_id_++;
                assert(event_id < time_stamps_info_.size());
                event_stack_.push(event_id);
                //for timestamp query does not has begin query 
                HALEndQueryPacket begin_query{ .query_pool_ = time_stamps_pool_, .query_index_ = 2*event_id, };
                cmd_context->Enqueue(&begin_query);
                time_stamps_info_[event_id] = { fmt::format(format, args) };
                return event_id;
            }
            void EndEvent(HAL::HALCommandContext* cmd_context, uint32_t event_id) {
                SpinLock lock(lock_);
                assert(event_id == event_stack_.top());
                event_stack_.pop();
                HALEndQueryPacket end_query{ .query_pool_ = time_stamps_pool_, .query_index_ = 2*event_id+1 };
                cmd_context->Enqueue(&end_query);
            }
            void Reset(HAL::HALCommandContext* cmd_context) {
                SpinLock lock(lock_);
                HALResetQueryPoolPacket reset_query{};
                cmd_context->Enqueue(&reset_query);
                //curr_event_id_ ??
            }
            void Reset() {
                SpinLock lock(lock_);
                curr_event_id_ = 0u;
                time_stamps_info_.clear();
                event_stack_.swap(Stack<uint32_t>{});
                time_stamps_pool_->Reset();
            }
            void WaitReadBack() {
                SpinLock lock(lock_);
                if (!time_stamps_pool_->IsQueryReady()) {
                    time_stamps_pool_->WaitReadBack();
                }
            }
            bool QueryEventResult(uint32_t event_id, GPUEventInfo& info, uint64_t& duration) {
                //assume all work done, no need to sync
                if (event_id > curr_event_id_) {
                    return false;
                }

                info = time_stamps_info_[event_id];
                duration = *(uint64_t*)(time_stamps_pool_->GetQueryResult(2 * event_id + 1) -
                             *(uint64_t*)(time_stamps_pool_->GetQueryResult(2 * event_id));
                return true;
            }
        private:
            EType   type_{ EType::eTimeStamp };
            Stack<uint32_t> event_stack_;
            HAL::HALQueryPool*  time_stamps_pool_{ nullptr };
            Vector<GPUEventInfo>    time_stamps_info_;
            std::atomic_bool    lock_;
            uint32_t    curr_event_id_{ 0u };
        };

        class RenderGPUEvent
        {
        public:
            template<typename ...Args>
            RenderGPUEvent(RenderGPUEventPool* pool, HAL::HALCommandContext* cmd_context, const String& format, Args&&... args) :pool_(pool), cmd_context_{ cmd_context } {
                event_id_ = pool_->BeginEvent(cmd_context, format, args);
            }
            ~RenderGPUEvent() {
                pool_->EndEvent(cmd_context, event_id);
            }
        private:
            uint32_t    event_id_{ 0u };
            RenderGPUEventPool* pool_{ nullptr };
            HAL::HALCommandContext* cmd_context_{ nullptr };
        };
            
    }
}

#define RENDER_EVENT(fmt, args...) Shard::Render::RenderEvent event(fmt, args);
#define RENDER_EVENT_BEGIN(fmt, args...) do{ std::RenderEventRegistry::TlSIntance().Push(fmt, args); } while(0);
#define RENDER_EVENT_END() do{ std::RenderEventRegistry::TLSInstance().Pop();} while(0);
#define RENDER_GPU_EVENT(pool, cmd_context, fmt, args...) do{ Shard::Render::RenderGPUEvent(pool, cmd_context, fmt, args);} while(0);
#define RENDER_GPU_EVENT_BEGIN(pool, cmd_context, fmt, args...) do{ pool->BeginEvent(cmd_context, fmt, args);} while(0);
#define RENDER_GPU_EVENT_END(pool, cmd_context) do{ pool->EndEvent(cmd_context);} while(0);
#else
#define RENDER_EVENT(...)
#define RENDER_EVENT_BEGIN(...)
#define RENDER_EVENT_END(...)
#define RENDER_GPU_EVENT_BEGIN(...)
#define RENDER_GPU_EVENT_END(...)
#endif

