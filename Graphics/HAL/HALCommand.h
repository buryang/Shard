#pragma once
#include "Utils/CommonUtils.h"
#include "Render/RenderBarrier.h"
#include "HAL/HALGlobalEntity.h"
#include "HAL/HALCommandIR.h"

namespace Shard
{
    namespace HAL
    {
        //http://danglingpointers.com/post/copy-queues/
        class MINIT_API HALCommandContext
        {
        public:
            HALCommandContext(EContextQueueType queue) : queue_(queue) {}
            void Enqueue(Span<const HALCommandPacket*> cmds);
            //enqueue command sequeues
            virtual void Enqueue(const HALCommandPacket* cmd) = 0;
            //submit command
            virtual void Submit(HALGlobalEntity* rhi) = 0;
            virtual ~HALCommandContext() {}
            EContextQueueType GetQueue() const {
                return queue_;
            }
            void SetQueue(EContextQueueType queue){
                queue_ = queue;
            }
        private:
            DISALLOW_COPY_AND_ASSIGN(HALCommandContext);
        private:
            EContextQueueType   queue_{ EContextQueueType::eUnkown };
            uint32_t    queue_index_{ 0u }; //the queue the command should submit to
            uint32_t    device_mask_{ ~0u };
        };

        /*a virtual command context, maybe usage: record command ir represent for reuse/debug*/
        class MINIT_API HALVirtualCommandContext: public HALCommandContext
        {
        public:
            void Enqueue(const HALCommandPacket* cmd) override;
            //submit command
            void Submit(HALGlobalEntity* rhi) override;
            /*re broadcast to other real command context*/
            void ReplayByOther(HALCommandContext* other_context);
            virtual ~HALVirtualCommandContext() = default;
        protected:
            Vector<const HALCommandPacket*> record_cmds_;
            mutable uint32_t    record_count_;
        };

        class MINIT_API HALCommandQueue
        {
        public:
            HALCommandQueue() = default;
            explicit HALCommandQueue(uint32_t queue_index, float priority):queue_index_(queue_index), priority_(std::clamp(priority, 0.f, 1.f)){}
            FORCE_INLINE uint32_t GetQueueIndex() const { return queue_index_; }
            virtual void Submit(const HALCommandContext* command) = 0;
            virtual void Submit(Span<const HALCommandContext*> commands) = 0;
            virtual uint64_t GetTimeStampFrequency() = 0;
            virtual void WaitFence(uint64_t fence_value) = 0;
            virtual void SignalFence(uint64_t fence_value) = 0;
            virtual ~HALCommandQueue() = default;
        private:
            uint32_t    queue_index_{ 0u };
            float   priority_{ 0.f };
        };
    }
}
