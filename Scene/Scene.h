#pragma once
#include "Utils/SimpleEntitySystem.h"
#include "Utils/SimpleEntitySystemPrimitive.h"
#include "Scene/Primitive.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "tiny_gltf.h"
#include <memory>

namespace Shard
{
	namespace Scene
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
			friend class RuntimeHDRPRenderer;
			CameraList		cameras_;
			Camera* curr_camera_{ nullptr };
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

		class MINIT_API SceneGltfParser :public ISceneParser
		{
		public:
			void Import(const String& gltf_file, SceneProxyHelper& helper) override;
		private:
			struct NodeCache {
				mat4 affine_ = mat4{ 1.0f };
			};
			void ParseMaterials(SceneProxyHelper& helper);
			void ParseLights(SceneProxyHelper& helper);
			void ParseCamera(SceneProxyHelper& helper, const tinygltf::Camera& camera, const NodeCache& node = NodeCache());
			void ParseMeshes(SceneProxyHelper& helper, const tinygltf::Mesh& mesh, const NodeCache& node);
			void ParseNode(SceneProxyHelper& helper, const tinygltf::Node& node, const NodeCache& parent = NodeCache());
		private:
			tinygltf::Model			gltf_model_;
			tinygltf::TinyGLTF		gltf_loader_;
			std::string				gltf_dir_;
		};

		class MINIT_API SceneLoadSystem
		{
		public:
			static void Init();
			static void UnInit();
			static void Tick(float dt);
		private:
			SceneLoadSystem() = default;
			DISALLOW_COPY_AND_ASSIGN(SceneLoadSystem);
		};

		enum class ESystemPriorLevel
		{
			ePrePhyX,
			ePhyX,
			ePostPhyX,
			eRender,
		};

		class WorldScene;

		struct WorldSceneUpdateContext final: Utils::ECSSystemUpdateContext
		{
			WorldSceneUpdateContext();
			WorldScene::Ptr GetScence();
		};

		//ecs scene interface
		class MINIT_API WorldScene : public Utils::ECSAdmin<int> //todo
		{
		public:
			using Ptr = WorldScene*;
			class EntityProxy: public std::enable_shared_from_this<EntityProxy>
			{
			public:
				using SharedPtr = std::shared_ptr<EntityProxy>;
				EntityProxy(WorldScene* world) :world_(world) {
					entity_ = world_->NewEntity();
				}
				EntityProxy(const EntityProxy& other):world_(other.world_) {
					entity_ = world_->Clone({ other });
				}
				SharedPtr GetPtr() {
					return shared_from_this();
				}
				operator Utils::Entity() {
					return entity_;
				}
				operator bool() {
					return bool(entity_);
				}
				template<typename Component, typename ...Args>
				void Assign(Utils::Entity entt, Args&& ...args) {
					world_->AddComponent<Component>(entt, std::forward(args)..);
				}
				template<typename Component>
				void Remove(Utils::Entity entt) {
					world_->RemoveComponent<Component>(entt);
				}
				template<typename Component>
				Component& Get() {
					admin_->GetComponent<Component>(*this);
				}
				void AttachEntity(Utils::Entity parent, Utils::Entity child)
				{
					//attach children to parent
					Assign<Utils::ECSRelationShipComponent>(child, {} );
				}

				void DetachEntity(Utils::Entity entt) {
					Remove<Utils::ECSRelationShipComponent>(entt);
				}

				void DetachChildrenEntity(Utils::Entity parent) {

				}

				~EntityProxy() {
					world_->DelEntity(*this);
				}
			private:
				Utils::Entity	entity_;
				Ptr	world_{ nullptr };
			};
			
			virtual void Tick(float dt);
			virtual ~WorldScene() = default;
			EntityProxy::SharedPtr CreateEntity();
			void Serialize(const String& world_file);
			void UnSerialize(const String& world_file) const;
		};
	}
}

