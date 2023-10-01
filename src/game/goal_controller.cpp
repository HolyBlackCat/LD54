#include "goal_controller.h"

#include "game/ship.h"

bool GoalController::ShouldReloadLevel() const
{
    return fading_out && fade_timer > 0.999f;
}

void GoalController::Tick()
{
    // Check condition.
    if (!fading_out)
    {
        auto map = game.get<MapObject>().get_opt();
        auto tree = game.get<DynamicSolidTree>().get_opt();

        bool ok = !goal_blocks.empty();
        for (Game::Id id : goal_blocks)
        {
            ConnectedShipParts parts;
            parts.AddSingleBlocksObject(id);

            int num_hits = 0;

            for (int i = 0; i < 4; i++)
            {
                bool hit = CollideShipParts(parts, ivec2::dir4(i), map, tree, [&](const Game::Entity &e)
                {
                    return e.id() != id && goal_blocks.contains(e.id());
                });

                if (hit)
                    num_hits++;
            }

            if (num_hits < 3)
            {
                ok = false;
                break;
            }
        }

        if (ok)
            fading_out = true;
    }

    // Check goal bricks falling out.
    if (!fading_out && !level_failed)
    {
        for (auto id : goal_blocks)
        {
            int y = game.get(id).get<ShipPartBlocks>().pos.y - game.get<Camera>()->pos.y;
            if (y > screen_size.y/2 + 2)
            {
                level_failed = true;
                fading_out = true;
            }
        }
    }

    // Update timers.
    if (initial_delay > 0)
        initial_delay--;
    if (fading_out || initial_delay == 0)
        clamp_var(fade_timer += (fading_out ? 1 : -1) * 0.02f);
}

void GoalController::GuiRender() const
{
    r.itext(ivec2(0, -screen_size.y/2 + 15), Graphics::Text(Fonts::main, level_name)).color(fvec3(0.2f, 0.45f, 0.8f)).alpha(0.75f);
}

void GoalController::FadeRender() const
{
    if (fade_timer > 0.001f)
    {
        float t = smoothstep(smoothstep(fade_timer / 2)) * 2;

        ivec2 rect_size = screen_size with(.y /= 2);

        int offset = iround(rect_size.y * t);

        r.iquad(ivec2(-screen_size.x/2, -screen_size.y) + ivec2(0, offset), rect_size).color(fvec3(0)).alpha(fade_timer);
        r.iquad(ivec2(-screen_size.x/2, screen_size.y/2 - offset), rect_size).color(fvec3(0)).alpha(fade_timer);
    }
}
