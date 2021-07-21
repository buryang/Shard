#pragma once
#include "Utils/CommonUtils.h"
#include "folly/DiscriminatedPtr.h"

namespace MetaInit
{
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
			Vector<T>			data_;
			AttributeFrequency	freq_ = AttributeFrequency::None;
		};

		Attribute<vec3>		positions_;
		Attribute<vec3>		normals_;
		Attribute<vec2>		tex_coords_;
		Attribute<uint32_t> indices_;
		uint32_t			face_num_;
		uint32_t			vertex_num_;


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
			vec3	pos_;
			vec3	normal_;
			vec2	tex_coord_;
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

	struct Curve
	{
		Vector<vec3>	ctrl_points_;
		Vector<float>	segments_;
		void ToSpherePorint() const;
		Mesh ToMesh() const;
	};

	
	enum class LightType:uint8_t
	{
		POINT,
		SPOT,
		DIRECTIONAL,
		AREA,
		DISTANT,
	};


	struct PointLight
	{
		LightType Type() const { return LightType::POINT; }
	};

	struct SpotLight
	{
		LightType Type() const { return LightType::SPOT; }
	};

	struct DistantLight
	{
		LightType Type() const { return LightType::DISTANT; }
	};

	using LightPtr = folly::DiscriminatedPtr<PointLight, SpotLight, DistantLight>;

	struct Camera
	{
		enum class Type:uint8_t{
			PERSPECTIVE,
			ORTHO,
		};

		Type	type_;
		//intrisics
		vec3	pos_{ 0.0f };
		vec2	center_{ 0.0f };
		float	fov_;
		float	skew_;
		float	znear_;
		float	zfar_;
		float	xmag_;
		float	ymag_;

		//extrinsics
		mat4	affine_{ 1.0f };
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
		uvec3			size_;
		uint32_t		mip_levels_{ 1 };
		uint32_t		array_layers_{ 1 };
		EImageType		type_{ EImageType::TEXTURE_2D };
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
		EFilterType		mag_filter_;
		EFilterType		min_filter_;
		EAddressMode	warpS_;
		EAddressMode	warpT_;
	};

	struct Material
	{
		enum class AlphaMode : uint8_t
		{
			AOPAQUE,
			AMASK,
			ABLEND,
		};
		using TexturePtr = Texture::Ptr;
		AlphaMode alpha_mode_ = AlphaMode::AOPAQUE;
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

}
