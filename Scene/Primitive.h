#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "folly/DiscriminatedPtr.h"
#include "Graphics/Render/RenderResources.h"
#include "Material.h"

/**
 * \relates https://gdcvault.com/play/1020787/Math-for-Game-Programmers-Introduction
 *  Grassmann Algebra for Intesection calc
 */

#define MAX_STATIC_MESH_LODS 8u

namespace Shard::Scene
{

    enum class EInputTopoType :uint8_t
    {
        eUnkown,
        ePointList,
        eLineList,
        eLineStrip,
        eTriangleList,
        eTriangleStrip,
        eTriangleFAN,
        eLineListAdj,
        eLineStripAdj,
        eTriangleListAdj,
        eTriangleStripAdj,
        //to do d3d control pointer patch list
        ePatchList,
        eNum,
    };

    //copy from falcor
    struct Mesh
    {
        enum class AttributeFrequency
        {
            None,
            Constant,       ///< Constant value for mesh. The element count must be 1.
            Uniform,        ///< One value per face. The element count must match `faceCount`.
            Vertex,         ///< One value per vertex. The element count must match `vertexCount`.
            FaceVarying,    ///< One value per vertex per face. The element count must match `indexCount`.
        };

        template<typename T>
        struct Attribute
        {
            Vector<T>            data_;
            AttributeFrequency    freq_ = AttributeFrequency::None;
        };

        struct MeshLet
        {
            Utils::Entity    material_{ Utils::Entity::Null };
            uint32_t    offset_{ 0u };
            uint32_t    indices_count_{ 0u };
        };

        Attribute<float3>        positions_;
        Attribute<float3>        normals_;
        Attribute<float3>        tex_coords_;
        Attribute<uint32_t> indices_;
        uint32_t            face_num_;
        uint32_t            vertex_num_;
        //tessellation factor
        float    tessellation_factor_{ 0.f };


        template<typename T>
        const T Get(const Attribute<T>& attr, uint32_t face, uint32_t vert) const {
            if (!attr.data_.empty())
            {
                switch (attr.freq_)
                {
                case AttributeFrequency::Constant:
                    return attr.data_[0];
                case AttributeFrequency::Uniform:
                    return attr.data_[face];
                case AttributeFrequency::Vertex:
                    return attr.data_[indices_.data_[face * 3 + vert]];
                case AttributeFrequency::FaceVarying:
                    return attr.data_[face * 3 + vert];
                default:
                    break;
                }
            }
            return T();
        }

        void AddPosition(float3 pos) { positions_.data_.emplace_back(pos); }
        const float3 GetPosition(uint32_t face, uint32_t vert)const { return Get(positions_, face, vert); }
        void AddNormal(float3 normal) { normals_.data_.emplace_back(normal); }
        const float3 GetNormal(uint32_t face, uint32_t vert)const { return Get(normals_, face, vert); }
        void AddTexCoord(float2 uv) { tex_coords_.data_.emplace_back(uv); }
        const float2 GetTexCoord(uint32_t face, uint32_t vert)const { return Get(tex_coords_, face, vert); }
        void AddIndice(uint32_t index) { indices_.data_.emplace_back(index); }

        struct Vertex
        {
            float3    pos_;
            float3    normal_;
            float3    tangent_;
            float3    bitangent_;
            float2    tex_coord_;
        };

        //https://www.yosoygames.com.ar/wp/2018/03/vertex-formats-part-1-compression/
        struct VertexCompressed
        {
            uint16_t position[4];
            uint16_t qtangent[4]; //https://pdfs.semanticscholar.org/73b6/fa8f3ab6348975c3715c2f2d152f6b5c5296.pdf
            uint16_t uv[2];
        };

        Vertex GetVertex(uint32_t face, uint32_t vert)const
        {
            Vertex v;
            v.pos_ = GetPosition(face, vert);
            v.normal_ = GetNormal(face, vert);
            v.tex_coord_ = GetTexCoord(face, vert);
            return v;
        }

    };

    struct TerrainSection
    {
        AABB    local_bounding_;
        Mesh    vertex_;
    };

