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

struct ShipEditorController :
    Tickable, MouseFocusTickable,
    GuiRenderable
{
    IMP_STANDALONE_COMPONENT(Game)

    static constexpr int
        editor_tile_size = 16,
        tile_selector_step = 26;

    bool shown = false;
    float shown_anim_timer = 0;

    // This becomes true when the mouse is first released.
    // Otherwise clicking to stop the starting preview places a block immediately.
    bool editing_initial_enabled = false;

    ivec2 world_pos;

    ivec2 editor_tiles_screen_pos;
    ivec2 tile_selector_screen_pos;
    ivec2 play_pause_button_pos;

    bool any_tile_hovered = false;
    ivec2 hovered_tile;

    int hovered_tile_selector = -1;
    ShipGrid::Tile selected_tile = ShipGrid::Tile::block;

    bool has_mouse_focus = false;
    Draw::Color rect_style = Draw::Color::selection;

    bool play_pause_hovered = false;

    // If true, we're doing the initial preview.
    bool initial_preview = true;

    // The game state watches this, and reloads the level when this becomes true.
    bool want_level_reload = false;

    Array2D<ShipGrid::Tile, int> cells;

    int world_box_fade_out_timer = 0;

    bool tutorial_mode = false;
    bool tutorial_erased_at_least_once = false;
    int tutorial_tooltip_fade_out_timer = 0;

    std::vector<ShipGrid::Tile> GetAvailableTileTypes() const;

    void Tick() override;
    bool MouseFocusTick() override;
    void GuiRender() const override;
};

struct Tooltip :
    Tickable,
    GuiRenderable
{
    IMP_STANDALONE_COMPONENT(Game)

    ENUM( Kind ENUM_CLASS AT_CLASS_SCOPE,
        (left_click)
        (right_click)
    )

    ivec2 pos;

    int timer = 0;
    int timer2 = 0;

    bool extended_once = false;
    bool contracted_once = false;

    Kind kind = Kind::left_click;

    void Tick() override;
    void GuiRender() const override;
};
