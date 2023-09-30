#include "game/entities.h"
#include "game/main.h"
#include "game/map.h"

struct MapObject : Renderable
{
    IMP_STANDALONE_COMPONENT(Game)

    ivec2 pos;
    Map<WorldGrid> map;

    MapObject() {}
    MapObject(Stream::Input input) : map(std::move(input)) {}

    void Render() const override
    {
        // map.Render(game.get<Camera>()->pos - pos);
    }
};

struct ShipPart : Tickable, Renderable
{
    IMP_STANDALONE_COMPONENT(Game)

    ivec2 pos;
    Map<ShipGrid> map;

    // Split this into multiple entities, by continuous map regions.
    // This entity itself is destroyed.
    void DecomposeToComponentsAndDelete()
    {
        // Whether this tile is not empty and not special for splitting purposes.
        auto IsRegularTile = [&](ShipGrid::Tile tile)
        {
            return tile != ShipGrid::Tile::air;
        };

        Array2D<char/*bool*/, int> visited(map.cells.size());

        for (ivec2 tile_pos : vector_range(map.cells.size()))
        {
            if (IsRegularTile(map.cells.safe_nonthrowing_at(tile_pos).tile) && !visited.safe_nonthrowing_at(tile_pos))
            {
                auto &new_part = game.create<ShipPart>();

                ivec2 new_part_tile_offset = tile_pos;

                auto lambda = [&](auto &lambda, ivec2 abs_tile_pos) -> void
                {
                    if (!map.cells.bounds().contains(abs_tile_pos))
                        return;
                    if (!IsRegularTile(map.cells.safe_nonthrowing_at(abs_tile_pos).tile))
                        return;
                    if (auto &flag = visited.safe_nonthrowing_at(abs_tile_pos))
                        return;
                    else
                        flag = true;

                    ivec2 rel_tile_pos = abs_tile_pos - new_part_tile_offset;

                    if (!new_part.map.cells.bounds().contains(rel_tile_pos))
                    {
                        ivec2 delta = clamp_max(rel_tile_pos, 0);
                        new_part.map.cells.resize(new_part.map.cells.bounds().combine(rel_tile_pos).size(), -delta);
                        new_part_tile_offset += delta;
                        rel_tile_pos -= delta;
                    }

                    new_part.map.cells.safe_nonthrowing_at(rel_tile_pos) = map.cells.safe_nonthrowing_at(abs_tile_pos);

                    for (int i = 0; i < 4; i++)
                        lambda(lambda, abs_tile_pos + ivec2::dir4(i));
                };
                lambda(lambda, tile_pos);

                new_part.pos = pos + new_part_tile_offset * ShipGrid::tile_size;
            }
        }

        game.destroy(*this);
    }

    void Tick() override
    {
        // std::cout << (map.CollidesWithMap(game.get<MapObject>()->map, pos - game.get<MapObject>()->pos)) << '\n';
    }

    void Render() const override
    {
        // Bounding box:
        // r.iquad(pos - game.get<Camera>()->pos, map.cells.size() * ShipGrid::tile_size).color(fvec3(0)).alpha(0.1f);

        map.Render(game.get<Camera>()->pos - pos);
    }
};

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

            game.create<MapObject>(Program::ExeDir() + "assets/maps/1.json");
            game.create<Camera>().pos = game.get<MapObject>()->map.cells.size() * WorldGrid::tile_size / 2;

            auto &ship = game.create<ShipPart>();
            ship.map = Map<ShipGrid>(Program::ExeDir() + "assets/maps/test_ship.json");
            ship.DecomposeToComponentsAndDelete();
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            for (auto &e : game.get<AllTickable>())
                e.get<Tickable>().Tick();

            // static int i = 0;
            // auto &parts = game.get<Game::Category<Ent::OrderedList, ShipPart>>();
            // if (mouse.right.pressed())
            //     i = (i + 1) % parts.size();
            // std::next(parts.begin(), i)->get<ShipPart>().pos = mouse.pos() + game.get<Camera>()->pos;

            // std::cout << std::next(parts.begin(), i)->get<ShipPart>().map.cells.size() << '\n';
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(ivec2(), screen_size).center().color(fvec3(1));

            for (auto &e : game.get<AllRenderable>())
                e.get<Renderable>().Render();

            r.Finish();
        }
    };
}
