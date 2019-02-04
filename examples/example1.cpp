#include "progression.hpp"
#include <iomanip>

using namespace Progression;

int main(int argc, char* argv[]) {
	PG::EngineInitialize(PG_ROOT_DIR "configs/default.toml");

    LOG("debug message");
    LOG_WARN("warn message p1", "p2", "p3");
    LOG_ERR("error message");

	auto scene = Scene::Load(PG_ROOT_DIR "resources/scenes/scene1.pgscn");
    if (!scene) {
        LOG_ERR("Failed to load scene:");
        exit(EXIT_FAILURE);
    }
	auto camera = scene->GetCamera();
	camera->AddComponent<UserCameraComponent>(new UserCameraComponent(camera, 3));

	Window::SetRelativeMouse(true);
	PG::Input::PollEvents();

	graphics::BindFrameBuffer(0);
	// Game loop
    while (!PG::EngineShutdown) {
        PG::Window::StartFrame();
        PG::Input::PollEvents();

        if (PG::Input::GetKeyDown(PG::PG_K_ESC))
            PG::EngineShutdown = true;

        scene->Update();
		RenderSystem::Render(scene);

        PG::Window::EndFrame();
    }

    delete scene;

    PG::EngineQuit();

    return 0;
}
