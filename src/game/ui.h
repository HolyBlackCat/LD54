#pragma once

#include "game/entities.h"
#include "game/main.h"
#include "game/ship.h"

// Lets you extend and contract pistons with mouse.
struct PistonMouseController : Tickable, GuiRenderable
{
    IMP_STANDALONE_COMPONENT(Game)

    Game::Id active_piston_id;
    // Those are needed to fade-out GUI.
    irect2 last_piston_rect;
    bool last_piston_is_vertical = false;

    float anim_timer = 0;

    void Tick() override;
    void GuiRender() const override;
};
