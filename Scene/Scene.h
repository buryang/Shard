#pragma once
#include "Scene/Primitive.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

namespace MetaInit
{

	class EXPORT_API SceneProxyHelper
	{
	public:
		using CurveList = Vector<Curve>;
		using MeshList = Vector<Mesh>;
		using MaterialList = Vector<Material>;
		using LightList = Vector<Light>;
		using VolumeList = Vector<Volume>;
		using CameraList = Vector<Camera>;
		SceneProxyHelper() = default;
		SceneProxyHelper& SetCamera(uint32_t index);
		SceneProxyHelper& AddCamera(Camera&& camera);
		Camera GetCamera() const;
		SceneProxyHelper& AddLight(Light&& light);
		Light GetLight() const;
		SceneProxyHelper& AddMesh(Mesh&& mesh);
		Mesh GetMesh() const;
		SceneProxyHelper& AddMaterials(Material&& material);
		Material GetMaterial(uint32_t id) const;
		SceneProxyHelper& AddTexture(Texture&& texture);
		Texture GetTexture(uint32_t id) const;
	private:
		//load post proc functions

	private:
		CameraList		cameras_;
		Camera*			curr_camera_{nullptr};
		MeshList		meshes_;
		LightList		lights_;
		MaterialList	materials_;
		VolumeList		volumes_;
	};

	class EXPORT_API ISceneParser
	{
	public:
		virtual void Import(const std::string& file, SceneProxyHelper& helper) = 0;
		virtual ~ISceneParser() {};
	};

	class EXPORT_API SceneGltfParser:public ISceneParser
	{
	public:
		void Import(const std::string& gltf_file, SceneProxyHelper& helper) override;
	private:
		struct NodeCache {
			mat4 affine_ = mat4{1.0f};
		};
		void ParseMaterials(SceneProxyHelper& helper);
		void ParseLights(SceneProxyHelper& helper);
		void ParseCamera(SceneProxyHelper& helper, const tinygltf::Camera& camera, const NodeCache& node=NodeCache());
		void ParseSampler(SceneProxyHelper& helper);
		void ParseTextures(SceneProxyHelper& helper);
		void ParseMeshes(SceneProxyHelper& helper, const tinygltf::Mesh& mesh, const NodeCache& node);
		void ParseNode(SceneProxyHelper& helper, const tinygltf::Node& node, const NodeCache& parent=NodeCache());
	private:
		tinygltf::Model			gltf_model_;
		tinygltf::TinyGLTF		gltf_loader_;
	};

}
