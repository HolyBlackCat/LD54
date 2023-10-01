#include "game/draw.h"
#include "game/entities.h"
#include "game/goal_controller.h"
#include "game/main.h"
#include "game/map.h"
#include "game/ship.h"
#include "game/ui.h"


namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS()

        int cur_level_index = 0;

        void LoadLevel(int index)
        {
            cur_level_index = index;

            game = nullptr;

            game.create<DynamicSolidTree>();
            game.create<PistonMouseController>();
            game.create<GravityController>();
            game.create<GoalController>();

            game.create<MapObject>(FMT("{}assets/maps/{}.json", Program::ExeDir(), index));
            game.create<Camera>().pos = game.get<MapObject>()->map.cells.size() * WorldGrid::tile_size / 2;
        }

        void Init() override
        {
            // Configure the audio.
            float audio_distance = screen_size.x * 3;
            Audio::ListenerPosition(fvec3(0, 0, -audio_distance));
            Audio::ListenerOrientation(fvec3(0,0,1), fvec3(0,-1,0));
            Audio::Source::DefaultRefDistance(audio_distance);

            LoadLevel(1);
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            if (auto goal = game.get<GoalController>().get_opt(); goal && goal->ShouldSwitchToNextLevel())
                LoadLevel(cur_level_index + 1);

            for (auto &e : game.get<AllTickable>())
                e.get<Tickable>().Tick();

            // auto &ships = game.get<Game::Category<Ent::OrderedList, ShipPartBlocks>>();
            // if (ships.has_elems())
            // {
            //     auto parts = FindConnectedShipParts(&ships.begin()->get<ShipPartBlocks>());

            //     std::cout << CollideShipParts(parts, ivec2(0,0), game.get<MapObject>().get_opt(), game.get<DynamicSolidTree>().get_opt(), parts.LambdaNoSuchEntityHere());
            //     std::cout << CollideShipParts(parts, ivec2(0,1), game.get<MapObject>().get_opt(), game.get<DynamicSolidTree>().get_opt(), parts.LambdaNoSuchEntityHere());
            //     std::cout << '\n';

            //     MoveShipParts(parts, mouse.pos_delta());
            // }
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(ivec2(), screen_size).center().color(fvec3(0.6f));

            for (auto &e : game.get<Game::Category<Ent::OrderedList, MapObject>>())
                e.get<MapObject>().RenderMap();
            for (auto &e : game.get<AllPreRenderable>())
                e.get<PreRenderable>().PreRender();
            for (auto &e : game.get<AllRenderable>())
                e.get<Renderable>().Render();
            for (auto &e : game.get<AllGuiRenderable>())
                e.get<GuiRenderable>().GuiRender();
            for (auto &e : game.get<AllFadeRenderable>())
                e.get<FadeRenderable>().FadeRender();

            r.Finish();
        }
    };
}
