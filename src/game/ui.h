#pragma once

#include "game/draw.h"
#include "game/entities.h"
#include "game/main.h"
#include "game/ship.h"

// Lets you extend and contract pistons with mouse.
struct PistonMouseController : MouseFocusTickable, GuiRenderable
{
    IMP_STANDALONE_COMPONENT(Game)

    Game::Id active_piston_id;
    // Those are needed to fade-out GUI.
    irect2 last_piston_rect;
    bool last_piston_is_vertical = false;

    float anim_timer = 0;

    bool MouseFocusTick() override;
    void GuiRender() const override;
};

struct ShipEditController : Tickable, MouseFocusTickable, GuiRenderable
{
    IMP_STANDALONE_COMPONENT(Game)

    static constexpr int
        editor_tile_size = 16,
        tile_selector_step = 26;


    bool shown = true;
    float shown_anim_timer = 0;

    ivec2 world_pos;

    ivec2 editor_tiles_screen_pos;
    ivec2 tile_selector_screen_pos;

    bool any_tile_hovered = false;
    ivec2 hovered_tile;

    int hovered_tile_selector = -1;
    ShipGrid::Tile selected_tile = ShipGrid::Tile::block;

    bool has_mouse_focus = false;
    Draw::Color rect_style = Draw::Color::selection;

    Array2D<ShipGrid::Tile, int> cells;

    std::vector<ShipGrid::Tile> GetAvailableTileTypes() const;

    void Tick() override;
    bool MouseFocusTick() override;
    void GuiRender() const override;
};
