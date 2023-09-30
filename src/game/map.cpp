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
        { .tile = Tile::air,   .solid = false, .draw = Invis{}, },
        { .tile = Tile::block, .solid = true,  .draw = Invis{}, }, // Dual grid is used for this.
    }};

    return ret;
}
