#include "map.h"

#include "game/goal_controller.h"
#include "game/ship.h"
#include "game/ui.h"

auto WorldGridData::GetRawTileInfoArray() -> std::array<TileInfo, std::to_underlying(Tile::_count)>
{
    using namespace TileDrawMethods;

    std::array<TileInfo, std::to_underlying(Tile::_count)> ret = {{
        { .tile = Tile::air,  .solid = false, .draw = Invis{}, },
        { .tile = Tile::wall, .solid = true,  .draw = SimpleTile{ivec2(0,0)}, },
    }};

    return ret;
}

auto ShipGridData::GetRawTileInfoArray() -> std::array<TileInfo, std::to_underlying(Tile::_count)>
{
    using namespace TileDrawMethods;

    std::array<TileInfo, std::to_underlying(Tile::_count)> ret = {{
        { .tile = Tile::air,      .solid = false, .draw = Invis{},                .piston = PistonRelation::empty,                },
        { .tile = Tile::block,    .solid = true,  .draw = Invis{}/*dual grid*/,   .piston = PistonRelation::solid_attachable,     },
        { .tile = Tile::piston_h, .solid = true,  .draw = SimpleTile{ivec2(0,2)}, .piston = PistonRelation::piston,               },
        { .tile = Tile::piston_v, .solid = true,  .draw = SimpleTile{ivec2(0,3)}, .piston = PistonRelation::piston,               },
        { .tile = Tile::goal,     .solid = true,  .draw = Invis{}/*dual grid*/,   .piston = PistonRelation::solid_non_attachable, },
    }};

    return ret;
}

MapObject::MapObject(Stream::Input input)
    : map(std::move(input))
{
    map.points.ForEachPointWithNamePrefix("=", [](std::string_view suffix, fvec2 pos)
    {
        bool nograv = false;
        bool fixed = false;
        bool goal = false;
        phmap::flat_hash_map<std::string, std::function<void()>> funcs = {
            {"nograv", [&]{nograv = true;}},
            {"fixed", [&]{fixed = true;}},
            {"goal", [&]{goal = true;}},
        };

        auto sep = suffix.find_first_of(',');
        std::string_view name = suffix.substr(0, sep);

        while (sep != std::string_view::npos)
        {
            suffix = suffix.substr(sep + 1);
            sep = suffix.find_first_of(',');

            std::string_view part = suffix.substr(0, sep);
            auto it = funcs.find(part);
            if (it == funcs.end())
                throw std::runtime_error(FMT("Unknown object modifier: `{}`.", part));
            it->second();
        }

        auto &new_blocks = game.create<ShipPartBlocks>();
        new_blocks.map = Map<ShipGrid>(FMT("{}assets/objects/{}.json", Program::ExeDir(), name));
        new_blocks.pos = iround(pos);

        DecomposeToComponentsAndDelete(new_blocks, [&](ShipPartBlocks &blocks)
        {
            blocks.gravity.enabled = !nograv && !fixed;
            blocks.can_move = !fixed;

            if (goal)
                game.get<GoalController>()->goal_blocks.insert(dynamic_cast<Game::Entity &>(blocks).id());
        });
    });

    map.points.ForEachPointWithNamePrefix("editor", [](std::string_view suffix, fvec2 pos)
    {
        Stream::Input input = Stream::ReadOnlyData::mem_reference(suffix);
        ivec2 box_size;
        input.Discard(':');
        Refl::InterfaceFor(box_size.x).FromString(box_size.x, input, {}, Refl::initial_state);
        input.Discard(':');
        Refl::InterfaceFor(box_size.y).FromString(box_size.y, input, {}, Refl::initial_state);
        input.ExpectEnd();

        auto &con = game.create<ShipEditController>();
        con.world_pos = iround(pos);
        con.cells.resize(box_size);
    });
}
