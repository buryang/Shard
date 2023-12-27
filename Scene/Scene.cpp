#include "Scene.h"
#include "Utils/CommonUtils.h"
/*
*copy code from URL:
*https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/1bfea2ae1e09a9b2429e1eab5a55a405751dc1d2/base/VulkanglTFModel.cpp
*/

namespace Shard
{

	SceneProxyHelper& SceneProxyHelper::SetCamera(uint32_t index)
	{
		curr_camera_ = &cameras_[index];
		return *this;
	}

	SceneProxyHelper& SceneProxyHelper::AddCamera(Camera&& camera)
	{
		cameras_.emplace_back(camera);
		return *this;
	}

	Camera SceneProxyHelper::GetCamera() const
	{
		return Camera();
	}

	SceneProxyHelper& SceneProxyHelper::AddLight(LightPtr light)
	{

		return *this;
	}

	LightPtr SceneProxyHelper::GetLight() const
	{
		return LightPtr();
	}

	SceneProxyHelper& SceneProxyHelper::AddMesh(Mesh&& mesh)
	{
		return *this;
	}

	Mesh SceneProxyHelper::GetMesh() const
	{
		return Mesh();
	}

	SceneProxyHelper& SceneProxyHelper::AddMaterials(Material&& material)
	{
		return *this;
	}

	Material SceneProxyHelper::GetMaterial(uint32_t id) const
	{
		return Material();
	}

	void SceneGltfParser::Import(const std::string& gltf_file, SceneProxyHelper& helper)
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

		auto dir_end = gltf_file.find_last_of('/\\');
		gltf_dir_ = gltf_file.substr(0, dir_end);

		//load light
		if (!gltf_model_.lights.empty())
		{
			ParseLights(helper);
		}

		if (!gltf_model_.materials.empty())
		{
			ParseMaterials(helper);

		}

