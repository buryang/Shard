#pragma once
#include "Utils/CommonUtils.h"

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

	struct Light
	{
		enum class Type:uint8_t
		{
			POINT,
			SPOT,
			DIRECTIONAL,
			AREA,
		};
		vec3	pos_;
		vec3	dir_;
		vec3	intensity_;
	};

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

	struct Volume
	{

	};
	
	struct Texture
	{
		enum EleType : uint32_t
		{
			NONE,
			RGB32FLOAT,
		};
		EleType			type_;
		Vector<uint8_t>	data_;
		uint32_t		width_;
		uint32_t		height_;
	};

	//texture sampler interface
	struct Sampler
	{
		uint32_t mag_filter_;
		uint32_t min_filter_;
		uint32_t warpS_;
		uint32_t warpT_;
	};

	struct Material
	{
		enum class AlphaMode : uint8_t
		{
			AOPAQUE,
			AMASK,
			ABLEND,
		};
		using TexturePtr = Texture*;
		AlphaMode alpha_mode_ = AlphaMode::AOPAQUE;
		float alpha_cutoff_ = 1.0f;
		float metal_factor_ = 1.0f;
		float roughness_factor_ = 1.0f;
		vec4 base_color_factor_{ 1.0f };
		vec4 emissive_factor_{ 1.0f };

		//texture data structure
		TexturePtr base_color_texture_ = nullptr;
		TexturePtr metal_roughness_texture_ = nullptr;
		TexturePtr normal_texture_ = nullptr;
		TexturePtr occlusion_texture_ = nullptr;
		TexturePtr emissive_texture_ = nullptr;
	};

}
