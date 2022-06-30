#include "Application.hpp"
#include "Entrypoint.hpp"
#include "Renderer.hpp"

#include "Image.hpp"

class ExampleLayer : public Forgotten::Layer {
private:
	float vp_width{ 1600 };
	float vp_height{ 900 };
	Renderer renderer{};

public:
	void on_attach() override { }

	void on_ui_render() override
	{
		{
			ImGui::Begin("App Info");
			ImGui::TextColored({ 1, 1, 0, 1.0 }, "%f", Application::get_frame_time());
			ImGui::End();
		}
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Viewport");

			vp_width = ImGui::GetContentRegionAvail().x;
			vp_height = ImGui::GetContentRegionAvail().y;

			const auto& image = renderer.get_final_image();
			if (image) {
				ImGui::Image((ImU64)image->get_descriptor_set(),
					ImVec2{ static_cast<float>(image->get_width()), static_cast<float>(image->get_height()) },
					ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}

		on_render();
	}

	void on_render() override
	{
		renderer.on_resize(vp_width, vp_height);
		renderer.render();
	}
};

Forgotten::Application* Forgotten::create_application(int argc, char** argv)
{
	Forgotten::ApplicationProperties spec;
	spec.name = "Forgotten Example";

	auto* app = new Forgotten::Application(spec);
	app->push_layer<ExampleLayer>();
	app->set_menubar_callback([app]() {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit")) {
				app->close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}