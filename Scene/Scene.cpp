#include "Scene.h"

namespace MetaInit
{

	void SceneGltfParser::Import(const std::string& gltf_file, ScenceProxyHelper& helper)
	{
		std::string err, warn;
		bool load_stat = false;
		if (gltf_file.find("glb") != std::string::npos)
		{
			load_stat = gltf_loader_.LoadBinaryFromFile(&gltf_model_, &err, &warn, gltf_file);
		}
		else
		{
			load_stat = gltf_loader_.LoadASCIIFromFile(&gltf_model_, &err, &warn, gltf_file);
		}

		if (!load_stat)
		{
			throw std::runtime_error("load gltf file failed");
		}

		//load light
		if (!gltf_model_.lights.empty())
		{
			ParseLight(helper);
		}

		//load camera
		if (!gltf_model_.cameras.empty())
		{
			ParseCamera(helper);
		}

	}

	void SceneGltfParser::ParseMesh(ScenceProxyHelper& helper)
	{
		for (auto mesh:gltf_model_.meshes)
		{

		}
	}
}