#pragma once
#include "Graphics/HAL/HALCommonUtils.h"
#include "Graphics/HAL/HALCommand.h"

namespace Shard::HAL {

    /* only vulkan split barrier use event now,
    * so I decide not to expose this class
    class HALEvent
    {
    public:
        HALEvent(const String& name) :name_(name) {}
        const String& GetName()const { return name_; }
        virtual ~HALEvent() = 0;
    protected:
        const String    name_;
    };
    */

    class HALFence
    {
    public:
        /*fence created as unsignaled defaultly*/
        HALFence(const String& name) :name_(name) {}
        const String& GetName()const { return name_; }
        virtual void Reset() = 0;
        virtual void Wait() = 0;
        virtual ~HALFence() = default;
    protected:
        const String    name_;
    };

    class HALSemaphore
    {
    public:
        HALSemaphore(const String& name) :name_(name) {}
        const String& GetName()const { return name_; }
        uint64_t IncrExpectedValue() { return ++expected_value_; }
        uint64_t GetExpectedValue() const { return expected_value_; }
        bool IsCompleted()const { return GetSemaphoreCounterValueImpl() >= expected_value_; }
        virtual void Signal(uint64_t expected_value) = 0;
        virtual void Wait(uint64_t expected_value) = 0;
        virtual ~HALSemaphore() = default;
    protected:
        virtual uint64_t GetSemaphoreCounterValueImpl()const = 0;
    protected:
        uint64_t    expected_value_{ 0u };
        const String    name_;
    };

    struct HALBarrier
    {
        union {
            HALResource* resource_{ nullptr };
            HALTexture* texture_;
            HALBuffer* buffer_;
        };
        enum class EType : uint8_t
        {
            eBuffer,
            eTexture,
        };
        enum class EAction :uint8_t
        {
            eInvalid,
            eFlush,
        };
        EType   type_;
        EAction action_;
        static HALBarrier Invalid(HALTexture* texture) {
            HALBarrier barrier{ .texture_ = texture, .type_ = EType::eTexture, .action_ = EAction::eInvalid };
            return barrier;
        }
        static HALBarrier Flush(HALTexture* texture) {
            HALBarrier barrier{ .texture_ = texture, .type_ = EType::eTexture, .action_ = EAction::eFlush };
            return barrier;
        }
        static HALBarrier Invalid(HALBuffer* buffer) {
            HALBarrier barrier{ .buffer_ = buffer, .type_ = EType::eBuffer, .action_ = EAction::eInvalid };
            return barrier;
        }
        static HALBarrier Flush(HALBuffer* buffer) {
            HALBarrier barrier{ .buffer_ = buffer, .type_ = EType::eBuffer, .action_ = EAction::eFlush };
            return barrier;
        }
    };
    struct HALTransition
    {
        union {
            HALResource* resource_{ nullptr };
            HALTexture* texture_;
            HALBuffer* buffer_;
        };
        enum class EType :uint8_t
        {
            eBuffer,
            eTexture,
        };
        EType   type_{ EType::eBuffer };
        EAccessFlags    src_flags_{ EAccessFlags::eNone };
        EAccessFlags    dst_flags_{ EAccessFlags::eNone };
        static HALTransition Texture(HALTexture* texture, EAccessFlags src_flags, EAccessFlags dst_flags) {

        }
        static HALTransition Buffer(HALBuffer* buffer, EAccessFlags src_flags, EAccessFlags dst_flags) {

        }
    };


    //todo rember memory allocation bucket, refactor allocation logic
    struct HALTransientAliasing
    {
        union {
            HALResource*    resource_{ nullptr };
            HALTexture* texture_;
            HALBuffer*  buffer_;
        };
        enum class EType : uint8_t
        {
            eBuffer,
            eTexture,
        };
        enum class EActionType : uint8_t
        { 
            eDiscard,
            eAcquire,
        };
        EType   type_{ EType::eBuffer };
        EActionType action_{ EActionType::eAcquire };
        HALAllocation* allocation_{ nullptr };
        static HALTransientAliasing Acquire(HALTexture* texture) {
            HALTransientAliasing aliasing{ .texture_ = texture, .type_ = EType::eTexture, .action_ = EActionType::eAcquire };
            return aliasing;
        }
        static HALTransientAliasing Discard(HALTexture* texture) {

        }
        static HALTransientAliasing Acquire(HALBuffer* buffer) {

        }
        static HALTransientAliasing Discard(HALBuffer* buffer) {

        }
    };

    namespace TransitionHelper
    {
        template<typename TransitionType>
        static bool IsBuffer(TransitionType&& trans) {
            return trans.type_ == TransitionType::EType::eBuffer;
        }

        template<typename TransitionType>
        static bool IsTexture(TransitionType&& trans) {
            return trans.type_ == TransitionType::EType::eTexture;
        }
    }
    class HALTransitionBatch
    {
    public:
        HALTransitionBatch();
        void AddBarreir(const HALBarrier& barrier);
        void AddTransition(const HALTransition& trans);
        void AddAliasing(const HALTransientAliasing& aliasing);
    private:
        EPipelineStageFlags src_stage_;
        EPipelineStageFlags dst_stage_;
        SmallVector<HALTransition>  transition_;
        SmallVector<HALTransientAliasing>   aliasing_;
    };

}