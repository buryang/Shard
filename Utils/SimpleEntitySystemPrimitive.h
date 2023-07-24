#pragma once
/*for some usually usaged ecs component and system*/
#include "Utils/SimpleEntitySystem.h"
namespace Shard
{
	namespace Utils {

		//for component that freq add/delete to save time
		struct ECSEnableComponent
		{
			bool enable_{ false };
		};
		
		struct ECSNameComponent
		{
			enum
			{
				MAX_NAME_LENGTH = 128,
			};
			char name_[MAX_NAME_LENGTH];
		};

		struct ECSRelationShipComponent
		{
			std::size_t	children_{ 0u };
			//parent
			Entity	parent_{ Entity::Null };
			//prev sibling 
			Entity	prev_{ Entity::Null };
			//next sibling
			Entity	next_{ Entity::Null };
			//first child
			Entity	first_child_{ Entity::Null };
		};

		struct ECSInputComponent
		{

		};

		struct ECSSpatialTransformComponent
		{
			mat4	rotate_trans_scale_;
		};

		using ECSLocalSpatitalTransformComponent = ECSSpatialTransformComponent;
		using ECSWorldSpatialTransformComponent = ECSSpatialTransformComponent;

		struct ECSSpatialBoundBoxComponent
		{
			vec3	original_;
			vec3	extents_;
		};

		struct ECSStaticMeshComponent
		{
			enum class EMobility :uint8_t
			{
				eStatic,
				eDynamic,
			};
		};
		
		struct ECSSkeletalBoneComponent
		{

		};

		struct ECSSkeletalMeshComponent
		{

		};

		struct ECSParticleComponent
		{

		};

		struct ECSGlobalEnvironmentMapComponent
		{

		};

		struct ECSLocalEnvironmentMapComponent
		{

		};

		struct ECSRenderAbleComponent
		{

		};

		//render sprite componenet
		template<typename T>
		struct ECSRenderSpriteComponent : ECSRenderAbleComponent
		{
			union
			{
				struct
				{
					uint32_t	dirty_ : 1;
					uint32_t	static_ : 1;
					uint32_t	visible_ : 1;
				};
				uint32_t	packet_bits_{ 0u };
			}property_;
			T	data_;
			//todo 
			ECSRenderSpriteComponent(T data) :data_(data) {

			}
		};
		
	}
}
