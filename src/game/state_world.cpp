#include "game/draw.h"
#include "game/entities.h"
#include "game/goal_controller.h"
#include "game/main.h"
#include "game/map.h"
#include "game/ship.h"
#include "game/ui.h"
#include "utils/filesystem.h"


namespace States
{
    STRUCT( World EXTENDS StateBase )
    {
        MEMBERS(
            DECL(int INIT=0) cur_level_index
        )

        int max_level_index = 0;
        bool is_fullscreen = false;

        void ToggleFullscreen()
        {
            if (is_fullscreen)
                window.SetMode(Interface::windowed);
            else
                window.SetMode(Interface::borderless_fullscreen);

            is_fullscreen = !is_fullscreen;
        }

        static std::string LevelIndexToFilename(int index)
        {
            return FMT("{}assets/maps/{}.json", Program::ExeDir(), index);
        }

        void LoadLevel(int index)
        {
            cur_level_index = index;

            game = nullptr;

            game.create<DynamicSolidTree>();
            game.create<PistonMouseController>();
            game.create<GravityController>();
            game.create<GoalController>().level_name = index == 0 ? "" : FMT("{}/{}", index, max_level_index);

            game.create<MapObject>(LevelIndexToFilename(index));
            game.create<Camera>().pos = game.get<MapObject>()->map.cells.size() * WorldGrid::tile_size / 2;
        }

        void Init() override
        {
            if (IMP_PLATFORM_IS(prod))
                ToggleFullscreen();

            // Configure the audio.
            float audio_distance = screen_size.x * 3;
            Audio::ListenerPosition(fvec3(0, 0, -audio_distance));
            Audio::ListenerOrientation(fvec3(0,0,1), fvec3(0,-1,0));
            Audio::Source::DefaultRefDistance(audio_distance);
            Audio::Volume(2.5f);

            // Count the levels.
            while (true)
            {
                bool ok = true;
                (void)Filesystem::GetObjectInfo(LevelIndexToFilename(max_level_index + 1), &ok);
                if (!ok)
                    break;
                max_level_index++;
            }

            LoadLevel(cur_level_index);
        }

        void Tick(std::string &next_state) override
        {
            (void)next_state;

            // Toggle fullscreen.
            if ((Input::Button(Input::l_alt).down() || Input::Button(Input::r_alt).down()) && Input::Button(Input::enter).pressed())
                ToggleFullscreen();

            // Reload the level if needed (this should be first).
            if (auto editor = game.get<ShipEditorController>().get_opt(); editor && editor->want_level_reload)
            {
                auto editor_cells = std::move(game.get<ShipEditorController>()->cells);
                auto editor_selected_tile = game.get<ShipEditorController>()->selected_tile;
                LoadLevel(cur_level_index);
                game.get<ShipEditorController>()->initial_preview = false;
                game.get<ShipEditorController>()->shown = true;
                game.get<ShipEditorController>()->cells = std::move(editor_cells);
                game.get<ShipEditorController>()->selected_tile = editor_selected_tile;
                game.get<GoalController>()->initial_delay = 0;
                game.get<GoalController>()->fade_timer = 0;
                game.get<GravityController>()->enabled = false; // The editor will reenable this.
            }

            // Load the next level if we're finished.
            if (auto goal = game.get<GoalController>().get_opt(); goal && goal->ShouldReloadLevel())
            {
                int next_index = cur_level_index + !goal->level_failed;
                if (next_index > max_level_index)
                    Program::Exit(1);

                LoadLevel(next_index);
            }

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
