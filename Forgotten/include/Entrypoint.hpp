#pragma once

extern Forgotten::Application* Forgotten::create_application(int argc, char** argv);
bool is_application_running = true;

namespace Forgotten {
int forgotten_main(int argc, char** argv)
{
	while (is_application_running) {
		Forgotten::Application* app = Forgotten::create_application(argc, argv);
		app->construct_and_run();
		delete app;
	}

	return 0;
}

} // namespace Forgotten

int main(int argc, char** argv) { return Forgotten::forgotten_main(argc, argv); }
