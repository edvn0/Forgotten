#include "ForgottenLayer.hpp"

#include "fg.hpp"
#include "imgui/CoreUserInterface.hpp"

// Note: Switch this to true to enable dockspace
static auto is_dockspace_open = true;

static bool should_show_debug_stats = true;

ForgottenLayer::ForgottenLayer()
	: Layer("Sandbox 2D")
	, user_camera(45.0f, 1280.0f, 720.0f, 0.1f, 1000.0f)
{
}

void ForgottenLayer::on_attach()
{
	renderer = Reference<Renderer2D>::create();
	const auto [width, height] = Application::the().get_window().get_size<float>();

	FramebufferSpecification compFramebufferSpec;
	compFramebufferSpec.debug_name = "SceneComposite";
	compFramebufferSpec.clear_colour = { 0.1f, 0.9f, 0.1f, 1.0f };
	compFramebufferSpec.swapchain_target = true;
	compFramebufferSpec.attachments = { ImageFormat::RGBA };

	Reference<Framebuffer> framebuffer = Framebuffer::create(compFramebufferSpec);

	PipelineSpecification pipeline_spec;
	pipeline_spec.layout = { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float2, "a_TexCoord" } };
	pipeline_spec.backface_culling = false;
	pipeline_spec.shader = Renderer::get_shader_library()->get("TexturePass");

	RenderPassSpecification renderPassSpec;
	renderPassSpec.target_framebuffer = framebuffer;
	renderPassSpec.debug_name = "SceneComposite";
	pipeline_spec.render_pass = RenderPass::create(renderPassSpec);
	pipeline_spec.debug_name = "SceneComposite";
	pipeline_spec.depth_write = false;
	swapchain_pipeline = Pipeline::create(pipeline_spec);

	swapchain_pipeline->get_specification().render_pass->get_specification().target_framebuffer->get_image(0)->invalidate();

	Renderer::wait_and_render();

	swapchain_material = Material::create(pipeline_spec.shader);
	command_buffer = RenderCommandBuffer::create_from_swapchain();
	projection_matrix = glm::ortho(0.0f, width, 0.0f, height);
}

void ForgottenLayer::on_detach() { }

void ForgottenLayer::on_update(const TimeStep& ts)
{
	const auto [width, height] = Application::the().get_window().get_size<float>();
	projection_matrix = glm::ortho(0.0f, (float)width, 0.0f, (float)height);

	update_fps_timer -= ts;
	if (update_fps_timer <= 0.0f) {
		update_fps_stats();
		update_fps_timer = 1.0f;
	}

	update_performance_timer -= ts;
	if (update_performance_timer <= 0.0f) {
		update_performance_timers();
		update_performance_timer = 0.2f;
	}

	user_camera.set_viewport_size(width, height);
	projection_matrix = glm::ortho(0.0f, (float)width, 0.0f, (float)height);

	if (width != width || height != height) {
		this->width = width;
		this->height = height;
		renderer->on_recreate_swapchain();

		// Retrieve new main command buffer
		command_buffer = RenderCommandBuffer::create_from_swapchain();
	}

	user_camera.on_update(ts);

	if (should_show_debug_stats)
		draw_debug_stats();

	// Render final image to swapchain
	Reference<Image2D> final_image = swapchain_pipeline->get_specification().render_pass->get_specification().target_framebuffer->get_image(0);
	if (final_image) {
		swapchain_material->set("u_Texture", final_image);

		command_buffer->begin();
		Renderer::begin_render_pass(command_buffer, swapchain_pipeline->get_specification().render_pass, false);
		Renderer::submit_fullscreen_quad(command_buffer, swapchain_pipeline, nullptr, swapchain_material);
		Renderer::end_render_pass(command_buffer);
		command_buffer->end();
	} else {
		// Clear render pass if no image is present
		command_buffer->begin();
		Renderer::begin_render_pass(command_buffer, swapchain_pipeline->get_specification().render_pass, false);
		Renderer::end_render_pass(command_buffer);
		command_buffer->end();
	}
}

void ForgottenLayer::update_fps_stats()
{
	auto& app = Application::the();
	frames_per_second = 1.0f / (float)app.get_frametime();
}

void ForgottenLayer::update_performance_timers()
{
	auto& app = Application::the();
	frame_time = (float)app.get_frametime();
}

