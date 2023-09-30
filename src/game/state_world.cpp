#include "game/entities.h"
#include "game/main.h"
#include "game/map.h"
#include "game/ship.h"



namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS()

        void Init() override
        {
            // Configure the audio.
            float audio_distance = screen_size.x * 3;
            Audio::ListenerPosition(fvec3(0, 0, -audio_distance));
            Audio::ListenerOrientation(fvec3(0,0,1), fvec3(0,-1,0));
            Audio::Source::DefaultRefDistance(audio_distance);

            // Entities.
            game = nullptr;

            game.create<DynamicSolidTree>();

            game.create<MapObject>(Program::ExeDir() + "assets/maps/1.json");
            game.create<Camera>().pos = game.get<MapObject>()->map.cells.size() * WorldGrid::tile_size / 2;

            auto &ship = game.create<ShipPartBlocks>();
            ship.map = Map<ShipGrid>(Program::ExeDir() + "assets/maps/test_ship.json");
            ship.pos = ivec2(64, 92);
            DecomposeToComponentsAndDelete(ship);
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            for (auto &e : game.get<AllTickable>())
                e.get<Tickable>().Tick();

            auto &ships = game.get<Game::Category<Ent::OrderedList, ShipPartBlocks>>();
            if (ships.has_elems())
            {
                auto parts = FindConnectedShipParts(&ships.begin()->get<ShipPartBlocks>());

                std::cout << CollideShipParts(parts, ivec2(0,0), game.get<MapObject>().get_opt(), game.get<DynamicSolidTree>().get_opt(), parts.LambdaNoSuchEntityHere());
                std::cout << CollideShipParts(parts, ivec2(0,1), game.get<MapObject>().get_opt(), game.get<DynamicSolidTree>().get_opt(), parts.LambdaNoSuchEntityHere());
                std::cout << '\n';

                MoveShipParts(parts, mouse.pos_delta());
            }
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(ivec2(), screen_size).center().color(fvec3(0.6f));

            for (auto &e : game.get<AllPreRenderable>())
                e.get<PreRenderable>().PreRender();
            for (auto &e : game.get<AllRenderable>())
                e.get<Renderable>().Render();

            r.Finish();
        }
    };
}
