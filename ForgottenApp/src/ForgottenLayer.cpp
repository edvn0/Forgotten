#include "ForgottenLayer.hpp"

#include "fg.hpp"

// Note: Switch this to true to enable dockspace
static auto is_dockspace_open = true;

ForgottenLayer::ForgottenLayer()
	: Layer("Sandbox 2D")
{
}

void ForgottenLayer::on_attach()
{
	renderer = Reference<Renderer2D>::create();
	const auto [width, height] = Application::the().get_window().get_size<float>();

	FramebufferSpecification compFramebufferSpec;
	compFramebufferSpec.DebugName = "SceneComposite";
	compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
	compFramebufferSpec.SwapChainTarget = true;
	compFramebufferSpec.Attachments = { ImageFormat::RGBA };

	Reference<Framebuffer> framebuffer = Framebuffer::create(compFramebufferSpec);

	PipelineSpecification pipelineSpecification;
	pipelineSpecification.Layout
		= { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float2, "a_TexCoord" } };
	pipelineSpecification.BackfaceCulling = false;
	pipelineSpecification.Shader = Renderer::get_shader_library()->get("TexturePass");

	RenderPassSpecification renderPassSpec;
	renderPassSpec.TargetFramebuffer = framebuffer;
	renderPassSpec.DebugName = "SceneComposite";
	pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
	pipelineSpecification.DebugName = "SceneComposite";
	pipelineSpecification.DepthWrite = false;
	swapchain_pipeline = Pipeline::create(pipelineSpecification);
	swapchain_material = Material::create(pipelineSpecification.Shader);
	command_buffer = RenderCommandBuffer::create_from_swapchain();
	projection_matrix = glm::ortho(0.0f, width, 0.0f, height);
}

void ForgottenLayer::on_detach() { }

void ForgottenLayer::on_update(const TimeStep& ts)
{
	Renderer::wait_and_render();
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

	draw_debug_stats();

	if (m_Width != width || m_Height != height) {
		m_Width = width;
		m_Height = height;
		renderer->on_recreate_swapchain();

		// Retrieve new main command buffer
		command_buffer = RenderCommandBuffer::create_from_swapchain();
	}

	// Render final image to swapchain
	Reference<Image2D> final_image
		= renderer->get_target_render_pass()->get_specification().TargetFramebuffer->get_image();
	if (final_image) {
		swapchain_material->set("u_Texture", final_image);

		command_buffer->begin();
		Renderer::begin_render_pass(command_buffer, swapchain_pipeline->get_specification().RenderPass, false);
		Renderer::submit_fullscreen_quad(command_buffer, swapchain_pipeline, nullptr, swapchain_material);
		Renderer::end_render_pass(command_buffer);
		command_buffer->end();
	} else {
		// Clear render pass if no image is present
		command_buffer->begin();
		Renderer::begin_render_pass(command_buffer, swapchain_pipeline->get_specification().RenderPass, false);
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
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove;
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

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		{
			ImGui::Begin("Viewport");
			{
				auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
				auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
				auto viewportOffset = ImGui::GetWindowPos();
				viewport_bounds[0]
					= { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
				viewport_bounds[1]
					= { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

				viewport_focused = ImGui::IsWindowFocused();
				viewport_hovered = ImGui::IsWindowHovered();
				const auto& imgui_layer = dynamic_cast<ImGuiLayer*>(Application::the().get_imgui_layer().get());
				imgui_layer->should_block(!viewport_focused && !viewport_hovered);

				ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
				viewport_size = { viewport_panel_size.x, viewport_panel_size.y };

				// ImTextureID texture_id = 0;
				// ImGui::Image(
				//	texture_id, ImVec2{ viewport_size.x, viewport_size.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
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

	ImGui::Begin("##toolbar", nullptr,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	float size = ImGui::GetWindowHeight() - 4.0f;
	ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	ImGui::End();
}

void ForgottenLayer::on_overlay_render(){};

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
	renderer->begin_scene(projection_matrix, glm::mat4(1.0f));

	// Add font size to this after each line
	float y = 30.0f;

	auto fps = 1.0f / (float)Application::the().get_frametime();

	draw_string(fmt::format("{:.2f} ms ({}ms FPS)", frame_time, frames_per_second), { 30.0f, y });
	y += 50.0f;
	draw_string(fmt::format("{} fps", (uint32_t)fps), { 30.0f, y });
	y += 50.0f;
}

void ForgottenLayer::draw_string(
	const std::string& string, const glm::vec2& position, const glm::vec4& color, float size)
{
	glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(size));
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), { position.x, position.y, -0.2f }) * scale;
	renderer->draw_string(string, Font::get_default_font(), transform, 1000.0f);
}
