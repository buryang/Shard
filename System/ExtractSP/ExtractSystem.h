#pragma once

#include "Utils/CommonUtils.h"
#include "Utils/SimpleJobSystem.h"
#include "Utils/SimpleCoro.h"
#include "Utils/SimpleEntitySystem.h"
#include "Utils/Algorithm.h"
#include "../Renderer/RenderCommon.h"

/*
* a middle transfer system to extract renderable data from ecs to renderer world
* as discribed in slide "Destiny¡¯s Multi-threaded Renderer Architecture", and also
* the bevy engine wiki https://bevy-cheatbook.github.io/gpu/intro.html
*/

//fixme i decide to reuse enity in simulation world for render world, so the extract system just transfer data
//but bevy use seperate entity id for render world, so the extract system need to map entity id from sim world to render world

namespace Shard::ExtractSP
{
    using Utils::TRingBuffer;
    using Utils::Coro;

    struct ExtractOp
    {
    };

    template<typename, typename>
    struct ExtractOpTraits final : ExtractOp;

    template<class ...Input, class ...Target>
    struct ExtractOpTraits<Utils::Tuple<Input...>, Utils::Tuple<Target...>> final : ExtractOp
    {
        using InputType = Utils::Tuple<Input...>;
        using OutputType = Utils::Tuple<Target...>;

        eastl::function<void(InputType& input, OutputType& output)> extract_;
    };


    //entity sync, todo add this to ecs resource
    enum EEntityEventType
    {
        eSpawned,
        eDespawned,
        /*when a component is removed, notify the render world to re-spawn entity*/
        eComponentRemoved,
    };

    struct EntityEvent
    {
        EEntityEventType event_;
        Utils::Entity entity_;
    };

    //singleton component for entity pending
    struct ECSPendingEntityEvent
    {
        Vector<ECSPendingEntityEvent, xx> pending_events_;
    };

    /* sync point for app and renderer system; extracting work must be wide
     * and lightweight, should be fast; for entity add/remove is less, so 
     * use a queue to sync it; while for component change, use change detection
     * and extract
     */
    class ExtractRenderRelevantSystem : public Utils::ECSSystem
    {
        //todo
    public:
        void Init() override;
        void UnInit() override;
        void Update(Utils::ECSSystemUpdateContext& ctx) override;
        bool IsPhaseUpdateBefore(const ECSSystem& other, Utils::EUpdatePhase phase)const override;
        bool IsPhaseUpdateEnabled(Utils::EUpdatePhase phase)const override;
        //~RenderAbleExtractSystem();
    protected:
        Coro<void> SyncEntity(); //sync entities added/removed liftecycle + indentity mapping
        //functions to sync components and resources
        Coro<void> EnumerateViewsToRender();
        Coro<void> EnumerateLightsToRender();
        /*predictive-visibility test in destiny's slide, just start as view changed, concurrent with simulation*/
        Coro<void> EnumerateStaticPrimtivesToRender();
        /*visibility test for dynamic primitves, after all simulation end*/
        Coro<void> EnumerateDynamicPrimitivesForViews();
        Coro<void> PopulateViewRenderPrimitives();
        void BeginExtractJobs();
        void FinalizeExtractJobs();
        Coro<void> ExtractPerView();
    protected:
        //scene update data buffer ping-pong, app & render sync point
        
    };
}
