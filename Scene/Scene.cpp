#include "Scene.h"

/*
*copy code from URL:
*https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/1bfea2ae1e09a9b2429e1eab5a55a405751dc1d2/base/VulkanglTFModel.cpp
*/

namespace MetaInit
{

	SceneProxyHelper& SceneProxyHelper::SetCamera()
	{
		curr_camera_ = nullptr;
		return *this;
	}

	SceneProxyHelper& SceneProxyHelper::AddCamera(Camera camera)
	{
		cameras_.emplace_back(camera);
		return *this;
	}

	Camera SceneProxyHelper::GetCamera() const
	{

	}

	SceneProxyHelper& SceneProxyHelper::AddLight(Light light)
	{

		return *this;
	}

	Light SceneProxyHelper::GetLight() const
	{

	}

	SceneProxyHelper& SceneProxyHelper::AddMesh(Mesh&& mesh)
	{

	}

	Mesh SceneProxyHelper::GetMesh() const
	{

	}

	SceneProxyHelper& SceneProxyHelper::AddMaterials(Material&& material)
	{
		return *this;
	}

	Material SceneProxyHelper::GetMaterial(uint32_t id) const
	{

	}

	void SceneGltfParser::Import(const std::string& gltf_file, SceneProxyHelper& helper)
	{
		std::string err, warn;
		bool load_stat = false;
		if (gltf_file.find("glb") != String::npos)
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
			ParseLights(helper);
		}

		//load camera
		if (!gltf_model_.cameras.empty())
		{
			ParseCameras(helper);
		}

		//load mesh
		if (!gltf_model_.meshes.empty())
		{
			ParseMeshes(helper);
		}

		if (!gltf_model_.materials.empty())
		{
			ParseMaterials(helper);
		}

	}

	void SceneGltfParser::ParseMeshes(SceneProxyHelper& helper)
	{

		auto& buffers = gltf_model_.buffers;
		auto& buffer_views = gltf_model_.bufferViews;
		auto& accessors = gltf_model_.accessors;
		for (auto mesh : gltf_model_.meshes)
		{
			Mesh mesh_helper;
			
			auto prim_trans = [&](tinygltf::Primitive& prim) {
				
				if (prim.indices>=0) 
				{
					const auto& indice_acc = accessors[prim.indices];
					const auto& indice_view = buffer_views[indice_acc.bufferView];
					const auto* indice_data = &buffers[indice_view.buffer].data[indice_acc.byteOffset+indice_acc.byteOffset];
					assert(indice_acc.type == TINYGLTF_TYPE_SCALAR&&"indices should be scalar");
					
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
							uint32_t* indice_data_ = reinterpret_cast<const uint32_t*>(indice_data);
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
					const float* values = reinterpret_cast<const float*>(buffer + index * stride);
					for (auto n = 0; n < comp_num; ++n)
					{
						comps[n] = values[n];
					}
				};

				for (auto& attrib : prim.attributes)
				{
					const auto& attrib_acc = accessors[attrib.second];
					const auto& attrib_view = buffer_views[attrib_acc.bufferView];
					const auto& byte_stride = attrib_acc.ByteStride(attrib_view);
					assert(attrib_acc.type == TINYGLTF_COMPONENT_TYPE_FLOAT && "default vertex data be type float");
					const auto* attrib_data = &buffers[attrib_view.buffer].data[attrib_acc.byteOffset + attrib_acc.byteOffset];
					const auto attrib_comp_count = GetNumComponentsInType(attrib_acc.type);

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
					material_helper.base_color_factor_ = static_cast<float>(values["baseColorFactor"].Factor());
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
						material_helper.alpha_mode_ = Material::AlphaMode::BLEND;
					}
					else if (param.string_value == "MASK")
					{
						material_helper.alpha_cutoff_ = 0.5f;
						material_helper.alpha_mode_ = Material::AlphaMode::MASK;
					}

				}

				if (values.find("alphaCutoff") != values.end())
				{
					material_helper.alpha_cutoff_ = static_cast<float>(values["alphaCutoff"].Factor());
				}

				if (values.find("emissiveFactor") != values.end())
				{
					material_helper.emissive_factor_ = vec4(make_vec3(values["emissiveFactor"].ColorFactor().data()), 1.0f);
				}
			}
			helper.AddMaterials(std::move(material_helper));
		}
	}

	void SceneGltfParser::ParseTextures(SceneProxyHelper& helper)
	{
		//image now only save url
		auto& image_list = gltf_model_.images;
		for (auto& tex : gltf_model_.textures)
		{
			auto& image = image_list[tex.source];
			if (tex.sampler != -1)
			{

			}
			else
			{

			}

			Texture tmp_texture;
			helper.AddTexture(std::move(tmp_texture));
		}
	}
}