    struct TerrainData
    {
        Vector<TerrainSection>  sections_;
        AABB    local_bounding_;
        //float2    resolution_;
        float2    size_; //size in world 
        float   bump_scale_{ 1.f };


        //property flag bits
        uint32_t    with_cpu_usage_ : 1;
        //todo others

        //todo HAL

        //todo layers
    };

    struct Cone
    {
        float3    pos_;
        float3    normal_;
    };

    struct HalfEdge
    {
        uint32_t    twin_;
        uint32_t    prev_;
        uint32_t    next_;
        uint32_t    vertex_;
        uint32_t    face_;
        uint32_t    index_;
    };

    struct Triangle
    {
        float3 vertices_[3];
    };

    //todo:plane to 4D vector Grassmann Algebra
    struct Plane
    {
        float4 normal_;
        float4 tangent_;
    };

    /*
    * frustum planes are ordered left, right, top, bottom, near, far
    * the normal vectors point toward the interior of the frustum
    */
    struct Frustum
    {
        enum {
            NUM_PLANES = 6u,
        };
        float4 xyzw_[NUM_PLANES];
    };

    template<uint32_t planes>
    struct ConvexVolume
    {
        enum {
            NUM_PLANES = planes,
        };
        float4 xyzw_[NUM_PLANES];
    };

    //ABCD/Hessian Representation, permuted plane data, re-range for SIMD intrinsics
    struct PermutedFrustum
    {
        enum {
            NUM_DIMS = 6,
            NUM_DIMS_VEC4 = (NUM_DIMS + 3u) / 4u,
            NUM_DIMS_REMAIN = NUM_DIMS % 4u,
        };
        float4 xxxx_[NUM_DIMS_VEC4];
        float4 yyyy_[NUM_DIMS_VEC4];
        float4 zzzz_[NUM_DIMS_VEC4];
        float4 wwww_[NUM_DIMS_VEC4];

    };

    template<uint32_t planes>
    struct PermutedConvexVolume
    {
        enum {
            NUM_DIMS = planes,
            NUM_DIMS_VEC4 = (NUM_DIMS + 3u) / 4u,
            NUM_DIMS_REMAIN = NUM_DIMS % 4u,
        };
        float4 xxxx_[NUM_DIMS_VEC4];
        float4 yyyy_[NUM_DIMS_VEC4];
        float4 zzzz_[NUM_DIMS_VEC4];
        float4 wwww_[NUM_DIMS_VEC4];
    };

    template<class Volume, class PermutedVolume>
    PermutedVolume VolumePermute(const Volume& vol)
    {
        static_assert(PermulatedVolume::NUM_DIMS == Volume::NUM_PLANES);

        PermutedVolume permuted_vol;
        constexpr auto permute_count = PermutedVolume::NUM_DIMS_VEC4;
        constexpr auto permute_remain = PermutedVolume::NUM_DIMS_REMAIN;

        auto n = 0, offset = 0;
        for (; n < permute_count-1; ++n, offset += 4)
        {
            //permute to SIMD ready order
            permuted_vol.xxxx_[n] = float4(0, 0, 0, 0);
            permuted_vol.yyyy_[n] = float4(0, 0, 0, 0);
            permuted_vol.zzzz_[n] = float4(0, 0, 0, 0);
            permuted_vol.wwww_[n] = float4(0, 0, 0, 0);
        }

        if constexpr (permute_remain)
        {
            float4 xxxx = float4(0.f);
            float4 yyyy = float4(0.f);
            float4 zzzz = float4(0.f);
            float4 wwww = float4(0.f);
            
            if constexpr (permute_remain > 0u)
            {
                xxxx[0] = xx;
                yyyy[0] = xx;
                zzzz[0] = xx;
                wwww[0] = xx;
            }

            if constexpr (permute_remain > 1u)
            {
                xxxx[1] = xx;
                yyyy[1] = xx;
                zzzz[1] = xx;
                wwww[1] = xx;
            }

            if constexpr (permute_remain > 2u)
            {
                normal[2] = xx;
                tangent[2] = xx;
            }
            
            permuted_vol.xxxx_[n] = float4(normal[0].x, normal[1].x, normal[2].x, normal[3].x);
            permuted_vol.yyyy_[n+1] = float4(normal[0].y, normal[1].y, normal[2].y, normal[3].y);
            permuted_vol.zzzz_[n+2] = float4(normal[0].z, normal[1].z, normal[2].z, normal[3].z);
            permuted_vol.wwww_[n+3] = float4(normal[0].w, normal[1].w, normal[2].w, normal[3].w);

        }

        return permuted_vol;
    }

