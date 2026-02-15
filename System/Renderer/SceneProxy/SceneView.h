#pragma once
#include "Utils/CommonUtils.h"
#include "Scene/Scene.h"
#include "Graphics/Render/RenderShader.h"
#include "../../App/AppCommon.h"

namespace Shard::Renderer
{

    struct ECSExtractView
    {
        //todo each camera/light has a ECSextract view component
        //ach of these view entities has its ownVisibleEntities component 
        //(or the render-world equivalent like RenderVisibleEntities in some contexts).
        //This is a per - view list that contains references(Entity IDs) to the main - world entities that are visible
    };
    
    /* camerate/viewer projection type */
    enum class EProjectionMode : uint8_t {
        ePerspective,
        eOrthographic
    }; 
    //todo reserve-z

    class SceneViewInstance;
    using ECSCameraComponent = System::App::ECSCameraComponent;

    //class SceneViewInterface

    //define view parameters uniform buffer 
    BEGIN_SHADER_PARAMETER_DEF(ViewUniformShaderParameters)
    SHADER_PARAMETER(float, fov)
    END_SHADER_PARAMETER_DEF()
    
    /**
     * \brief camera/player views informations, user can config view parameters here
     */
    class SceneView //:public SceneViewInterface
    {
    public:
        SceneView() = default;
        SceneView(const String& name, const ECSCameraComponent& camera);
        
        void Initialize(const String& name, float fov, float near_clip, float far_clip, float priority) {
            name_ = name;
            fov_ = fov;
            near_clip_ = near_clip;
            far_clip_ = far_clip;
            priority_ = priority;
        }

        /*sync  world's ecs camera component*/
        void Update(ECSCameraComponent& camera, uint64_t frame_index);
        /*generate split view instance as app configure */
        void Split();
        /*iterator for view instances */
        const auto CBegin() const { return instances_.cbegin(); }
        const auto CEnd() const { return instances_.cend(); }
        auto Count() const { return instances_.size(); }

        //setup view-related shader parameters
        void SetupViewShaderParameters(ViewUniformShaderParameters& param) const;
        void RegisterInstance(SceneViewInstance* instance) const {
            instances_.push_back(instance);
        }
        void UnregisterInstance(SceneViewInstance* instance) const {
            instances_.erase(eastl::remove(instances_.begin(), instances_.end(), instance), instances_.end());
        }
        virtual ~SceneView() noexcept = default;
    protected:
        String  name_;
        float   fov_{ 90.f };
        float   near_clip_{ 0.1f };
        float   far_clip_{ 1000.f };
        float   priority_{ 0.f }; //higher priority view will be rendered first
        uint64_t    frame_counter_{ 0u }; //current frame index£¬ using uint64_t for all frame counter

        uint32_t    reverseZ_ : 1;

        //other parameters shared by all  view instances

    protected:
        mutable Vector<SceneViewInstance*>    instances_;
        //mutable Vector<SceneViewInterface*>    instances_;
    };


#define OVERRIDE_FIELD(type, name, base_field) \
    Optional<type> override_##name##_; \
    void SetVar##name(type v) { override_##name##_ = v; }\
    void GetVar##name() const { return override_##name##_ ? *override_##name##_ : base_->##field; }

    class SceneViewState;
    namespace FXR { class ViewRender; }

    /**
     * \brief actual view instance in scene, using Override-on-Instance pattern
     * viewinstance for vr stereo/mirror camera views
     */
    class SceneViewInstance final: public SceneView
    {
    public:
        explicit SceneViewInstance(const SceneView& base) : base_(base) {
            base_.RegisterInstance(this);
        }
        ~SceneViewInstance() {
            base_.UnregisterInstance(this);
        }

        float4x4 GetWorldTransformMatrix() const;
        float4x4 GetInvWorldTransformMatrix() const;
        //whether it's stero secondary view, sometime secondary view must follow the operation of primary view
        bool IsSecondaryView() const { return is_stero_secondary_view_; }
    protected:
        const SceneView& base_;
        OVERRIDE_FIELD(String, Name, name_);
        OVERRIDE_FIELD(float4x4, ViewMatrix, view_matrix_);
        OVERRIDE_FIELD(uint4, ViewportSize, viewport_size_);
        OVERRIDE_FIELD(float, FOV, fov_);
        OVERRIDE_FIELD(float, NearClip, near_clip_);
        OVERRIDE_FIELD(float, FarClip, far_clip_);

        //world transform
        float3  location_;
        float3  rotation_;

        float4x4    view_matrix_;
        float4x4    inv_view_matrix_;

        float4x4    projection_matrix_;
        float4x4    inv_projection_matrix_;

        uint32_t    is_stero_secondary_view_ : 1; //whether instance's secondary view of the stereo/vr views 
    };

    class SceneViewState
    {
        friend class SceneViewInstance;
    protected:
        uint32_t    is_frozen_state_ : 1;

    public:
        //todo each frame view state and etc.
        void Frozen() {
            is_frozen_state_ = 1u;
        }
        void UnFrozen() {
            is_frozen_state_ = 0u;
        }
    };

    class ScopeSceneViewStateFrozen final
    {
    public:
        explicit ScopeSceneViewStateFrozen(SceneViewState& state) : state_(state) {
            state_.Frozen();
        }
        ~ScopeSceneViewStateFrozen() {
            state_.UnFrozen();
        }
    protected:
        SceneViewState& state_;
    };


}