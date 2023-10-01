#include "ui.h"

#include "game/draw.h"

bool PistonMouseController::MouseFocusTick()
{
    constexpr int extra_radius = 4;

    ivec2 mouse_pos = mouse.pos() + game.get<Camera>()->pos;
    irect2 mouse_rect = mouse_pos.tiny_rect().expand(extra_radius);

    int best_dist = extra_radius + 1;
    ShipPartPiston *best_piston = nullptr;

    auto &aabb_tree = game.get<DynamicSolidTree>()->aabb_tree;
    aabb_tree.CollideAabb(mouse_rect, [&](DynamicSolidTree::Tree::NodeIndex node_index)
    {
        if (auto piston = game.get(aabb_tree.GetNodeUserData(node_index)).get_opt<ShipPartPiston>())
        {
            int dist = piston->DistanceToPoint(mouse_pos);

            if (dist < best_dist)
            {
                best_piston = piston;
                best_dist = dist;
            }
        }
        return false;
    });

    int control = 0;
    if (active_piston_id.is_nonzero() || best_piston)
    {
        control = mouse.left.down() - mouse.right.down();
        // control = Input::Button(Input::d).down() - Input::Button(Input::a).down();
    }

    if (!control)
    {
        if (best_piston)
            active_piston_id = dynamic_cast<Game::Entity &>(*best_piston).id();
        else
            active_piston_id = {};
    }

    if (auto active_piston_entity = game.get_opt(active_piston_id))
    {
        auto &active_piston = active_piston_entity->get<ShipPartPiston>();

        if (control)
            active_piston.ExtendOrRetract(control > 0);

        last_piston_rect = active_piston.last_rect;
        last_piston_is_vertical = active_piston.is_vertical;
    }

    clamp_var(anim_timer += (active_piston_id.is_nonzero() ? 1 : -1) * 0.17f);

    return active_piston_id.is_nonzero();
}

void PistonMouseController::GuiRender() const
{
    float t = smoothstep(anim_timer);

    if (t > 0.001f)
    {
        irect2 rect = last_piston_rect - game.get<Camera>()->pos;
        rect = rect.expand(ivec2(1,2) with(; if (last_piston_is_vertical) std::swap(_.x, _.y);));

        Draw::StyledRect(rect, Draw::Color::selection, 0.7f);
    }
}


std::vector<ShipGrid::Tile> ShipEditorController::GetAvailableTileTypes() const
{
    std::vector<ShipGrid::Tile> ret = {ShipGrid::Tile::block};

    if (cells.size().x > 2)
        ret.push_back(ShipGrid::Tile::piston_h);
    if (cells.size().y > 2)
        ret.push_back(ShipGrid::Tile::piston_v);

    return ret;
}

void ShipEditorController::Tick()
{
    clamp_var(shown_anim_timer += (shown ? 1 : -1) * 0.05f);

    has_mouse_focus = shown;

    play_pause_hovered = false;

    if (shown)
    {
        any_tile_hovered = false;
        hovered_tile_selector = -1;

        editor_tiles_screen_pos = -cells.size() * editor_tile_size / 2;
        tile_selector_screen_pos = editor_tiles_screen_pos + ivec2(cells.size().x * editor_tile_size + 16, -5);
    }

    play_pause_button_pos = screen_size/2 - ivec2("play_pause"_image.size().y);
}

