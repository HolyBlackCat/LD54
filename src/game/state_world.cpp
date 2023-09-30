#include "game/draw.h"
#include "game/entities.h"
#include "game/main.h"
#include "game/map.h"
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

    void Tick() override
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
            control = mouse.left.down() - mouse.right.down();

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
                active_piston.ExtendOrRetract(control > 0, true);
        }

        clamp_var(anim_timer += (active_piston_id.is_nonzero() ? 1 : -1) * 0.17f);
    }

    void GuiRender() const override
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
};


namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS()

        void Init() override
        {
            // Configure the audio.
            float audio_distance = screen_size.x * 3;
            Audio::ListenerPosition(fvec3(0, 0, -audio_distance));
            Audio::ListenerOrientation(fvec3(0,0,1), fvec3(0,-1,0));
            Audio::Source::DefaultRefDistance(audio_distance);

            // Entities.
            game = nullptr;

            game.create<DynamicSolidTree>();
            game.create<PistonMouseController>();

            game.create<MapObject>(Program::ExeDir() + "assets/maps/1.json");
            game.create<Camera>().pos = game.get<MapObject>()->map.cells.size() * WorldGrid::tile_size / 2;

            auto &ship = game.create<ShipPartBlocks>();
            ship.map = Map<ShipGrid>(Program::ExeDir() + "assets/maps/test_ship.json");
            ship.pos = ivec2(64, 92);
            DecomposeToComponentsAndDelete(ship);
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            for (auto &e : game.get<AllTickable>())
                e.get<Tickable>().Tick();

            // auto &ships = game.get<Game::Category<Ent::OrderedList, ShipPartBlocks>>();
            // if (ships.has_elems())
            // {
            //     auto parts = FindConnectedShipParts(&ships.begin()->get<ShipPartBlocks>());

            //     std::cout << CollideShipParts(parts, ivec2(0,0), game.get<MapObject>().get_opt(), game.get<DynamicSolidTree>().get_opt(), parts.LambdaNoSuchEntityHere());
            //     std::cout << CollideShipParts(parts, ivec2(0,1), game.get<MapObject>().get_opt(), game.get<DynamicSolidTree>().get_opt(), parts.LambdaNoSuchEntityHere());
            //     std::cout << '\n';

            //     MoveShipParts(parts, mouse.pos_delta());
            // }
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            r.iquad(ivec2(), screen_size).center().color(fvec3(0.6f));

            for (auto &e : game.get<AllPreRenderable>())
                e.get<PreRenderable>().PreRender();
            for (auto &e : game.get<AllRenderable>())
                e.get<Renderable>().Render();
            for (auto &e : game.get<AllGuiRenderable>())
                e.get<GuiRenderable>().GuiRender();

            r.Finish();
        }
    };
}
