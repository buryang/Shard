#define TINYGLTF_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"

#include "gtest/gtest.h"
#include <string>

using namespace tinygltf;

TEST(TEST_SCENE_MODULE, TEST_GLTF_READER)
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

TEST(TEST_SCENE_MODULE, TEST_SCENE_API)
{

}

