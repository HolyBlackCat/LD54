#include "game/draw.h"
#include "game/entities.h"
#include "game/goal_controller.h"
#include "game/main.h"
#include "game/map.h"
#include "game/ship.h"
#include "game/ui.h"


namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS()

        int cur_level_index = 0;

        void LoadLevel(int index)
        {
            cur_level_index = index;

            game = nullptr;

            game.create<DynamicSolidTree>();
            game.create<PistonMouseController>();
            game.create<GravityController>();
            game.create<GoalController>();

            game.create<MapObject>(FMT("{}assets/maps/{}.json", Program::ExeDir(), index));
            game.create<Camera>().pos = game.get<MapObject>()->map.cells.size() * WorldGrid::tile_size / 2;
        }

        void Init() override
        {
            // Configure the audio.
            float audio_distance = screen_size.x * 3;
            Audio::ListenerPosition(fvec3(0, 0, -audio_distance));
            Audio::ListenerOrientation(fvec3(0,0,1), fvec3(0,-1,0));
            Audio::Source::DefaultRefDistance(audio_distance);

            LoadLevel(1);
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            // Reload the level if needed (this should be first).
            if (auto editor = game.get<ShipEditorController>().get_opt(); editor && editor->want_level_reload)
            {
                auto editor_cells = std::move(game.get<ShipEditorController>()->cells);
                auto editor_selected_tile = game.get<ShipEditorController>()->selected_tile;
                LoadLevel(cur_level_index);
                game.get<ShipEditorController>()->initial_preview = false;
                game.get<ShipEditorController>()->shown = true;
                game.get<ShipEditorController>()->selected_tile = editor_selected_tile;
                game.get<ShipEditorController>()->selected_tile = editor_selected_tile;
                game.get<GoalController>()->initial_delay = 0;
                game.get<GoalController>()->fade_timer = 0;
                game.get<GravityController>()->enabled = false; // The editor will reenable this.
            }

            // Load the next level if we're finished.
            if (auto goal = game.get<GoalController>().get_opt(); goal && goal->ShouldReloadLevel())
                LoadLevel(cur_level_index + !goal->level_failed);

            // Tick.
            for (auto &e : game.get<AllTickable>())
                e.get<Tickable>().Tick();

            { // Mouse focus callbacks.
                auto &mouse_focus_tickable = game.get<AllMouseFocusTickable>();
                auto it = mouse_focus_tickable.end();
                while (it != mouse_focus_tickable.begin())
                {
                    --it;

                    if (it->get<MouseFocusTickable>().MouseFocusTick())
                        break;
                }
            }
        }

        void Render() const override
        {
            Graphics::SetClearColor(fvec3(0));
            Graphics::Clear();

            r.BindShader();

            // Background.
            r.iquad(ivec2(), "sky"_image).center();

            for (auto &e : game.get<Game::Category<Ent::OrderedList, MapObject>>())
                e.get<MapObject>().RenderMap();
            for (auto &e : game.get<AllPreRenderable>())
                e.get<PreRenderable>().PreRender();
            for (auto &e : game.get<AllRenderable>())
                e.get<Renderable>().Render();

            // Vignette.
            r.iquad(ivec2(), "vignette"_image).center().color(fvec3(0.2f, 0.45f, 0.8f)).mix(0).alpha(0.2f);

            for (auto &e : game.get<AllGuiRenderable>())
                e.get<GuiRenderable>().GuiRender();
            for (auto &e : game.get<AllFadeRenderable>())
                e.get<FadeRenderable>().FadeRender();

            r.Finish();
        }
    };
}
