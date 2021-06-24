#pragma once

#include "Utils/CommonUtils.h"
#include "Scene/Primitive.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

namespace MetaInit
{

	class ScenceProxyHelper
	{
	public:
		ScenceProxyHelper() = default;
		void SetCamera();
		void GetCamera() const;
		void AddLight();
		void GetLight() const;
		void AddMesh();
		void GetMesh() const;
		void AddMaterials();
		void GetMaterials() const;
	private:
		Vector<Mesh>	prims_;
		Vector<Light>	lights_;
	};

	class ISceneParser
	{
	public:
		virtual void Import(const std::string& file, ScenceProxyHelper& helper) = 0;
	};

	class SceneGltfParser:public ISceneParser
	{
	public:
		void Import(const std::string& gltf_file, ScenceProxyHelper& helper) override;
	private:
		void ParseMesh(ScenceProxyHelper& helper);
		void ParseMaterial(ScenceProxyHelper& helper);
		void ParseLight(ScenceProxyHelper& helper);
		void ParseCamera(ScenceProxyHelper& helper);
	private:
		tinygltf::Model			gltf_model_;
		tinygltf::TinyGLTF		gltf_loader_;
	};

}
