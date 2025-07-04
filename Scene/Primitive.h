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

    //todo:plane to 4D vector Grassmann Algebra
    struct Plane
    {
        float3    normal_;
        float3    tangent_;
    };

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
        ePOLYGONAL,
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
        vec4    color_;
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
        ufloat3            size_;
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


}
