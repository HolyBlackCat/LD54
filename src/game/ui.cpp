#include "ui.h"

#include "game/draw.h"

void PistonMouseController::Tick()
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

        last_piston_rect = active_piston.last_rect;
        last_piston_is_vertical = active_piston.is_vertical;

        if (control)
            active_piston.ExtendOrRetract(control > 0);
    }

    clamp_var(anim_timer += (active_piston_id.is_nonzero() ? 1 : -1) * 0.17f);
}

void PistonMouseController::GuiRender() const
{
    float t = smoothstep(anim_timer);

    if (t > 0.001f)
    {
        irect2 rect = last_piston_rect - game.get<Camera>()->pos;
        rect = rect.expand(ivec2(2,3) with(; if (last_piston_is_vertical) std::swap(_.x, _.y);));

        Draw::Rect(rect, fvec3(0.7f,1,0), 0.7f * t, 0.55f);
        Draw::Rect(rect.expand(1), fvec3(0,0,0), 0.4f * t);
    }
}
