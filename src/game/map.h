#pragma once

#include "game/entities.h"
#include "game/main.h"
#include "gameutils/tiled_map.h"
#include "utils/json.h"
#include "utils/multiarray.h"

namespace TileDrawMethods
{
    // Invisible.
    struct Invis {};
    // Just a single tile.
    struct SimpleTile {ivec2 tex;};

    // Not a method, used to describe a dual grid pass.
    template <typename Data>
    struct DualGridPass
    {
        phmap::flat_hash_set<typename Data::Tile> tiles;
        ivec2 tex;
        float alpha = 1;
    };

    // This is passed to `Map::Render()`.
    enum class RenderMode
    {
        pre,
        normal,
    };
}

template <typename Data>
class BasicGrid : public Data
{
    inline static const auto tile_info_array = []{
        auto ret = Data::GetRawTileInfoArray();

        for (std::underlying_type_t<typename Data::Tile> i = 0; i < std::to_underlying(Data::Tile::_count); i++)
        {
            if (ret[i].tile != typename Data::Tile(i))
                throw std::runtime_error(FMT("Bad tile info at #{}.", i));
        }
        return ret;
    }();

  public:
    BasicGrid() = delete;
    ~BasicGrid() = delete;

    using data_t = Data;

    [[nodiscard]] static const Data::TileInfo &GetTileInfo(Data::Tile tile)
    {
        if (tile < typename Data::Tile{} || tile >= Data::Tile::_count)
            throw std::runtime_error(FMT("Bad tile enum: {}", std::to_underlying(tile)));
        return tile_info_array[std::to_underlying(tile)];
    }
};

struct WorldGridData
{
    static constexpr int tile_size = 12;

    enum class Tile
    {
        air,
        wall,
        _count [[maybe_unused]]
    };

    struct TileInfo
    {
        using DrawMethod = std::variant<TileDrawMethods::Invis, TileDrawMethods::SimpleTile>;

        Tile tile = Tile::air;
        bool solid = false;
        DrawMethod draw;
    };

    static const Graphics::Region &GetImage() {return "tiles"_image;}
    static auto GetRawTileInfoArray() -> std::array<TileInfo, std::to_underlying(Tile::_count)>;
};
using WorldGrid = BasicGrid<WorldGridData>;

struct ShipGridData
{
    static constexpr int tile_size = 4;

    enum class Tile
    {
        air,
        block,
        piston_h, // Those are only used temporarily, until the ship is split into sections.
        piston_v, // Then pistons become entities.
        goal,
        _count [[maybe_unused]]
    };

    enum class PistonRelation
    {
        empty, // Empty space.
        solid_attachable, // A solid block.
        solid_non_attachable, // A solid block, but you can't attach a piston to it.
        piston, // A piston.
    };

    struct TileInfo
    {
        using DrawMethod = std::variant<TileDrawMethods::Invis, TileDrawMethods::SimpleTile>;

        Tile tile = Tile::air;
        bool solid = false;
        DrawMethod draw;

        PistonRelation piston{};
    };

    static const Graphics::Region &GetImage() {return "ship_tiles"_image;}
    static auto GetRawTileInfoArray() -> std::array<TileInfo, std::to_underlying(Tile::_count)>;

    inline static const TileDrawMethods::DualGridPass<ShipGridData> dual_grid_passes[] = {
        {.tiles = {Tile::block}, .tex = ivec2(0,0), .alpha = 1},
        {.tiles = {Tile::goal}, .tex = ivec2(0,4), .alpha = 1},
    };
    inline static const TileDrawMethods::DualGridPass<ShipGridData> dual_grid_pre_passes[] = {
        {.tiles = {Tile::block, Tile::goal}, .tex = ivec2(0,1), .alpha = 1},
    };
};
using ShipGrid = BasicGrid<ShipGridData>;


template <typename Grid>
class Map
{
  public:
    struct Cell
    {
        typename Grid::Tile tile{};

        unsigned char noise = 0;

        void RegenerateNoise()
        {
            noise = ra.i <= 255;
        }
    };

    Array2D<Cell, int> cells;
    Tiled::PointLayer points;

    Map() {}

    Map(Stream::Input source)
    {
        Json json(source.ReadToMemory().string(), 32);

        if (auto la = Tiled::FindLayerOpt(json.GetView(), "objects"))
            points = Tiled::LoadPointLayer(la);

        auto layer_mid = Tiled::LoadTileLayer(Tiled::FindLayer(json.GetView(), "mid"));

        cells.resize(ivec2(layer_mid.size()));

        for (ivec2 pos : vector_range(cells.size()))
        {
            int tile_index = layer_mid.safe_nonthrowing_at(pos);
            if (tile_index < 0 || tile_index >= std::to_underlying(Grid::Tile::_count))
                throw std::runtime_error(FMT("{}Bad tile {} at {}.", source.GetExceptionPrefix(), tile_index, pos));

            Cell &cell = cells.safe_nonthrowing_at(pos);

            cell.tile = typename Grid::Tile(tile_index);
            cell.RegenerateNoise();
        }
    }

