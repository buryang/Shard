#pragma once
#include "Utils/SimpleEntitySystem.h"
#include "Scene/Primitive.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

namespace Shard
{

	template <typename Primitive>
	struct PrimitiveComponent : public ECSCompoenent
	{
		using Type = Primitive;
		Type	value_;
	};

	class MINIT_API SceneProxyHelper
	{
	public:
		using Ptr = SceneProxyHelper*;
		using SharedPtr = std::shared_ptr<SceneProxyHelper>;
		using CurveList = Vector<Curve>;
		using MeshList = Vector<Mesh>;
		using MaterialList = Vector<Material>;
		using LightList = Vector<LightPtr>;
		//using VolumeList = Vector<Volume>;
		using CameraList = Vector<Camera>;
		SceneProxyHelper() = default;
		SceneProxyHelper& SetCamera(uint32_t index);
		SceneProxyHelper& AddCamera(Camera&& camera);
		Camera GetCamera() const;
		SceneProxyHelper& AddLight(LightPtr light);
		LightPtr GetLight() const;
		//fixme how to manage scene data
		SceneProxyHelper& AddMesh(Mesh&& mesh);
		Mesh GetMesh() const;
		SceneProxyHelper& AddMaterials(Material&& material);
		Material GetMaterial(uint32_t id) const;
		SceneProxyHelper& AddTexture(Texture&& texture);
		Texture GetTexture(uint32_t id) const;
	private:
		//load post proc functions
	private:
		friend class MyToySimpleRenderer;
		CameraList		cameras_;
		Camera*			curr_camera_{nullptr};
		MeshList		meshes_;
		LightList		lights_;
		MaterialList	materials_;
		//VolumeList		volumes_;
	};

	class MINIT_API ISceneParser
	{
	public:
		virtual void Import(const String& file, SceneProxyHelper& helper) = 0;
		virtual ~ISceneParser() = default;
	};

	class MINIT_API SceneGltfParser:public ISceneParser
	{
	public:
		void Import(const String& gltf_file, SceneProxyHelper& helper) override;
	private:
		struct NodeCache {
			mat4 affine_ = mat4{1.0f};
		};
		void ParseMaterials(SceneProxyHelper& helper);
		void ParseLights(SceneProxyHelper& helper);
		void ParseCamera(SceneProxyHelper& helper, const tinygltf::Camera& camera, const NodeCache& node=NodeCache());
		void ParseMeshes(SceneProxyHelper& helper, const tinygltf::Mesh& mesh, const NodeCache& node);
		void ParseNode(SceneProxyHelper& helper, const tinygltf::Node& node, const NodeCache& parent=NodeCache());
	private:
		tinygltf::Model			gltf_model_;
		tinygltf::TinyGLTF		gltf_loader_;
		std::string				gltf_dir_;
	};

	class MINIT_API SceneLoadSystem : public 
	{

	};

	//ecs scene interface
	class MINIT_API Scene : public Utils::ECSAdmin<>
	{
	public:
		virtual void Tick(float dt);
		virtual ~Scene() = default;
	};

}
