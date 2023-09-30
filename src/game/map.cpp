#include "map.h"

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
        { .tile = Tile::air,      .solid = false, .draw = Invis{},                .piston = PistonRelation::empty,            },
        { .tile = Tile::block,    .solid = true,  .draw = Invis{}/*dual grid*/,   .piston = PistonRelation::solid_attachable, },
        { .tile = Tile::piston_h, .solid = true,  .draw = SimpleTile{ivec2(0,2)}, .piston = PistonRelation::piston,           },
        { .tile = Tile::piston_v, .solid = true,  .draw = SimpleTile{ivec2(0,3)}, .piston = PistonRelation::piston,           },
    }};

    return ret;
}
