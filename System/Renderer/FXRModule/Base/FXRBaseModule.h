#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/Memory.h"
#include "Graphics/Render/RenderShader.h"
#include "Graphics/Render/RenderGraphBuilder.h"
#include "../../RenderCommon.h"


namespace Shard::Renderer::FXR //FXR stands for Full eXperience Renderer
{
    using ViewRender = ECSViewRenderRelevant;

    /*render stage phase, each fxr module should do different works for each stage*/
    enum class ERenderPhase : uint8_t
    {
        ePreInitView,
        ePreRender,
        eRender,
        ePostRender,
    };

#if 0  //implement module depends
    struct FXRDrawInfo
    {
        SmallVector<size_type>   prev_dep_; //modules must executor before current module
        SmallVector<size_type>   next_dep_; //modules must executor after current module
    };

    static auto& GetFXRRegistry() {
        static Map<std::type_index, FXRDrawInfo> registry;
        return registry;
    }

    template<class ThisClass, class ...PrevClass>
    struct BeforeDepend
    {
        BeforeDepend(){
            const auto this_id = typeid(ThisClass);
            auto& this_registry = GetFXRRegistry()[this_id].perv_dep_;
            auto expander = [&](std::type_index prev_id) {
                if (std::find(prev_deps.begin(), prev_deps.end(), this_id.hash_code()) == prev_deps.end())
                {
                    this_registry.emplace_back(prev_id.hash_code);
                    GetFXRRegistry()[prev_id].next_dep_.emplace_back(this_id.hash_code);
                }
            };
            (expander(typeid(PrevClass), ...);
        }
    };

    template<class ThisClass, class ...AfterClass>
    struct AfterDepend
    {
        AfterDepend() {
            const auto this_id = typeid(ThisClass);
            auto& this_registry = GetFXRRegistry()[this_id].next_dep_;
            auto expander = [&](std::type_index next_id) {
                if (std::find(next_deps.begin(), next_deps.end(), this_id.hash_code()) == next_deps.end())
                {
                    this_registry.emplace_back(next_id.hash_code);
                    GetFXRRegistry()[next_id].prev_dep_.emplace_back(this_id.hash_code);
                }
                };
            (expander(typeid(AfterClass), ...);
        }
    };
#endif

    /** abstract interface for vfx/other rendering module for input view*/
    class MINIT_API FXRDrawBase
    {
        //declare prev/next module rely here(if needed) like this
        //BeforeDepend<ThisFXR, ...OtherFXR> _prev_deps;
        //AfterDepend<ThisFXR, ...OtherFXR> _next_deps;
    public:
        struct ViewRenderState
        {
            //dummy render state for each view
            //regist for each module
        };
        //all render jobs logic should execute while rendering 
        virtual void Draw(Render::RenderGraph& builder, Span<ViewRender>& views, ERenderPhase phase) = 0;
        //sometime we include fxr module in pipeline, but it's frozen so pre-draw/draw will not work as normal or not work

#if 0  //features are group of similar objects and their shared rendering jobgs;
        /* realize render feature extract entry point*/
        virtual JobHandle OnFrameBeginExtract();
        virtual JobHandle ExtractPerFrame();
        virtual JobHandle ExtractPerView();
        virtual JobHandle FinalizeExtractPerView();
        virtual JobHandle OnFrameFinalizeExtract();

        /*entry point for preparing*/
        virtual JobHandle OnFrameBeginPrepare();
        virtual JobHandle PreparePerFrame();
        virtual JobHandle PreparePerView();
        virtual JobHandle FinalizePreparePerView();
        virtual JobHandle OnFrameFinalizePrepare();

        /*entry point for submit*/
        virtual JobHandle OnSubmitNodeBlockBegin();
        virtual JobHandle SubmitNode();
        virtual JobHandle OnSubmitNodeBlockEnd();
#endif
        //fxr module view state related utils
        template<class ViewRenderState, class ...Args>
        void RegistViewState(Args&& ...args) {
            xx.RegisterComponent((std::forward<Args...>(args));
        }

        //allocate view state for each view entity
        template<class ViewRenderState, class ...Args>
        ViewRenderState& AllocateViewState(Utils::Entity view_entity, Args&&... args) {
            return xx.GetComponentRepos<ViewRenderState>().Spawn(view_entity, (std::forward<Args...>(args)); //todo component repo functions
        }

        void Frozen(bool frozen) { is_frozen_ = !!frozen; }
        bool IsFrozen()const { return is_frozen_; }
        float Priority() const { return priority_; }
        void SetPriority(float priority) { priority_ = priority; }
		virtual ~FXRDrawBase() = default;
    protected:
        void CacheMeshDrawCommands(Render::RenderGraphBuilder& builder);
        void GetCachedMeshDrawCommands();
    protected:
        mutable uint32_t    is_frozen_ : 1; //frozen current module
        uint32_t    priority_{ 0u };        //if sort modules, using this as key
    };

#define DECLARE_RENDER_STATE_OP(void)                       \
    private:                                                \
        SmallVector<ViewRenderState*>    view_render_states_;\
        auto* GetViewState(uint32_t view_id) {              \
            return view_render_states_[view_id];            \
        }                                                   \
        const auto* GetViewState(uint32_t view_id) const {  \
            return view_render_states_[view_id];            \
        }                                                   \
        void AllocateViewRenderState(ViewRender& view_render)\
        {                                                   \
            /*allocate states from view's allocator*/       \   
            auto* render_state = new(view_render->allocator_->Alloc(sizeof(ViewRenderState)))ViewRenderState; \
            view_render_states_.emplace_back(render_state); \
         }

#define DECLARE_RENDER_INDENTITY(class_name)                                \
        uint32_t GetTypeHash() const                                        \
        {                                                                   \
            static const auto type_info = typeid(class_name);               \
            return reinterpret_cast<uint32_t>(type_info.hash_code());       \
        }
                                            
    using FXRArray = SmallVector<FXRDrawBase*>;
}

