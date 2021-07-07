#include "gtest/gtest.h"
#include <string>
#include "Scene/Scene.h"

using namespace tinygltf;

TEST(TEST_Scene_MODULE, TEST_GLTF_READER)
{
	tinygltf::Model	gltf_model;
	tinygltf::TinyGLTF gltf_loader;

	std::string err, warn;
	//auto ret = gltf_loader.LoadBinaryFromFile(&gltf_model, &err, &warn, "D:/Data/gltf/AttenuationTest.glb");
	auto ret = gltf_loader.LoadASCIIFromFile(&gltf_model, &err, &warn, "D:/Data/gltf/Sponza/glTF/Sponza.gltf");
	ASSERT_TRUE(ret != false && "load gltf file success");
	std::cout << err << std::endl;
	std::cout << warn << std::endl;

	for (const auto& acc : gltf_model.accessors)
	{
		std::cout << acc.count << std::endl;
	}

	for (const auto& image : gltf_model.images)
	{
		std::cout << image.mimeType << " : " << image.uri << std::endl;
	}

}

TEST(TEST_Scene_MODULE, TEST_Scene_API)
{
	using namespace MetaInit;
	using Mesh = MetaInit::Mesh;

	SceneGltfParser parser;
	SceneProxyHelper proxy;

	parser.Import("D:/Data/gltf/Sponza/glTF/Sponza.gltf", proxy);

	Mesh mesh = proxy.GetMesh();
	ASSERT_TRUE(mesh.face_num_ != 0 && "check mesh face bigger than zero");
	ASSERT_TRUE(mesh.positions_.data_.size() > 0);
}

