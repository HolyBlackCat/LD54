#include "main.h"

const ivec2 screen_size = ivec2(480, 270);
const std::string_view window_name = "LD54";

Interface::Window window(std::string(window_name), screen_size * 2, Interface::windowed, adjust_(Interface::WindowSettings{}, .min_size = screen_size));
static Graphics::DummyVertexArray dummy_vao = nullptr;

Audio::Context audio_context = nullptr;
Audio::SourceManager audio;

const Graphics::ShaderConfig shader_config = Graphics::ShaderConfig::Core();
Interface::ImGuiController gui_controller(Poly::derived<Interface::ImGuiController::GraphicsBackend_Modern>, adjust_(Interface::ImGuiController::Config{}, .shader_header = shader_config.common_header, .store_state_in_file = {}));

Graphics::FontFile Fonts::Files::main(Program::ExeDir() + "assets/Monocat_6x12.ttf", 12);
Graphics::Font Fonts::main;

GameUtils::AdaptiveViewport adaptive_viewport(shader_config, screen_size);
Render r = adjust_(Render(0x2000, shader_config), .SetMatrix(adaptive_viewport.GetDetails().MatrixCentered()));

Input::Mouse mouse;

Random::DefaultGenerator random_generator = Random::MakeGeneratorFromRandomDevice();
Random::DefaultInterfaces<Random::DefaultGenerator> ra(random_generator);

struct Application : Program::DefaultBasicState
{
    GameUtils::State::Manager<StateBase> state_manager;
    GameUtils::FpsCounter fps_counter;

    void Resize()
    {
        adaptive_viewport.Update();
        mouse.SetMatrix(adaptive_viewport.GetDetails().MouseMatrixCentered());
    }

    Metronome metronome = Metronome(60);

    Metronome *GetTickMetronome() override
    {
        return &metronome;
    }

    int GetFpsCap() override
    {
        return 60 * NeedFpsCap();
    }

    void EndFrame() override
    {
        fps_counter.Update();
        window.SetTitle(STR((window_name), " TPS:", (fps_counter.Tps()), " FPS:", (fps_counter.Fps()), " SOUNDS:", (audio.ActiveSources())));
    }

    void Tick() override
    {
        // window.ProcessEvents();
        window.ProcessEvents({gui_controller.EventHook()});

        if (window.ExitRequested())
            Program::Exit();
        if (window.Resized())
        {
            Resize();
            Graphics::Viewport(window.Size());
        }

        gui_controller.PreTick();
        state_manager.Tick();
        audio.Tick();

        Audio::CheckErrors();
    }

    void Render() override
    {
        gui_controller.PreRender();
        adaptive_viewport.BeginFrame();
        state_manager.Call(&StateBase::Render);
        adaptive_viewport.FinishFrame();
        gui_controller.PostRender();
        Graphics::CheckErrors();

        window.SwapBuffers();
    }


    void Init(int level_index)
    {
        // Initialize ImGui.
        ImGui::StyleColorsDark();

        { // Load images.
            Graphics::GlobalData::Load(Program::ExeDir() + "assets/images/");
            r.SetAtlas("");

            // Load the font atlas.
            struct FontLoader : Graphics::GlobalData::Generator
            {
                ivec2 Size() const override
                {
                    return ivec2(256);
                }

                void Generate(Graphics::Image &image, irect2 rect) override
                {
                    Unicode::CharSet glyph_ranges;
                    glyph_ranges.Add(Unicode::Ranges::Basic_Latin);

                    Graphics::MakeFontAtlas(image, rect, {
                        {Fonts::main, Fonts::Files::main, glyph_ranges, Graphics::FontFile::monochrome_with_hinting},
                    });
                }
            };
            (void)Graphics::GlobalData::Image<"font_atlas", FontLoader>();
        }

        // Load various small fonts
        auto monochrome_font_flags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;

        gui_controller.LoadFont(Program::ExeDir() + "assets/Monocat_6x12.ttf", 12.0f, adjust(ImFontConfig{}, .FontBuilderFlags = monochrome_font_flags));
        gui_controller.LoadDefaultFont();

        Graphics::Blending::Enable();
        Graphics::Blending::FuncNormalPre();

        Audio::GlobalData::Load(Audio::mono, Audio::wav, Program::ExeDir() + "assets/sounds/");

        state_manager.SetState(FMT("World{{cur_level_index={}}}", level_index));
    }
};

IMP_MAIN(argc, argv)
{
    int level_index = 1;
    if (argc > 2)
        throw std::runtime_error("Need at most two arguments.");
    if (argc == 2)
    {
        std::string_view prefix = "--level=";
        if (!std::string_view(argv[1]).starts_with(prefix))
            throw std::runtime_error("The argument must be `--mode=NUM`.");
        level_index = Refl::FromString<int>(argv[1] + prefix.size());
    }

    Application app;
    app.Init(level_index);
    app.Resize();
    app.RunMainLoop();
    return 0;
}
