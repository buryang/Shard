#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/Memory.h"
#include "Graphics/Render/RenderShader.h"
#include "Graphics/Render/RenderGraphBuilder.h"
#include "../../RenderCommon.h"


namespace Shard::Renderer::FXR //FXR stands for Full eXperience Renderer
{

    struct ViewVisibleLightInfo
    {
        uint32_t lightID_;
        uint32_t is_in_view_frustum_ : 1;
        uint32_t is_in_draw_range_ : 1;
    };

    //todo
    class ViewLocalAllocator final
    {
    public:
        ViewLocalAllocator(size_type size) : capacity_(size)
        {
            bulk_ptr_ = GetRendererAllocator().allocate(size, 1u);
        }
        ~ViewLocalAllocator()
        {
            if (nullptr != bulk_ptr_) {
                GetRendererAllocator().deallocate(bulk_ptr_, capacity_);
            }
        }

        [[nodiscard]] void* allocate(size_type size, size_type n);
        void deallocate(void* ptr, size_type n);
        const size_type max_size() { return capacity_; }
    protected:
        DISALLOW_ASSIGN_AND_COPY(ViewLocalAllocator);
        void* bulk_ptr_{ nullptr };
        size_type capacity_{ 0u };
        size_type offset_{ 0u };
    };

    using ViewInstanceMask = BitVector<ViewLocalAllocator>;

    /*
    * \brief view render related resource shared by all FXR modules   
    */
    struct ViewRender
    {
        //view matrix
        float4x4    view_matrix_;
        float4x4    prev_view_matrix_;
        float4x4    inv_view_matrix_;

        float4x4    proj_matrix_;
        float4x4    inv_proj_matrix_;

        Render::BufferHandle    view_uniform_buffer_;
        //fxr module shared render target
        Render::TextureHandle    render_target_;

        //render frame index
        uint64_t    frame_index_;

        //render related flags
        uint32_t    flags_;

        //visible lights( for example spot/point/texture light in view frustum)
        //directional lights are always visible
        RenderArray<ViewVisibleLightInfo>   visible_lights_;

        /*allocator to allocate view related resource */
        ViewLocalAllocator  allocator_;

        //view related instance visiblity mask
        ViewInstanceMask    instance_mask_;

        //LOD selection view proxy, for stereo/VR need only using left view for LOD selection
        //to avoid inconsistent between views, default set to the view render itself
        const ViewRender* lod_proxy_view_{ nullptr }; 
        float lod_distance_factor_{ 1.f };

    };

    inline float4x4 GetViewMatrix(const ViewRender& view_render)
    {
        return view_render.view_matrix_;
    }

    inline float4x4 GetInvViewMatrix(const ViewRender& view_render)
    {
        return view_render.prev_view_matrix_;
    }

    //check whether delegate LOD selection to another view
    inline const ViewRender& GetLODViewProxy(const ViewRender& view_render)
    {
        assert(view_render.lod_proxy_view_ != nullptr);
        return *view_render.lod_proxy_view_;
    }

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
            //i wanna realize that allocate state from view's allocator
            //all fxr module state should be allocated from view's allocator
        };
		//all render jobs logic should execute while rendering 
    	virtual void Draw(Render::RenderGraph& builder, Span<ViewRender>& views, ERenderPhase phase) = 0;
        //sometime we include fxr module in pipeline, but it's frozen so pre-draw/draw will not work as normal or not work
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