    template <TileDrawMethods::RenderMode Mode>
    void Render(ivec2 camera_pos) const
    {
        if (!cells.bounds().has_area())
            return;

        const auto &image = Grid::GetImage();

        if constexpr (Mode == TileDrawMethods::RenderMode::pre ? requires{Grid::dual_grid_pre_passes;} : requires{Grid::dual_grid_passes;})
        {
            std::span<const TileDrawMethods::DualGridPass<typename Grid::data_t>> passes;
            if constexpr (Mode == TileDrawMethods::RenderMode::pre)
                passes = Grid::dual_grid_pre_passes;
            else
                passes = Grid::dual_grid_passes;

            ivec2 a = div_ex(camera_pos - screen_size / 2 - Grid::tile_size / 2, Grid::tile_size);
            ivec2 b = div_ex(camera_pos + screen_size / 2 + Grid::tile_size / 2, Grid::tile_size);

            clamp_var_min(a, -1);
            clamp_var_max(b, cells.size());

            for (const auto &pass : passes)
            {
                for (ivec2 tile_pos : a <= vector_range </*sic*/ b)
                {
                    ivec2 pixel_pos = tile_pos * Grid::tile_size + Grid::tile_size / 2 - camera_pos;

                    auto GetBit = [&](ivec2 offset) -> bool
                    {
                        ivec2 point = tile_pos + offset;
                        if (cells.bounds().contains(point))
                            return pass.tiles.contains(cells.safe_nonthrowing_at(point).tile);
                        else
                            return false;
                    };

                    int index = GetBit(ivec2(1,1)) | GetBit(ivec2(0,1)) << 1 | GetBit(ivec2(0,0)) << 2 | GetBit(ivec2(1,0)) << 3;
                    if (index == 0)
                        continue;

                    r.iquad(pixel_pos, image with(= (_.a + Grid::tile_size * (pass.tex + ivec2(index - 1, 0))).rect_size(Grid::tile_size))).alpha(pass.alpha);
                }
            }
        }

        if constexpr (Mode == TileDrawMethods::RenderMode::normal)
        {
            ivec2 a = div_ex(camera_pos - screen_size / 2, Grid::tile_size);
            ivec2 b = div_ex(camera_pos + screen_size / 2, Grid::tile_size);
            clamp_var_min(a, 0);
            clamp_var_max(b, cells.size() - 1);

            for (ivec2 tile_pos : a <= vector_range <= b)
            {
                const Cell &cell = cells.safe_nonthrowing_at(tile_pos);
                const typename Grid::TileInfo &info = Grid::GetTileInfo(cell.tile);
                if (std::holds_alternative<TileDrawMethods::Invis>(info.draw))
                    continue;

                ivec2 pixel_pos = tile_pos * Grid::tile_size - camera_pos;

                std::visit(Meta::overload{
                    [&](const TileDrawMethods::Invis &) {},
                    [&](const TileDrawMethods::SimpleTile &draw)
                    {
                        r.iquad(pixel_pos, image with(= _.a + (draw.tex * Grid::tile_size).rect_size(Grid::tile_size)));
                    },
                }, info.draw);
            }
        }
    }

    [[nodiscard]] bool CollidesWithPoint(ivec2 point) const
    {
        ivec2 tile_pos = div_ex(point, Grid::tile_size);
        if (!cells.bounds().contains(tile_pos))
            return false;

        return Grid::GetTileInfo(cells.safe_nonthrowing_at(tile_pos).tile).solid;
    }

    [[nodiscard]] bool CollidesWithBox(irect2 box) const
    {
        return Math::for_each_cuboid_point(box.a, box.b - 1, ivec2(Grid::tile_size), nullptr, [&](ivec2 point)
        {
            return CollidesWithPoint(point);
        });
    }

    template <typename OtherGrid>
    [[nodiscard]] bool CollidesWithMap(const Map<OtherGrid> &other, ivec2 self_relative_pos) const
    {
        if constexpr (Grid::tile_size > OtherGrid::tile_size)
        {
            return other.CollidesWithMap(*this, -self_relative_pos);
        }
        else
        {
            if (!(cells.bounds() * Grid::tile_size + self_relative_pos).touches(other.cells.bounds() * OtherGrid::tile_size))
                return false;

            ivec2 a = div_ex(-self_relative_pos, Grid::tile_size);
            ivec2 b = div_ex(-self_relative_pos + other.cells.size() * OtherGrid::tile_size - 1, Grid::tile_size);
            clamp_var_min(a, 0);
            clamp_var_max(b, cells.size() - 1);

            for (ivec2 tile_pos : a <= vector_range <= b)
            {
                if (!Grid::GetTileInfo(cells.safe_nonthrowing_at(tile_pos).tile).solid)
                    continue;

                for (ivec2 point : (tile_pos * Grid::tile_size + self_relative_pos).rect_size(Grid::tile_size - 1).to_contour())
                {
                    if (other.CollidesWithPoint(point))
                        return true;
                }
            }
            return false;
        }
    }
};

struct MapObject
{
    IMP_STANDALONE_COMPONENT(Game)

    ivec2 pos;
    Map<WorldGrid> map;

    MapObject() {}
    MapObject(Stream::Input input);

    void RenderMap() const
    {
        map.Render<TileDrawMethods::RenderMode::normal>(game.get<Camera>()->pos - pos);
    }
};
