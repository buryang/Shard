#include "assimp/Importer.hpp"
#include "assimp/Scene.h"
#include "assimp/postprocess.h"

#include "gtest/gtest.h"

using namespace Assimp;

TEST(TEST_Scene_MODULE, TEST_ASSIMP_READER)
{
	Importer importer;
	const char* fbx_file = "D:/Data/SunTemple_v3/SunTemple/SunTemple.fbx";
	auto Scene = importer.ReadFile( fbx_file,
									aiProcess_GenNormals	 | aiProcess_GenSmoothNormals |
									aiProcess_SplitLargeMeshes | aiProcess_SortByPType);
	EXPECT_TRUE(Scene != nullptr) << "load " << fbx_file << "failed";
	EXPECT_TRUE(Scene->HasMeshes());
}