		//deal with scene and node
		auto default_scene = gltf_model_.defaultScene >= 0 ? gltf_model_.defaultScene : 0;
		const auto& scene = gltf_model_.scenes[default_scene];
		//for (const auto& scene : gltf_model_.Scenes)
		{
			//FIXME for access node from scene, so will not access one node twice
			for (const auto& node : scene.nodes)
			{
				ParseNode(helper, gltf_model_.nodes[node]);
			}
		}

	}

	void SceneGltfParser::ParseMaterials(SceneProxyHelper& helper)
	{
		//ParseTextures(helper);
		for (auto material : gltf_model_.materials)
		{
			Material material_helper;
			
			//values
			{
				auto& values = material.values;

				if (values.find("baseColorTextures") != values.end())
				{
					material_helper.base_color_texture_ = nullptr;//FIXME

				}

				if (values.find("metallicRoughnessTexture") != values.end())
				{
					material_helper.metal_roughness_texture_ = nullptr; //FIXME

				}

				if (values.find("roughnessFactor") != values.end())
				{
					material_helper.roughness_factor_ = static_cast<float>(values["roughnessFactor"].Factor());
				}

				if (values.find("metallicFactor") != values.end())
				{
					material_helper.metal_factor_ = static_cast<float>(values["metallicFactor"].Factor());
				}

				if (values.find("baseColorFactor") != values.end())
				{
					material_helper.base_color_factor_ = vec4(glm::make_vec3(values["baseColorFactor"].ColorFactor().data()), 1.0f);
				}

			}
			//addtional values
			{
				auto& values = material.additionalValues;
				
				if (values.find("normalTexture") != values.end())
				{
					material_helper.normal_texture_ = nullptr; //FIXME
				}

				if (values.find("emissiveTexture") != values.end())
				{
					material_helper.emissive_texture_ = nullptr; //FIXME
				}

				if (values.find("occlusionTexture") != values.end())
				{
					material_helper.emissive_texture_ = nullptr; //FIXME
				}

				if (values.find("alphaMode") != values.end())
				{
					auto& param = values["alphaMode"];
					if (param.string_value == "BLEND")
					{
						material_helper.alpha_mode_ = Material::AlphaMode::ABLEND;
					}
					else if (param.string_value == "MASK")
					{
						material_helper.alpha_cutoff_ = 0.5f;
						material_helper.alpha_mode_ = Material::AlphaMode::AMASK;
					}

				}

				if (values.find("alphaCutoff") != values.end())
				{
					material_helper.alpha_cutoff_ = static_cast<float>(values["alphaCutoff"].Factor());
				}

				if (values.find("emissiveFactor") != values.end())
				{
					material_helper.emissive_factor_ = vec4(glm::make_vec3(values["emissiveFactor"].ColorFactor().data()), 1.0f);
				}
			}
			helper.AddMaterials(std::move(material_helper));
		}
	}

	void SceneGltfParser::ParseLights(SceneProxyHelper& helper)
	{

	}

	void SceneGltfParser::ParseCamera(SceneProxyHelper& helper, const tinygltf::Camera& camera, const NodeCache& node)
	{
		Camera camera_helper;

		camera_helper.affine_ = node.affine_;

		if (camera.type == "perspective")
		{
			camera_helper.type_ = Camera::Type::PERSPECTIVE;
			camera_helper.fov_ = static_cast<float>(camera.perspective.yfov);
			camera_helper.zfar_ = static_cast<float>(camera.perspective.zfar);
			camera_helper.znear_ = static_cast<float>(camera.perspective.znear);
		}
		else
		{
			camera_helper.type_ = Camera::Type::ORTHO;
			camera_helper.xmag_ = static_cast<float>(camera.orthographic.xmag);
			camera_helper.ymag_ = static_cast<float>(camera.orthographic.ymag);
			camera_helper.znear_ = static_cast<float>(camera.orthographic.znear);
			camera_helper.zfar_ = static_cast<float>(camera.orthographic.zfar);
		}

		helper.AddCamera(std::move(camera_helper));
	}

	void SceneGltfParser::ParseMeshes(SceneProxyHelper& helper, const tinygltf::Mesh& mesh, const NodeCache& node)
	{
		//todo whether record the position min and max for BoundingBox AABB
		auto& buffers = gltf_model_.buffers;
		auto& buffer_views = gltf_model_.bufferViews;
		auto& accessors = gltf_model_.accessors;
		//for (auto mesh : gltf_model_.meshes)
		{
			Mesh mesh_helper;

			auto prim_trans = [&](const tinygltf::Primitive& prim) {
				//only supoort triangle now
				if (prim.mode != TINYGLTF_MODE_TRIANGLES)
					return;

				if (prim.indices >= 0)
				{
					const auto& indice_acc = accessors[prim.indices];
					const auto& indice_view = buffer_views[indice_acc.bufferView];
					const auto* indice_data = &buffers[indice_view.buffer].data[indice_acc.byteOffset + indice_acc.byteOffset];
					assert(indice_acc.type == TINYGLTF_TYPE_SCALAR && "indices should be scalar");

					switch (indice_acc.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						for (auto n = 0; n < indice_acc.count; ++n)
						{
							mesh_helper.AddIndice(indice_data[n]);
						}
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						const uint16_t* indice_data_ = reinterpret_cast<const uint16_t*>(indice_data);
						for (auto n = 0; n < indice_acc.count; ++n)
						{
							mesh_helper.AddIndice(indice_data_[n]);
						}
					}
					break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					{
						const uint32_t* indice_data_ = reinterpret_cast<const uint32_t*>(indice_data);
						for (auto n = 0; n < indice_acc.count; ++n)
						{
							mesh_helper.AddIndice(indice_data_[n]);
						}
					}
					break;
					default:
						throw std::invalid_argument("not supoort indice tpye");
					}

				}

				auto vert_extract = [](const uint8_t* buffer, const uint32_t index, const uint32_t byte_stride, float* comps, const uint32_t comp_num) {
					const float* values = reinterpret_cast<const float*>(buffer + index * byte_stride);
					for (auto n = 0; n < comp_num; ++n)
					{
						comps[n] = values[n];
					}
				};
				
				assert(prim.attributes.find("POSITION") != prim.attributes.end() && "primite must have position attrib");
				for (auto& attrib : prim.attributes)
				{
					const auto& attrib_acc = accessors[attrib.second];
					const auto& attrib_view = buffer_views[attrib_acc.bufferView];
					const auto& byte_stride = attrib_acc.ByteStride(attrib_view);
					assert(attrib_acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && "default vertex data be type float");
					const auto* attrib_data = &buffers[attrib_view.buffer].data[attrib_acc.byteOffset + attrib_acc.byteOffset];
					const auto attrib_comp_count = tinygltf::GetNumComponentsInType(attrib_acc.type);

					if (attrib.first == "POSITION")
					{
						for (auto v = 0; v < attrib_acc.count; ++v)
						{
							vec4 position{ 1.0f };
							vert_extract(attrib_data, v, byte_stride, &position[0], attrib_comp_count);
							mesh_helper.AddPosition(position);
						}
					}
					else if (attrib.first == "TEXCOORD_0")
					{
						for (auto v = 0; v < attrib_acc.count; ++v)
						{
							vec2 uv;
							vert_extract(attrib_data, v, byte_stride, &uv[0], attrib_comp_count);
							mesh_helper.AddTexCoord(uv);
						}
					}
					else if (attrib.first == "COLOR_0")
					{
						for (auto v = 0; v < attrib_acc.count; ++v)
						{
							vec3 color;
							vert_extract(attrib_data, v, byte_stride, &color[0], attrib_comp_count);
						}
						//todo
					}
					else if (attrib.first == "TANGENT")
					{
						//todo
					}
					else if (attrib.first == "NORMAL")
					{
						for (auto v = 0; v < attrib_acc.count; ++v)
						{
							vec3 normal;
							vert_extract(attrib_data, v, byte_stride, &normal[0], attrib_comp_count);
							mesh_helper.AddNormal(normal);
						}
					}
				}
				return;
			};
			std::for_each(mesh.primitives.begin(), mesh.primitives.end(), prim_trans);
			helper.AddMesh(std::move(mesh_helper));
		}
	}

	void SceneGltfParser::ParseNode(SceneProxyHelper& helper, const tinygltf::Node& node, const NodeCache& parent)
	{
		mat4 affine{ 1.0f };
		if (node.matrix.size() == 16)
		{
			affine = glm::make_mat4x4(node.matrix.data());
		}
		else 
		{
			//with spearate trans/rot/scale
			//https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_004_ScenesNodes.md
			vec3 trans_vec{ 0.f }, scale_vec{ 1.0f };
			if (node.translation.size() == 3)
			{
				trans_vec = glm::make_vec3(node.translation.data());
			}
			auto trans = glm::translate(glm::mat4(1.0f), trans_vec);

			if (node.scale.size() == 3)
			{
				scale_vec = glm::make_vec3(node.scale.data());
			}
			auto scale = glm::scale(glm::mat4(1.0f), scale_vec);
			mat4 rot{ 1.0f };
			if (node.rotation.size() == 4)
			{
				rot = glm::mat4_cast(glm::make_quat(node.rotation.data()));
			}

			affine = trans * rot * scale;
		}

		NodeCache node_cache;
		node_cache.affine_ = parent.affine_ * affine;
		if (node.children.size() > 0)
		{
			for (const auto& child : node.children)
			{
				ParseNode(helper, node, node_cache);
			}
		}

		//FIXME node matrix to mesh 
		if (node.mesh >= 0)
		{
			const auto& tiny_mesh = gltf_model_.meshes[node.mesh];
			ParseMeshes(helper, tiny_mesh, node_cache);
		}
				
		if (node.camera >= 0)
		{
			ParseCamera(helper, gltf_model_.cameras[node.camera], node_cache);
		}
	}

	SceneProxyHelper& SceneProxyHelper::AddTexture(Texture&& texture)
	{
		return *this;
	}
}

EntityProxy::SharedPtr Shard::Scene::WorldScene::CreateEntity()
{
	return std::make_shared<EntityProxy>(this);
}

WorldScene::Ptr Shard::Scene::WorldSceneUpdateContext::GetScence()
{
	return static_cast<WorldScene::Ptr>(admin_);
}
