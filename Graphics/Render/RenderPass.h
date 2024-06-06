#pragma once
#include "Utils/Handle.h"
#include "Utils/CommonUtils.h"
#include "Utils/SimpleEntitySystem.h"
#include "HAL/HALRenderPass.h"
#include "Render/RenderBarrier.h"
#include "Render/RenderGraph.h"
#include "Render/RenderResources.h"

namespace Shard
{
    namespace Render
    {
        class RenderGraphExecutor;
        //frame graph render pass
        class RenderPass
        {
        public:
            //how to bind resource and field FIXME
            class ScheduleContext
            {
            public:
                ScheduleContext() = default;
                ScheduleContext(ScheduleContext&& other)noexcept;
                ScheduleContext& operator=(ScheduleContext&& other)noexcept;
                ScheduleContext& AddField(Field&& field);
                bool IsEmpty()const { return fields_.empty(); }
                bool IsValid()const { 
                    if (auto iter = eastl::find(fields_.cbegin(), fields_.cend(), [](auto field) {
                            /*at least one DSV/RTV*/
                            return Utils::HasAnyFlags(field.GetSubFieldState().access_, EAccessFlags::eDSVWrite | EAccessFlags::eRTV);
                        }); iter != fields_.cend()) {
                        return true;
                    }
                    return false;
                }
                void EnumerateInputFields(SmallVector<Field>& input_fields);
                void EnumerateOutputFields(SmallVector<Field>& output_fields);
                Field* operator[](const String& name) {
                    if (auto iter = eastl::find_if(fields_.begin(), fields_.end(), [](auto iter) { return iter->GetName() == name; }); iter != fields_.end()) {
                        return std::addressof(*iter);
                    }
                    return nullptr;
                }
                const Field* operator[](const String& name) const {
                    if (auto iter = eastl::find_if(fields_.cbegin(), fields_.cend(), [](const auto iter) { return iter->GetName() == name; }); iter != fields_.cend()) {
                        return std::addressof(*iter);
                    }
                    return nullptr;
                } 
                ScheduleContext& operator+=(ScheduleContext&& other) {
                    fields_.merge(other.fields_); //it'll keep all different element in *this
                }
                bool operator==(const ScheduleContext& other)const {
                    return fields_ == other.fields_;
                }
                bool operator!=(const ScheduleContext& other)const {
                    return !(*this == other);
                }
                const auto& GetFields()const {
                    return fields_;
                }
                auto& GetFields() {
                    return fields_;
                }
                template <typename Function>
                auto& Enumerate(Function&& func) {
                    for (auto& fd: fields_) {
                        func(fd);
                    }
                    return *this;
                }
            private:
                Set<Field>   fields_;
            };

            //pass and edge resource manager
            using PassRepo = Utils::ResourceManager<RenderPass>;
            using Handle = PassRepo::Handle;

            /*global pass create instance*/
            static PassRepo& PassRepoInstance();
        
            explicit RenderPass(const String& name, EPipeline pipeline, bool never_culled);
            virtual ~RenderPass() = default;
            virtual void Init() {}; //setup shader and declare input/ouput fields
            virtual void PostCompile();
            virtual void Execute(HAL::HALCommandContext* context, RenderResourceCache* resource_cache) = 0;
            virtual void UnInit() {};
            virtual void Serialize(Utils::IOArchive& ar);
            FORCE_INLINE ScheduleContext& GetScheduleContext() { return schedule_context_; }
            FORCE_INLINE bool IsAsync()const { return pipeline_ == EPipeline::eAsyncCompute; }
            FORCE_INLINE bool IsAsyncComputeFork()const { return is_compute_fork_; }
            FORCE_INLINE bool IsAsyncComputeJoin()const { return is_compute_join_; }
            FORCE_INLINE bool IsGFXFork()const { return is_gfx_fork_; }
            FORCE_INLINE bool IsGFXJoin()const { return is_gfx_join_; }
            FORCE_INLINE bool IsCullAble()const { return is_culling_able_; }
            FORCE_INLINE const String& GetName()const { return name_; }
            FORCE_INLINE EPipeline GetPipeline()const { return pipeline_; }
            FORCE_INLINE uint16_t& DependencyLevel() { return dependency_level_; }
            FORCE_INLINE uint16_t& QueueIndex() { return queue_index_; }
            FORCE_INLINE uint32_t& GlobalExecuteOrder() { return global_execute_order_; }
            FORCE_INLINE uint32_t& QueueExecuteOrder() { return queue_execute_order_; }
            FORCE_INLINE uint32_t& DependencyLevelExecuteOrder() { return dependency_execute_order_; }
            FORCE_INLINE BarrierBatch& GetPrologureBarrier() { return barriers_prologue_; }
            FORCE_INLINE BarrierBatch& GetEpilogueBarrier() { return barriers_epilogue_; }
        protected:
            virtual void GenerateHALPassInitializer(HAL::HALRenderPassInitializer& initializer);
        private:
            const String    name_;
            EPipeline   pipeline_;
            uint16_t    ref_count_{ 0u };
            uint16_t    dependency_level_{ 0u };
            uint16_t    queue_index_{ 0u };
            //execute orders
            uint32_t    global_execute_order_{ 0u }; //execute order in global orderred pass_
            uint32_t    queue_execute_order_{ 0u };  //execute order in command queue
            uint32_t    dependency_execute_order_{ 0u }; //execute order in a dependency level