    template<class PermutedVolume>
    bool IntersectBoxTest(const PermutedVolume& vol, const float3& box_origin, const float3& box_extent)
    {
        const float4 cx = float4(box_origin.x);
        const float4 cy = float4(box_origin.y);
        const float4 cz = float4(box_origin.z);

        const float4 ex = float4(box_extent.x);
        const float4 ey = float4(box_extent.y);
        const float4 ez = float4(box_extent.z);

        for(auto n = 0; n < PermutedVolume::NUM_DIMS_VEC4; ++n)
        {
            //base dot = |n|¡¤center + d
            float4 dot_center = vol.xxxx_[i] * cx  + vol.yyyy_[i] * cy +
                    vol.zzzz_[i] * cz + vol.wwww_[i];

            //radius in plane normal direction = |n|¡¤extent
            float4 radius  = abs(vol.xxxx_[i]) * ex + abs(vol.yyyy_[i]) * ey +
                    abs(vol.zzzz_[i]) * ez;
            //the box is completely on the negative side if: dot_center + radius < 0
            //the padding xyzw components are all zero, so no problem
            if (any(dot_center + radius < 0.0f))
                return false;
        }

        return true;
    }

    template<class PermutedVolume>
    bool IntersectSphereTest(const PermutedVolume& vol, float4 sphere)
    {
        float4 cx(sphere.x), cy(sphere.y), cz(sphere.z), r(sphere.z);

        for (auto n = 0; n < PermutedVolume::NUM_DIMS_VEC4-1; ++n)
        {
            float4 dist = cx * vol.xxxx_[n] + cy * vol.yyyy_[n] + cz * vol.zzzz_[n] + vol.wwww_[n];
            if( glm::any(glm::lessThan(dist, -r))){
                return false;
            }
        }

        constexpr auto remain = PermutedVolume::NUM_DIMS_REMAIN;
        float4 dist = cx * vol.xxxx_[n] + cy * vol.yyyy_[n] + cz * vol.zzzz_[n] + vol.wwww_[n];
        auto is_outside = glm::lessThan(dist, -r);
        if constexpr (!remain)
        {
            return !is_outside.x;
        }
        else if constexpr (remain == 1u) {
            if( dist.x < -r.x ){
                return false;
        }
        else if constexpr (remain == 2u) {
            return !is_outside.x || !is_outside.y;
        }
        else if constexpr (remain == 3u) {
            return !is_outside.x || !is_outside.y || !is_outside.z;
        }

        return true;

    }

    //template<class PermutedVolume>
    //bool IntersectTriangleTest(const PermutedVolume& vol, const Triangle& tri)
    //{
    //}

    struct SubMeshIndexInfo32 //copy from unity 
    {
        uint32_t    start_index_ : 20;
        uint32_t    length_ : 7;
        uint32_t    reserved_ : 4;
        uint32_t    material_index_enabled_ : 1;
        SubMeshIndexInfo32() {
            *(uint32_t*)(this) = 0u;
        }
    };

    //mesh cluster/lod 
    /*
    struct MeshCluster
    {
        static constexpr uint32_t    MAX_CLUSTER_LOD = 8u;
        uint32_t    lod_{ 0u };
        uint32_t    offset_{ 0u };
        uint32_t    indices_count_{ 0u };
        OBB    bbox_;
        Cone    bcone_;
        //todo
    };
    */
    namespace NVClusterLOD
    {
        struct Cluster
        {
            uint32_t triangle_count_minus1_;
            uint32_t vertex_count_minus1_;
            uint32_t lod_level_;
            uint32_t group_child_index_;
            uint32_t groupID_;
            //material ID, according to cbt paper, use vertex position as UV to sample from texture&height field
            //uint32_t materialID_;
        };

        struct TraversalMetric
        {
            //scalar by design, avoid hiccups with packing
            //order must match `nvclusterlod::Node`
            float bounding_sphereX_;
            float bounding_sphereY_;
            float bounding_sphereZ_;
            float bounding_sphere_radius_;
            float max_quadric_error_;
        };

        struct Group
        {
            uint32_t geimetryID_;
            uint32_t groupID_;

            uint32_t residentID_;
            uint32_t cluster_residentID_;

            //when this group is first loaded, this is where the
            //temporary clas builds start
            uint32_t streaming_new_build_offset_;

            uint16_t lod_level_;
            uint16_t cluster_count_;

            //todo Meta: Bounds, LOD info, Material tables, etc.
            //The resident pages are stored in one big ByteAddressBuffer on the GPU.
        };

        struct Node
        {
            Scene::AABB bound_box_;

        };

        static_assert(alignof(Cluster) && alignof(Group) && alignof(Node) && "should align as hlsl code");

    }

    struct SkinBone
    {
        uint32_t    id_{ 0u };
    };

    struct VertexStream
    {
        enum EType : uint8_t
        {
            ePos,
            eNormal,
            eTangent,
            eColor,
            eTextureUV,
            eTextureUV1,
            eShadowUV,
        };
        const Render::RenderBuffer* vertex_buffer_; //todo change to handle
        uint8_t stream_index_{ 0u };
        uint8_t offset_{ 0u };
        EType   type_;
        //todo attribute index
        uint8_t    stride_{ 0u };
    };

    class VertexFactoryRenderInterface : public Render::RenderResource
    {
    public:
        using Handle = uint32_t; //todo
        template<typename  Function>
        void EnumerateVertexStreams(Function&& func) {
            for (auto& stream : vertex_streams_) {
                func(stream);
            }
        }
        virtual VertexStream* GetVertexStream(uint32_t index) = 0;
        virtual bool IsStaticMesh()const = 0;
        virtual ~VertexFactoryRenderInterface() {}
    protected:
        Array<VertexStream, 2>  vertex_streams_;
        //stream rhi declare todo
    };

    class MeshFactoryInterface
    {
    public:
        virtual void Init() {}
        virtual void UnInit() {}
        virtual void Tick() = 0;
        virtual bool IsStaticMesh() const = 0;
        //todo fix mesh collect interface
        virtual bool CollectRenderMeshBatch(const auto& filter, auto& mesh_collector)const = 0;

        //material releated
        virtual PrimitiveMaterialInterface* GetMaterial(uint32_t material_index) const { return nullptr; }
        virtual bool SetMaterial(uint32_t material_index, PrimitiveMaterialInterface* material) { return true; }
        virtual PrimitiveMaterialInterface* GetMaterialByHash(const PrimitiveMaterialInterface::HashName& name) const { return nullptr; }
        virtual bool SetMaterial(const PrimitiveMaterialInterface::HashName& name, PrimitiveMaterialInterface* material) { return true; }
        virtual ~MeshFactoryInterface() {}
    };

    //todo bind shader to mesh factory

    class MeshSection
    {
    public:
        MeshSection() = default;

    protected:
        Render::RenderBuffer* vertex_buffer_{ nullptr };
    };

    class MeshLOD
    {
    public:
        void Init();
    protected:
        SmallVector<MeshSection>    sections_;
        //ray tracing geometry
        RayTracingGeometry  raytracing_geometry_;

        //bit properties
        uint32_t    with_raytracing_geometry_ : 1;
        uint32_t    with_depth_only_indices_ : 1;
        uint32_t    with_color_vertex_data_ : 1;
    };

    //static mesh and procedual mesh factory
    class StaticMeshFactory : public MeshFactoryInterface
    {
    public:
        bool IsStaticMesh() const final { return true; }
        bool CollectRenderMeshBatch(const auto& filter, auto& mesh_collector) const override;
    protected:
        SmallVector<MeshLOD>    lods_;
        SmallVector<PrimitiveMaterialInterface*>   materials_;
    };

    class SkinMeshFactory : public MeshFactoryInterface    {
    public:
        bool IsStaticMesh() const final { return false; }
        bool CollectRenderMeshBatch(const auto& filter, auto& mesh_collector) const override;
    };

    //todo error nedd mesh compoent wrap abstract mesh not factory
    struct ECSMeshComponent : ECSyncObjectRefComponent<MeshFactoryInterface>
    {
        SubMeshIndexInfo32  sub_mesh_;
    };

    //todo HLOD Group
    //todo virtual geometry

    struct Curve
    {
        SmallVector<float3>    ctrl_points_;
        SmallVector<float>    segments_;
    };

    struct Rect
    {
        float2    xy_{ 0.f, 0.f };
        float2    wh_{ 1.f, 1.f };
    };

    struct Sphere
    {
        float3    xyz_{ 0.f, 0.f, 0.f };
        float    radius_;
    };

    struct Cylinder
    {
        float3    xyz_;
        float3    axis_;
        float    half_height_;
        float    radius_;
    };

    struct Capsule
    {
        float3    xyz_;
        float3    axis_;
        float    half_height_;
        float    radius_;
    };

    struct OBB
    {
        float3    lo_;
        float3    hi_;
        float3    axisX_;
        float3    axisY_;
        float3    axisZ_;
    };

 
    struct AABB
    {
        float3    lo_;
        float3    hi_;
    };

    //https://gpfault.net/posts/aabb-tricks.html said below AABB is better for ray intersection test
    //will write in hlsl later
    struct AABBMinMax
    {
        float x_minmax_[2];
        float y_minmax_[2];
        float z_minmax_[2];
    };

    inline float3 GetAABBCorner(const AABBMinMax& aabb, uint32_t index) {
        return float3(aabb.x_minmax_[index & 0x1],
            aabb.y_minmax_[(index & 0x2) >> 1],
            aabb.z_minmax_[(index & 0x4) >> 2]);
    }

    inline AABBMinMax EnclosingAABBs(const AABBMinMax& lhs, const AABBMinMax& rhs) {
        return AABBMinMax{
            { eastl::min(lhs.x_minmax_[0], rhs.x_minmax_[0]), eastl::max(lhs.x_minmax_[1], rhs.x_minmax_[1]) },
            { eastl::min(lhs.y_minmax_[0], rhs.y_minmax_[0]), eastl::max(lhs.y_minmax_[1], rhs.y_minmax_[1]) },
            { eastl::min(lhs.z_minmax_[0], rhs.z_minmax_[0]), eastl::max(lhs.z_minmax_[1], rhs.z_minmax_[1]) },
        };
    }

    inline AABBMinMax AABBToAABBMinMax(const AABB& aabb) {
        return AABBMinMax{
            { aabb.lo_.x, aabb.hi_.x },
            { aabb.lo_.y, aabb.hi_.y },
            { aabb.lo_.z, aabb.hi_.z },
        };
    }

    struct BoundingBoxSphere
    {
        float3  center_;
        float3  extent_;
        float   radius_;
    };

    /*get corner of the AABB bounding box*/
    static inline float3 GetCorner(const AABB& aabb, uint32_t index) {
        return glm::mix(aabb.lo_, aabb.hi_, float3(index & 0x1, index & 0x2, index & 0x4));
    }
    
    enum class ELightType:uint8_t
    {
        ePOINT,
        eSPOT,
        eDIRECTIONAL,
        eAREA,
        eDISTANT,
        eRECTANGLE,
        ePOLYGONAL,
        eLINE,
    };


    struct PointLight
    {
        enum { Type = ELightType::ePOINT };
    };

    struct SpotLight
    {
        enum { Type = ELightType::eSPOT };
    };

    struct DistantLight
    {
        enum { Type = ELightType::eDISTANT };
    };

    struct PolygonalLight
    {
        enum { Type = ELightType::ePOLYGONAL };
    };

    //unreal engine light light
    struct ECSLightComponent
    {
        float3    pos_;
        float3    direction_;
        float3    tangent_;
        float4    color_;
        float   fall_off_exp_;
        float   specular_scale_;
        float   spot_angles_;
    };

    using LightPtr = folly::DiscriminatedPtr<PointLight, SpotLight, DistantLight>;

    struct Camera
    {
        enum class EType:uint8_t{
            PERSPECTIVE,
            ORTHO,
        };

        EType    type_;
        //intrisics
        float3    pos_{ 0.0f };
        float2    center_{ 0.0f };
        float    fov_;
        float    skew_;
        float    znear_;
        float    zfar_;
        float    xmag_;
        float    ymag_;

        //extrinsics
        float4x4    affine_{ 1.0f };
    };

    /*
    struct Volume
    {

    };
    */
    
    struct Texture
    {
        using Ptr = std::shared_ptr<Texture>;
        enum class SlotType : uint32_t
        {
            BASECOLOR,
            SPECULAR,
            EMISSIVE,
            NORMAL,
            OCCLUSION,
            COUNT,
        };
    };

    enum class EImageType : uint32_t
    {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_3D,
        COUNT,
    };

    struct Image
    {
        Vector<uint8_t> raw_;
        uint2           size_;
        uint32_t        mip_levels_{ 1 };
        uint32_t        array_layers_{ 1 };
        EImageType        type_{ EImageType::TEXTURE_2D };
        Image(Image&& image);
    };


    /*sampler filter type*/
    enum class EFilterType : uint32_t
    {
        NEAREST,
        LINEAR,
    };

    enum class EAddressMode : uint8_t
    {
        WARP,
        REPEAT, //??
        MIRROR,
        CLAMP,
        BORDER,
        MIRROR_ONCE,
    };

    enum class EResourceType : uint32_t
    {
        UNKOWN,
        //todo add other format 
        COUNT,
    };

    //texture sampler interface
    struct Sampler
    {
        EFilterType        mag_filter_;
        EFilterType        min_filter_;
        EAddressMode    warpS_;
        EAddressMode    warpT_;
    };

    struct Material
    {
        enum class AlphaMode : uint8_t
        {
            eOPAQUE = 0x00,
            eMASK    = 0x01,
            eBLEND    = 0x02,
        };
        using TexturePtr = Texture*;
        AlphaMode alpha_mode_ = AlphaMode::eOPAQUE;
        bool double_sided_ = false;
        float alpha_cutoff_ = 1.0f;
        float metal_factor_ = 1.0f;
        float roughness_factor_ = 1.0f;
        float4 base_color_factor_{ 1.0f };
        float4 emissive_factor_{ 1.0f };

        //texture data structure
        TexturePtr base_color_texture_;
        TexturePtr metal_roughness_texture_ ;
        TexturePtr normal_texture_;
        TexturePtr occlusion_texture_;
        TexturePtr emissive_texture_;
    };

    struct EnvironmentMap
    {
        Texture*    cube_map_{ nullptr };
    };

    struct SkyBoxMap : EnvironmentMap
    {

    };

    struct EnvironmentProbeMap
    {
        Texture*    probes_;
    };

    struct SkyboxEnviromentMap : EnvironmentMap
    {
        Texture*    skybox_radiance_{ nullptr };
        float    intensity_{ 1.0f };
        float    exposure_{ -1.f };
    };

    //todo material
    struct CombinedMaterial
    {
        static constexpr uint32_t MAX_MATERIAL_LAYERS = 10;
        Material layers_[MAX_MATERIAL_LAYERS];
    };

    struct Terrain
    {

    };

    template<typename>
    struct Volumetric
    {
        float absorb_{ 0.f };
        float scatter_{ 0.f };
        float green_phase_{ 1.f };
    };

    template<typename>
    struct VolumetricFog : Volumetric<>
    {

    };

    template<typename>
    struct VolumetricCloud : Volumetric<>
    {

    };


    class MINIT_API PrimitiveMeshFactory
    {
    public:
        using HashType = SpookyV2Hash32;
        explicit PrimitiveMeshFactory(const String& name) :name_(name) {
            hash_.FromString(name);
        }
        FORCE_INLINE HashType HashName() const { return hash_; }
        virtual uint32_t GetVertexStreamCount() const = 0;
        virtual void GetVertexStream(uint32_t stream_index) const = 0;
        virtual ~PrimitiveMeshFactory() = 0;
    protected:
        String  name_;
        HashType hash_;
    };

    //primitive render relevant
    struct ECSPrimitveRelevant
    {
        uint32_t    layer_flags_;
    };

    inline bool IsLayerFlagsEnabled(const ECSPrimitiveRevant& relevant, uint32_t layer_mask)
    {
        return (relevant.layer_flags_ & layer_mask) != 0u;
    }

    struct ECSLightRelevant
    {
        //IES
        uint32_t ies_atlas_;
    };

    //tag that hidden a light, avoid render it
    using ECSLightHiddenTag = Uitls::ECSTagComponent<struct LightHidden>;

    /*
     * each paricle system instance has own indepent hal buffer, especially number of system is few
     * some situation maybe share buffer, 
     * 1.Mesh Renderers: If using the same static mesh asset across
     * systems, the base vertex/index data (from the mesh) is shared. but instance-specific transforms/-
     * particles use per-system dynamic buffers.
     * 2.Sprite/Ribbon Renderers: GPU buffers (e.g., structured buffers for positions) are per-emitter,
     * but sorting (if enabled) uses a temporary shared index buffer on GPU for reordering across particles in one draw call.
     */
    struct ECSVFXParticleSystem
    {

    };

    struct ECSSpatialTransformComponent
    {
        float4x4    rotate_trans_scale_;
    };

    struct ECSpatialQuaternionComponent
    {
        float4    quatern_;
    };

    struct ECSSpatialDualQuaternionComponent
    {
        float4    real_;
        float4    imag_;
    };

    struct ECSSpatialSE3Component
    {
        float3    lie_vec_;
    };

    using ECSLocalSpatitalTransformComponent = ECSSpatialTransformComponent; //local-to-world
    using ECSWorldSpatialTransformComponent = ECSSpatialTransformComponent; //inverse

    //view proj matrix
    using ECSViewProjMatrixComponent = ECSSpatialTransformComponent; //view matrix
    using ECSViewInvProjMatrixComponent = ECSSpatialTransformComponent; //inverse view matrix
    //translated transform component(move origin to camera position)
    using ECSTranslatedViewMatrixComponent = ECSSpatialTransformComponent;
    using ECSTranslatedViewInvMatrixComponent = ECSSpatialTransformComponent; //view matrix to translated world

    inline ECSTranslatedViewMatrixComponent MakeTranslatedViewMatrix(const ECSViewProjMatrixComponent& view_matrix, const float3& view_tanslation)
    {
        return xx;
    }

    inline ECSTranslatedViewInvMatrixComponent MakeTranslatedViewInvMatrix(const ECSViewInvProjMatrixComponent& view_inv_matrix, const float3& view_translation)
    {
        return xx;
    }

    using ECSSpatialBoundBoxComponent = BoundingBoxSphere;
    using ECSLocalSpatitalBoundBoxComponent = ECSSpatialBoundBoxComponent;
    using ECSWorldSpatitalBoundBoxComponent = ECSSpatialBoundBoxComponent;

    struct ECSStaticMeshComponent
    {
        enum class EMobility :uint8_t
        {
            eStatic,
            eDynamic,
        };
    };

    struct ECSMeshLODGroupComponent
    {
        uint32_t    lod_{ 0u };
    };

    struct ECSGlobalEnvironmentMapComponent
    {

    };

    struct ECSLocalEnvironmentMapComponent
    {

    };

    //render sprite componenet
    template<typename T>
    struct ECSRenderSpriteComponent : ECSRenderAbleComponent
    {
        union
        {
            struct
            {
                uint32_t    dirty_ : 1;
                uint32_t    static_ : 1;
                uint32_t    visible_ : 1;
            };
            uint32_t    packet_bits_{ 0u };
        }property_;
        T    data_;
        //todo 
        ECSRenderSpriteComponent(T data) :data_(data) {

        }
    };

    //instance render layer/properties
    struct ECSInstanceLayerComponent
    {
        uint8_t is_shadow_caster_ : 1;
        uint8_t is_occluder_ : 1;
        uint8_t is_depth_passes_ : 1; //include in depth render pass
        uint8_t is_main_passes_ : 1; //included in main pass
        uint8_t is_dither_lod_enabled_ : 1; //whether support dither lod
    }
}
