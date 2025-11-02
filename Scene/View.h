#pragma once
#include "Utils/CommonUtils.h"

namespace Shard::Scene
{
    
    /* camerate/viewer projection type */
    enum class EProjectionMode : uint8_t {
        ePerspective,
        eOrthographic
    };

    class InstanceView;

    /**
     * \brief camera/player views informations, user can config view parameters here
     */
    class ViewBase
    {
    public:
        void RegisterInstance(ViewInstance* instance) const {
            instances_.push_back(instance);
        }
        void UnregisterInstance(ViewInstance* instance) const {
            instances_.erase(eastl::remove(instances_.begin(), instances_.end(), instance), instances_.end());
        }
        virtual ~ViewBase() noexcept = default;
    protected:
        String  name_;
        float   fov_{ 90.f };
        float   near_clip_{ 0.1f };
        float   far_clip_{ 1000.f };
        float   priority_{ 0.f }; //higher priority view will be rendered first
    protected:
        mutable Vector<ViewInstance*>    instances_;
    };


#define OVERRIDE_FIELD(type, name, base_field) \
    std::optional<type> override_##name##_; \
    void SetVar##name(type v) { override_##name##_ = v; }\
    void GetVar##name() const { return override_##name##_ ? *override_##name##_ : base_->##field; }

    /**
     * \brief actual view instance in scene, using Override-on-Instance pattern
     */
    class ViewInstance final: public ViewBase
    {
    public:
        explicit ViewInstance(const ViewBase& base) : base_(base) {
            base_.RegisterInstance(this);
        }
        ~ViewInstance() {
            base_.UnregisterInstance(this);
        }

        float4x4 GetWorldTransformMatrix() const;
        float4x4 Get
    protected:
        const ViewBase& base_;
        OVERRIDE_FIELD(String, Name, name_);
        OVERRIDE_FIELD(float4x4, ViewMatrix, view_matrix_);
        OVERRIDE_FIELD(uint4, ViewportSize, viewport_size_);
        OVERRIDE_FIELD(float, FOV, fov_);
        OVERRIDE_FIELD(float, NearClip, near_clip_);
        OVERRIDE_FIELD(float, FarClip, far_clip_);

        //world transform
        float3  location_;
        float3  rotation_;

        float4x4    world_transform_matrix_;
        float4x4    inv_world_transform_matrix_;

        float4x4    view_matrix_;
        float4x4    inv_view_matrix_;
    };

}