void ForgottenLayer::on_ui_render(const TimeStep& ts)
{
	static bool opt_fullscreen_persistant = true;
	bool opt_fullscreen = opt_fullscreen_persistant;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

	// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
	// because it would be confusing to have two docking targets within each others.
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen) {
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}

	// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the
	// pass-thru hole, so we ask Begin() to not render a background.
	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;

	// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
	// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
	// all active windows docked into it will lose their parent and become undocked.
	// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
	// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", &is_dockspace_open, window_flags);
	ImGui::PopStyleVar();
	{
		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		style.WindowMinSize.x = minWinSizeX;

		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New", "Ctrl+N")) { }

				if (ImGui::MenuItem("Open...", "Ctrl+O")) { }

				if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) { }

				if (ImGui::MenuItem("Exit")) {
					Application::the().exit();
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::Begin("Stats");
		{
			ImGui::Text("Renderer Stats:");
			std::string name = "None";
			ImGui::Text("Hovered Entity: %s", name.c_str());
		}
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 0, 0 });
		{
			ImGui::Begin("Viewport");
			{
				auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				auto viewportOffset = ImGui::GetWindowPos();
				viewport_bounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
				viewport_bounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

				viewport_focused = ImGui::IsWindowFocused();
				viewport_hovered = ImGui::IsWindowHovered();

				const auto& imgui_layer = Application::the().get_imgui_layer();
				imgui_layer->block(!viewport_focused && !viewport_hovered);

				ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
				viewport_size = { viewport_panel_size.x, viewport_panel_size.y };

				ImVec2 vp_size = ImVec2 { viewport_size.x, viewport_size.y };

				const auto& image = swapchain_pipeline->get_specification().render_pass->get_specification().target_framebuffer->get_image(0);

				UI::image(image, vp_size, { 0, 1 }, { 1, 0 });
				ImGui::End();
			}
		}
		ImGui::PopStyleVar();

		ui_toolbar();
	}
	ImGui::End();
}

void ForgottenLayer::ui_toolbar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	auto& colors = ImGui::GetStyle().Colors;
	const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
	const auto& buttonActive = colors[ImGuiCol_ButtonActive];
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

	ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	float size = ImGui::GetWindowHeight() - 4.0f;
	ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	ImGui::End();
}

void ForgottenLayer::on_overlay_render() {};

void ForgottenLayer::on_event(Event& event)
{
	EventDispatcher dispatcher(event);

	dispatcher.dispatch_event<MouseButtonPressedEvent>([&](MouseButtonPressedEvent& e) { return false; });

	dispatcher.dispatch_event<KeyPressedEvent>([&](KeyPressedEvent& ev) {
		bool super = Input::key(Key::LeftSuper) || Input::key(Key::RightSuper);
		bool shift = Input::key(Key::LeftShift) || Input::key(Key::RightShift);

		switch (ev.get_key_code()) {
		case Key::X: {
			Application::the().exit();
		}
		case Key::H: {
			break;
		}
		case Key::N: {
			break;
		}
		case Key::O: {
			break;
		}
		case Key::S: {
			break;
		}
		case Key::D: {
			break;
		}
		case Key::Q: {
			break;
		}
		case Key::W: {
			break;
		}
		case Key::E: {
			break;
		}
		case Key::R: {
			break;
		}
		case Key::J: {
			break;
		}
		default:
			return false;
		}

		return false;
	});
}

void ForgottenLayer::draw_debug_stats()
{
	renderer->set_target_render_pass(swapchain_pipeline->get_specification().render_pass);
	renderer->begin_scene(projection_matrix, glm::mat4(1.0f));

	// Add font size to this after each line
	float y = 30.0f;

	auto fps = 1.0f / (float)Application::the().get_frametime();

	draw_string(fmt::format("{:.2f} ms ({}ms FPS)", frame_time, frames_per_second), { 30.0f, y });
	y += 50.0f;
	draw_string(fmt::format("{} fps", (uint32_t)fps), { 30.0f, y });
	y += 50.0f;

	renderer->end_scene();
}

void ForgottenLayer::draw_string(const std::string& string, const glm::vec2& position, const glm::vec4& color, float size)
{
	glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(size));
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), { position.x, position.y, -0.2f }) * scale;
	renderer->draw_string(string, Font::get_default_font(), transform, 1000.0f);
}