            Set<RenderPass::Handle> producers_;
            Set<RenderPass::Handle> consumers_;
            ScheduleContext schedule_context_;

            //render barrier
            BarrierBatch  barriers_prologue_;
            BarrierBatch  barriers_epilogue_;

            //HAL render pass handle
            HAL::HALRenderPass::Handle  hal_handle_;

            union
            {
                struct
                {
                    //generate/wait sync fence for follow pass
                    //whether gfx fork/join    
                    uint8_t    is_gfx_fork_ : 1;
                    uint8_t    is_gfx_join_ : 1;
                    //whether async compute fork/join
                    uint8_t    is_compute_fork_ : 1;
                    uint8_t    is_compute_join_ : 1;
                    //culling able
                    uint8_t    is_culling_able_ : 1;
                };
                uint8_t    pack_bits_{ 0u };
            };
        };

        //lambda render pass
        template <typename SetupLambda, typename ExecuteLambda>
        requires std::is_invocable_v<ExecuteLambda, HAL::HALCommandContext*, RenderResourceCache*> && std::is_invocable_v<SetupLambda, RenderPass::ScheduleContext&>
        class LambdaRenderPass : public RenderPass
        {
        public:
            LambdaRenderPass(const String& name, EPipeline pipe, bool never_culled, ExecuteLambda&& execute_lamda, SetupLambda&& setup_lambda) : RenderPass(name, pipe, never_culled), setup_lambda_(setup_lambda), execute_lambda_(execute_lambda_) {}
            void Init() override {
                setup_lambda_(GetScheduleContext());
            }
            void Execute(HAL::HALCommandContext* context, RenderResourceCache* resource_cache) override {
                execute_lambda_(context);
            }
            virtual ~LambdaRenderPass() {}
        private:
            const SetupLambda& setup_lambda_;
            const ExecuteLambda& execute_lambda_;
        };

        class NullRenderPass : public RenderPass
        {
        public:
            NullRenderPass(const String& name, EPipeline pipe, bool never_culled) :RenderPass(name, pipe, never_culled) {}
            void Execute(HAL::HALCommandContext* context, RenderResourceCache* resource_cache) override {}
        };

        class PrologueRenderPass final : public NullRenderPass
        {
        public:
            PrologueRenderPass(const String& name) :NullRenderPass(name, EPipeline::eGFX, true) {}
            void InitBarrier(uint32_t pass_count, RenderPass::Handle* passes);
        };

        class TextureResolveRenderPass final : public RenderPass
        {
        public:
            TextureResolveRenderPass(const String& name, EPipeline pipe, bool never_culled) :RenderPass(name, pipe, never_culled) {}
            void Init()override;
            void Execute(HAL::HALCommandContext* context, RenderResourceCache* resource_cache) override;
        };

        class PackedRenderPass final : public RenderPass
        {
        public:
            PackedRenderPass(const String& name, Span<RenderPass::Handle>& passes, EPipeline pipline, bool never_culled);
            void Init()override;
            void Execute(HAL::HALCommandContext* context, RenderResourceCache* resource_cache) override;
            void UnInit()override;
            ~PackedRenderPass();
        protected:
            void GenerateHALPassInitializer(HAL::HALRenderPassInitializer& initializer) override;
        private:
            struct SubPassInfo
            {
                struct InputAttachment
                {
                    uint8_t    attachment_ : 7;
                    uint8_t    flags_ : 1;
                };
                SmallVector<uint8_t>   color_attachment;
                SmallVector<InputAttachment> input_attachment;
            };
            struct SubPassDependency
            {
                uint8_t src_sub_pass_{ 0u };
                uint8_t dst_sub_pass_{ 0u };
                EPipelineStageFlags src_stage_flags_{ EPipelineStageFlags::eShaderFrag };
                EPipelineStageFlags dst_stage_flags_{ EPipelineStageFlags::eTopOfPipe };
                EAccessFlags    src_access_{ EAccessFlags::eRTV };
                EAccessFlags    dst_access_{ EAccessFlags::eSRV };
            };
            SmallVector<RenderPass::Handle> sub_passes_;
            SmallVector<SubPassInfo> sub_passes_info_;
            SmallVector<SubPassDependency> sub_passes_dependency_;
        };

    }
}