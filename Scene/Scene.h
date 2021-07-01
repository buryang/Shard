#pragma once

#include "Utils/CommonUtils.h"
#include "Scene/Primitive.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

namespace MetaInit
{

	class SceneProxyHelper
	{
	public:
		using CurveList = Vector<Curve>;
		using MeshList = Vector<Mesh>;
		using MaterialList = Vector<Material>;
		using LightList = Vector<Light>;
		using VolumeList = Vector<Volume>;
		using CameraList = Vector<Camera>;
		SceneProxyHelper() = default;
		SceneProxyHelper& SetCamera();
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
		Camera			curr_camera_;
		MeshList		meshes_;
		LightList		lights_;
		MaterialList	materials_;
	};

	class ISceneParser
	{
	public:
		virtual void Import(const std::string& file, SceneProxyHelper& helper) = 0;
		virtual ~ISceneParser() {};
	};

	class SceneGltfParser:public ISceneParser
	{
	public:
		void Import(const std::string& gltf_file, SceneProxyHelper& helper) override;
	private:
		void ParseMeshes(SceneProxyHelper& helper);
		void ParseMaterials(SceneProxyHelper& helper);
		void ParseLights(SceneProxyHelper& helper);
		void ParseCameras(SceneProxyHelper& helper);
		void ParseSampler(SceneProxyHelper& helper);
		void ParseTextures(SceneProxyHelper& helper);
	private:
		tinygltf::Model			gltf_model_;
		tinygltf::TinyGLTF		gltf_loader_;
	};

}
