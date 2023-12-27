#pragma once
#include "Utils/CommonUtils.h"
#include "folly/DiscriminatedPtr.h"

namespace Shard
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
               Vector<T>               data_;
               AttributeFrequency     freq_ = AttributeFrequency::None;
          };

          struct MeshLet
          {
               Utils::Entity     material_{ Utils::Entity::Null };
               uint32_t     offset_{ 0u };
               uint32_t     indices_count_{ 0u };
          };

          Attribute<vec3>          positions_;
          Attribute<vec3>          normals_;
          Attribute<vec2>          tex_coords_;
          Attribute<uint32_t> indices_;
          uint32_t               face_num_;
          uint32_t               vertex_num_;
          //tessellation factor
          float     tessellation_factor_{ 0.f };


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

          void AddPosition(vec3 pos) { positions_.data_.emplace_back(pos); }
          const vec3 GetPosition(uint32_t face, uint32_t vert)const { return Get(positions_, face, vert); }
          void AddNormal(vec3 normal) { normals_.data_.emplace_back(normal); }
          const vec3 GetNormal(uint32_t face, uint32_t vert)const { return Get(normals_, face, vert); }
          void AddTexCoord(vec2 uv) { tex_coords_.data_.emplace_back(uv); }
          const vec2 GetTexCoord(uint32_t face, uint32_t vert)const { return Get(tex_coords_, face, vert); }
          void AddIndice(uint32_t index) { indices_.data_.emplace_back(index); }

          struct Vertex
          {
               vec3     pos_;
               vec3     normal_;
               vec3     tangent_;
               vec3     bitangent_;
               vec2     tex_coord_;
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

     struct Cone
     {
          vec3     pos_;
          vec3     normal_;
     };

     //mesh cluster/lod 
     struct MeshCluster
     {
          static constexpr uint32_t     MAX_CLUSTER_LOD = 8u;
          uint32_t     lod_{ 0u };
          uint32_t     offset_{ 0u };
          uint32_t     indices_count_{ 0u };
          OBB     bbox_;
          Cone     bcone_;
          //todo
     };

     struct StaticMesh : Mesh
     {

     };

     struct SkinMesh : Mesh
     {

     };

     struct SkinBone
     {
          uint32_t     id_{ 0u };
     };


     struct ECSStaticMeshComponent
     {
          StaticMesh* handle_{ nullptr };
     };

     struct ECSSkinMeshComponent
     {
          SkinMesh* handle_{ nullptr };
     };

     struct Curve
     {
          SmallVector<vec3>     ctrl_points_;
          SmallVector<float>     segments_;
     };

     struct Rect
     {
          vec2     xy_{ 0.f, 0.f };
          vec2     wh_{ 1.f, 1.f };
     };

     struct Sphere
     {
          vec3     xyz_{ 0.f, 0.f, 0.f };
          float     radius_;
     };

     struct Cylinder
     {
          vec3     xyz_;
          vec3     axis_;
          float     half_height_;
          float     radius_;
     };

     struct Capsule
     {
          vec3     xyz_;
          vec3     axis_;
          float     half_height_;
          float     radius_;
     };

     struct OBB
     {
          vec3     xyz_;
          vec3     size_;
          vec3     axisX_;
          vec3     axisY_;
          vec3     axisZ_;
     };

     struct AABB
     {
          vec3     xyz_;
          vec3     size_;
     };
     
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

     using LightPtr = folly::DiscriminatedPtr<PointLight, SpotLight, DistantLight>;

     struct Camera
     {
          enum class EType:uint8_t{
               PERSPECTIVE,
               ORTHO,
          };

          EType     type_;
          //intrisics
          vec3     pos_{ 0.0f };
          vec2     center_{ 0.0f };
          float     fov_;
          float     skew_;
          float     znear_;
          float     zfar_;
          float     xmag_;
          float     ymag_;

          //extrinsics
          mat4     affine_{ 1.0f };
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
          uvec3               size_;
          uint32_t          mip_levels_{ 1 };
          uint32_t          array_layers_{ 1 };
          EImageType          type_{ EImageType::TEXTURE_2D };
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
          EFilterType          mag_filter_;
          EFilterType          min_filter_;
          EAddressMode     warpS_;
          EAddressMode     warpT_;
     };

     struct Material
     {
          enum class AlphaMode : uint8_t
          {
               eOPAQUE = 0x00,
               eMASK     = 0x01,
               eBLEND     = 0x02,
          };
          using TexturePtr = Texture::Ptr;
          AlphaMode alpha_mode_ = AlphaMode::eOPAQUE;
          bool double_sided_ = false;
          float alpha_cutoff_ = 1.0f;
          float metal_factor_ = 1.0f;
          float roughness_factor_ = 1.0f;
          vec4 base_color_factor_{ 1.0f };
          vec4 emissive_factor_{ 1.0f };

          //texture data structure
          TexturePtr base_color_texture_;
          TexturePtr metal_roughness_texture_ ;
          TexturePtr normal_texture_;
          TexturePtr occlusion_texture_;
          TexturePtr emissive_texture_;
     };

     struct EnvironmentMap
     {
          Texture::Ptr     cube_map_{ nullptr };
     };

     struct SkyBoxMap : EnvironmentMap
     {

     };

     struct EnvironmentProbeMap
     {
          Texture::Ptr     probes_;
     };

     struct SkyboxEnviromentMap : EnvironmentMap
     {
          Texture::Ptr     skybox_radiance_{ nullptr };
          float     intensity_{ 1.0f };
          float     exposure_{ -1.f };
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




}
