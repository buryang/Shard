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

		template<typename T>
		static T Get(const Attribute<T>& attr, uint32_t face, uint32_t vert) {
			if (!attr.data_.empty())
			{
				switch (attr.freq_)
				{
				case AttributeFrequency::Constant:
					return attr.data_[0];
				case AttributeFrequency::Uniform:
					return attr.data_[face];
				case AttributeFrequency::Vertex:
					return attr.data_[indices_[face * 3 + vert]];
				case AttributeFrequency::FaceVarying:
					return attr.data_[face * 3 + vert];
				default:
					break;
				}
			}
			return T();
		}

		vec3 GetPosition(uint32_t face, uint32_t vert)const { return Get(positions_, face, vert); }
		vec3 GetNormal(uint32_t face, uint32_t vert)const { return Get(normals_, face, vert); }
		vec2 GetTexCoord(uint32_t face, uint32_t vert)const { return Get(tex_coords_, face, vert); }

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

		//intrisics
		vec3	pos_;
		vec2	center_;
		float	fov_;
		float	skew_;
		float	near_;
		float	far_;

		//extrinsics
		mat4	trans_;
	};

}
