#pragma once
/*for some usually usaged ecs component and system*/
#include "Utils/SimpleEntitySystem.h"

namespace Shard
{
    namespace Utils {

        //for component that freq add/delete to save time
        template<typename Tag>
        struct ECSEnableComponent
        {
            mutable bool enable_{ false };
            FORCE_INLINE bool IsEnabled() const { return enable_; }
            FORCE_INLINE void Enable() { enable_ = true; }
            FORCE_INLINE void Disable() { enable_ = false; }
        };

        template<typename T>
        struct ECSEnableTemplateComponent : ECSEnableComponent<T>
        {
            T    value_;
            operator T& () { return value_; }
        };

        /*no dense data marker, a component has no dense data*/
        struct NoDenseDataPayload {};
        //empty tag component
        //usage : using ECSDummyTag = ECSTagComponent<struct Dumy_>;
        template<typename Tag>
        struct ECSTagComponent final : NoDenseDataPayload
        {
        };

        //wildcard for generating query 
        using ECSWildcard = ECSTagComponent<struct Wildcard>;

        struct ECSNameComponent
        {
            enum
            {
                MAX_NAME_LENGTH = 128u,
            };
            Tchar name_[MAX_NAME_LENGTH];
        };

        template<typename Relation, typename Target>
        struct ECSRelationUnionComponent
        {
            union {
                struct {
                    uint32_t target_;
                    uint32_t relation_ : 31;
                    uint32_t pair_flags_ : 1;
                };
                uint64_t packed_;
            }
        };

        //parent-child hierarchy
        struct ECSChildOf {};

        //owned-by ownership relationship
        struct ECSOwnedBy {};

        //linked-to generic link
        struct ECSLinkedTo {};


        //https://ajmmertens.medium.com/building-games-in-ecs-with-entity-relationships-657275ba2c6c
        struct ECSRelationShipComponent
        {
            size_type    children_{ 0u };
            //parent
            Entity    parent_{ Entity::Null };
            //prev sibling 
            Entity    prev_sibling_{ Entity::Null };
            //next sibling
            Entity    next_sibiling_{ Entity::Null };
            //first child
            Entity    first_child_{ Entity::Null };
            FORCE_INLINE Entity Parent()const { return parent_; }
            FORCE_INLINE bool HasParent()const { return parent_; }
            FORCE_INLINE bool HasChildren()const { return children_ != 0u; }
            FORCE_INLINE size_type    GetChildrenCount()const { return children_; }
        };

        template<typename Allocator>
        void OnRelationShipContruct(ComponentRepo<ECSRelationShipComponent, Allocator>* repo, Entity entt) {
            const auto& relation = repo->Element(repo->ToIndex(entt));//todo
            if (relation.parent_) {
                auto& parent_relation = repo->Element(repo->ToIndex(relation.parent_));
                const auto temp_first_child = parent_relation.first_children_;
                relation.next_ = temp_first_child;
                parent_relation.first_children_ = entt;
                if (temp_first_child) {
                    repo->Element(repo->ToIndex(temp_first_child)).prev_ = entt;
                }
                ++parent_relation.children_;
            }

            //do not add children by parent 
            //if (relation.children_ > 0) {
            //
            //}
        }

        //puzzled now, only can connect and disconnect parent and children ?
        //template<typename Allocator>
        //void OnRelationShipUpdate(ComponentRepo<ECSRelationShipComponent, Allocator>* repo, Entity entt) {
        //
        //}

        template<typename Allocator>
        void OnReleationShipRelease(ComponentRepo<ECSRelationShipComponent, Allocator>* repo, Entity entt) {
            Entity    adopt_parent = Entity::Null;
            const auto& relation = repo->Element(repo->ToIndex(entt));//todo
            if (relation.parent_) {
                adopt_parent = relation.parent_;
                auto& parent_relation = repo->Element(repo->ToIndex(relation.parent_));
                auto query_entt = parent_relation.first_children_;
                for (auto n = 0; n < parent_relation.children_; ++n) {
                    auto& query_relation = repo->Element(repo->ToIndex(query_entt));
                    if (query_entt == entt) {
                        if (query_relation.prev_ != Entity::Null) {
                            repo->Element(repo->ToIndex(query_relation.prev_)).next_ = query_relation.next_;
                        }
                        if (query_relation.next_ != Entity::Null) {
                            repo->Element(repo->ToIndex(query_relation.next_)).prev_ = query_relation.prev_;
                        }
                        break;
                    }
                    query_entt = query_relation.next_;
                }
                parent_relation.children_--;
            }

            //set children parent to adopt parent
            if (relation.children_ > 0) {
                auto query_entt = relation.first_child_;
                for (auto n = 0; n < relation.children_; ++n) {
                    auto& query_relation = repo->Element(repo->ToIndex(query_entt));
                    query_relation.parent_ = adopt_parent;
                    query_entt = query_relation.next_;
                }
            }
        }

        struct ECSInputComponent
        {

        };

        /** oop object reference warper,
         * https://github.com/sebas77/Svelto.MiniExamples/blob/master/Example6-Unity%20Hy-
         * brid-OOP%20Abstraction/Assets/Code/WithOOPLayer/OOPLayer/Engines/SyncTransformEngine.cs
         * warp all object function in oop sync system; with an handlemanager to store all object
         * and like the unity hybridcompoent
         */
        template<typename ObjectClass>
        struct ECSyncObjectRefComponent
        {
            using Handle = ObjectClass::Handle;
            Handle  handle_;
        };


        //state/state machine related primitives

        
    }
}
