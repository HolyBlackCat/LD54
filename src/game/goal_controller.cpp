#include "goal_controller.h"

#include "game/ship.h"

bool GoalController::ShouldReloadLevel() const
{
    return fading_out && fade_timer > 0.999f;
}

void GoalController::Tick()
{
    auto tree = game.get<DynamicSolidTree>().get_opt();

    auto AllBlocksInserted = [&](const phmap::flat_hash_set<Game::Id> &set)
    {
        bool ok = !set.empty();
        for (Game::Id id : set)
        {
            ConnectedShipParts parts;
            parts.AddSingleBlocksObject(id);

            int num_hits = 0;

            for (int i = 0; i < 4; i++)
            {
                bool hit = CollideShipParts(parts, ivec2::dir4(i), nullptr, tree, [&](const Game::Entity &e)
                {
                    return e.id() != id && set.contains(e.id());
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
        return ok;
    };

    // Check win condition.
    if (!fading_out && AllBlocksInserted(goal_blocks))
    {
        fading_out = true;
        audio.Play("level_transition"_sound, 1, ra.f.abs() <= 0.2f);
    }

    // Check gravity condition.
    game.get<GravityController>()->emerald_enabled = grav_blocks.empty() || AllBlocksInserted(grav_blocks);

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
    // Level name.
    r.itext(ivec2(0, -screen_size.y/2 + 15), Graphics::Text(Fonts::main, level_name)).color(fvec3(0.2f, 0.45f, 0.8f)).alpha(0.75f);

    if (level_name.empty())
        r.itext(ivec2(-screen_size.x/2+1, screen_size.y/2), Graphics::Text(Fonts::main, "by HolyBlackCat, for LD54 'limited space'")).color(fvec3(0.2f, 0.45f, 0.8f)).alpha(0.25f).align(ivec2(-1,1));
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