bool ShipEditorController::MouseFocusTick()
{
    if (shown)
    {
        has_mouse_focus = true;

        if (mouse.left.up())
            editing_initial_enabled = true;

        hovered_tile = div_ex(mouse.pos() - editor_tiles_screen_pos, editor_tile_size);
        any_tile_hovered = editing_initial_enabled && cells.bounds().contains(hovered_tile);

        // Hover tile selector.
        ivec2 cur = tile_selector_screen_pos;
        auto avail_tiles = GetAvailableTileTypes();
        for (int i = 0; i < (int)std::size(avail_tiles); i++)
        {
            if (editing_initial_enabled && cur.rect_size(editor_tile_size).contains(mouse.pos()))
            {
                hovered_tile_selector = i;

                if (mouse.left.pressed())
                    selected_tile = avail_tiles[i];
            }

            cur.y += tile_selector_step;
        }
        // Select tile with mouse wheel.
        if (int control = mouse.wheel_down.pressed() - mouse.wheel_up.pressed())
        {
            auto it = std::find(avail_tiles.begin(), avail_tiles.end(), selected_tile);
            if (it != avail_tiles.end())
            {
                int i = int(it - avail_tiles.begin());
                selected_tile = avail_tiles[clamp(i + control, 0, (int)std::size(avail_tiles) - 1)];
            }
        }

        // Hover tiles.
        rect_style = Draw::Color::selection;
        if (any_tile_hovered)
        {
            // Add/remove block.
            if (mouse.left.down())
            {
                cells.safe_nonthrowing_at(hovered_tile) = selected_tile;
            }
            else if (mouse.right.down())
            {
                // Remove block.
                rect_style = Draw::Color::danger;
                cells.safe_nonthrowing_at(hovered_tile) = ShipGrid::Tile::air;
            }
        }
    }

    // Space closes the editor. R opens the editor.
    // Both terminate the initial preview, as well as clicking anywhere.
    bool space_pressed = Input::Button(Input::space).pressed();
    bool reload_pressed = Input::Button(Input::r).pressed();

    { // The play/pause button.
        bool hotkey_pressed = initial_preview ? space_pressed || reload_pressed : shown ? space_pressed : reload_pressed;

        play_pause_hovered = initial_preview || play_pause_button_pos.rect_size("play_pause"_image.size().y).contains(mouse.pos());

        if ((play_pause_hovered && mouse.left.pressed()) || hotkey_pressed)
        {
            if (shown)
            {
                // Start level.
                auto &new_blocks = game.create<ShipPartBlocks>();
                new_blocks.pos = world_pos;
                new_blocks.map.cells.resize(cells.size());
                for (ivec2 pos : vector_range(cells.size()))
                {
                    auto &target_cell = new_blocks.map.cells.safe_nonthrowing_at(pos);
                    target_cell.tile = cells.safe_nonthrowing_at(pos);
                    target_cell.RegenerateNoise();
                }
                DecomposeToComponentsAndDelete(new_blocks);

                shown = false;
                game.get<GravityController>()->enabled = true;
            }
            else
            {
                // Reload the level.
                want_level_reload = true;
            }
        }
    }

    return shown || play_pause_hovered;
}

void ShipEditorController::PreRender() const
{
    r.iquad(world_pos - game.get<Camera>()->pos, cells.size() * ShipGrid::tile_size).color(fvec3(1)).alpha(0.2f);
}

void ShipEditorController::GuiRender() const
{
    if (shown_anim_timer > 0.001f)
    {
        float t = smoothstep(shown_anim_timer);

        auto DrawTile = [&](ivec2 pixel_pos, ShipGrid::Tile tile, float alpha = 1, fvec3 color = fvec3(), float mix = 1)
        {
            int index = -1;
            bool transpose = false;

            if (tile == ShipGrid::Tile::block)
                index = 0;
            else if (tile == ShipGrid::Tile::piston_v)
                index = 1;
            else if (tile == ShipGrid::Tile::piston_h)
                index = 1, transpose = true;

            if (index == -1)
                return;

            auto quad = r.iquad(pixel_pos, "editor_tiles"_image with(= (_.a + ivec2(index, 0) * editor_tile_size).rect_size(editor_tile_size)))\
                .alpha(t * alpha).color(color).mix(mix);
            if (transpose)
                quad.flip_x(true).pixel_center(fvec2(0)).matrix(ivec2(0,-1).to_rotation_matrix());
        };

        // Background.
        r.iquad(ivec2(), screen_size).center().color(fvec3(0)).alpha(0.75f * t);

        // Extra grid background.
        r.iquad(editor_tiles_screen_pos.rect_size(cells.size() * editor_tile_size).expand(9)).color(fvec3(0)).alpha(0.55f * t);

        // Grid outline.
        Draw::Rect(editor_tiles_screen_pos.rect_size(cells.size() * editor_tile_size).expand(4), fvec3(1), t);

        // Grid.
        fvec3 grid_color(1,1,1);
        float grid_alpha = 0.098f * t;
        for (int i = 1; i < cells.size().x; i++)
            r.iquad(editor_tiles_screen_pos with(.x += editor_tile_size * i - 1), ivec2(1, cells.size().y * editor_tile_size - 1)).color(grid_color).alpha(grid_alpha);
        for (int i = 1; i < cells.size().y; i++)
            r.iquad(editor_tiles_screen_pos with(.y += editor_tile_size * i - 1), ivec2(cells.size().x * editor_tile_size - 1, 1)).color(grid_color).alpha(grid_alpha);

        // Tiles.
        for (ivec2 pos : vector_range(cells.size()))
        {
            ShipGrid::Tile tile = cells.safe_nonthrowing_at(pos);

            // Check if this is an unfinished piston.
            bool ok = true;
            if (tile == ShipGrid::Tile::piston_h || tile == ShipGrid::Tile::piston_v)
            {
                for (bool backward : {false, true})
                {
                    ivec2 cur = pos;
                    while (true)
                    {
                        cur += ivec2::axis(tile == ShipGrid::Tile::piston_v, backward ? -1 : 1);
                        auto other_tile = cells.try_get(cur);
                        if (ShipGrid::GetTileInfo(other_tile).piston == ShipGrid::PistonRelation::solid_attachable)
                            break;
                        if (other_tile != tile)
                        {
                            ok = false;
                            break;
                        }
                    }
                }
            }

            // Draw the tile.
            ivec2 pixel_pos = editor_tiles_screen_pos + pos * editor_tile_size;
            if (ok)
                DrawTile(pixel_pos, tile);
            else
                DrawTile(pixel_pos, tile, 1, Draw::ColorFromEnum(Draw::Color::danger), 0.5f);

            // Draw the hint for an unfinished piston.
            if (tile == ShipGrid::Tile::air && hovered_tile != pos)
            {
                bool need_hint = false;
                for (int i = 0; i < 4; i++)
                {
                    if (cells.try_get(pos + ivec2::dir4(i)) == (i % 2 ? ShipGrid::Tile::piston_v : ShipGrid::Tile::piston_h))
                    {
                        need_hint = true;
                        break;
                    }
                }

                if (need_hint)
                    Draw::StyledRect((pixel_pos + editor_tile_size / 2).centered_rect_halfsize(4), Draw::Color::danger, 0.8f);
            }
        }

        // Hovered cell.
        if (any_tile_hovered && has_mouse_focus)
        {
            ivec2 pixel_pos = editor_tiles_screen_pos + hovered_tile * editor_tile_size;

            if (rect_style != Draw::Color::danger && cells.safe_nonthrowing_at(hovered_tile) != selected_tile)
            {
                // Extra background.
                r.iquad(pixel_pos.rect_size(editor_tile_size)).color(fvec3(0)).alpha(0.85f);

                // Tile ghost.
                DrawTile(pixel_pos, selected_tile, 0.5f, Draw::ColorFromEnum(rect_style), 0.5f);
            }

            // Rect.
            Draw::StyledRect(pixel_pos.rect_size(editor_tile_size), rect_style, t);
        }

        // Tile selector.
        ivec2 cur = tile_selector_screen_pos;
        auto avail_tiles = GetAvailableTileTypes();
        for (int i = 0; i < (int)std::size(avail_tiles); i++)
        {
            // Extra background.
            r.iquad(cur.rect_size(editor_tile_size).expand(4)).color(fvec3(0)).alpha(0.55f * t);

            // Selection.
            if (avail_tiles[i] == selected_tile)
                Draw::Rect(cur.rect_size(editor_tile_size).expand(3), fvec3(1), 7 * t);

            // Tile.
            DrawTile(cur, avail_tiles[i], 0.7f);

            // Cursor.
            if (i == hovered_tile_selector)
                Draw::StyledRect(cur.rect_size(editor_tile_size).expand(2), Draw::Color::selection);

            cur.y += tile_selector_step;
        }
    }

    // Play/pause button.
    if (!initial_preview)
    {
        int button_size = "play_pause"_image.size().y;
        auto quad = r.iquad(play_pause_button_pos, "play_pause"_image with(= _.a + ivec2(button_size * !shown, 0).rect_size(button_size)));
        if (play_pause_hovered && !mouse.left.down())
            quad.color(Draw::ColorFromEnum(Draw::Color::selection)).mix(0.77f);
    }
}
