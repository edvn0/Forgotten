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
	const auto [w, h] = Application::the().get_window().get_size<float>();
	width = w;
	height = h;
	VertexBufferLayout vertex_layout = { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float3, "a_Color" },
		{ ShaderDataType::Float2, "a_Tangent" }, { ShaderDataType::Float, "a_TexIndex" }, { ShaderDataType::Float, "a_TilingFactor" } };

	{
		FramebufferSpecification geo_framebuffer_spec;
		geo_framebuffer_spec.attachments = { ImageFormat::RGBA32F, ImageFormat::RGBA16F, ImageFormat::RGBA };

		// Don't blend with luminance in the alpha channel.
		geo_framebuffer_spec.attachments.texture_attachments[1].blend = false;
		geo_framebuffer_spec.samples = 1;
		geo_framebuffer_spec.clear_colour = { 0.0f, 0.0f, 0.0f, 1.0f };
		geo_framebuffer_spec.debug_name = "Geometry";
		geo_framebuffer_spec.clear_depth_on_load = false;
		auto framebuffer = Framebuffer::create(geo_framebuffer_spec);

		RenderPassSpecification render_pass_spec;
		render_pass_spec.debug_name = geo_framebuffer_spec.debug_name;
		render_pass_spec.target_framebuffer = framebuffer;
		auto render_pass = RenderPass::create(render_pass_spec);

		PipelineSpecification pipeline_specification;
		pipeline_specification.debug_name = "Renderer2D_Render";
		pipeline_specification.shader = Renderer::get_shader_library()->get("Renderer2D_Render");
		pipeline_specification.depth_operator = DepthCompareOperator::Equal;
		pipeline_specification.depth_write = false;
		pipeline_specification.layout = vertex_layout;
		pipeline_specification.line_width = 1.0f;
		pipeline_specification.render_pass = render_pass;
		geometry_pipeline = Pipeline::create(pipeline_specification);

		swapchain_material = Material::create(pipeline_specification.shader);
	}

	Renderer::wait_and_render();

	command_buffer = RenderCommandBuffer::create_from_swapchain();
	projection_matrix = glm::ortho(0.0f, w, 0.0f, h);
}

void ForgottenLayer::on_detach() { }

void ForgottenLayer::on_update(const TimeStep& ts)
{
	const auto [w, h] = Application::the().get_window().get_size<float>();
	projection_matrix = glm::ortho(0.0f, (float)w, 0.0f, (float)h);

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

	user_camera.set_viewport_size(w, h);
	projection_matrix = glm::ortho(0.0f, (float)w, 0.0f, (float)h);

	if (this->width != w || this->height != h) {
		this->width = w;
		this->height = h;
		renderer->on_recreate_swapchain();

		// Retrieve new main command buffer
		command_buffer = RenderCommandBuffer::create_from_swapchain();
	}

	user_camera.on_update(ts);

	draw_debug_stats();
	// Render final image to swapchain
	Reference<Image2D> final_image = geometry_pipeline->get_specification().render_pass->get_specification().target_framebuffer->get_image(0);
	if (!final_image) {
		swapchain_material->set("u_Texture", final_image);

		command_buffer->begin();
		Renderer::begin_render_pass(command_buffer, geometry_pipeline->get_specification().render_pass, false);
		Renderer::submit_fullscreen_quad(command_buffer, geometry_pipeline, nullptr, swapchain_material);
		Renderer::end_render_pass(command_buffer);
		command_buffer->end();
	} else {
		// Clear render pass if no image is present
		command_buffer->begin();
		Renderer::begin_render_pass(command_buffer, geometry_pipeline->get_specification().render_pass, true);
		Renderer::end_render_pass(command_buffer);
		command_buffer->end();
	}
}

void ForgottenLayer::update_fps_stats()
{
	auto& app = Application::the();
	frames_per_second = 1.0f / static_cast<float>(app.get_frametime());
}

void ForgottenLayer::update_performance_timers()
{
	auto& app = Application::the();
	frame_time = static_cast<float>(app.get_frametime());
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
		float min_window_size_x = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		style.WindowMinSize.x = min_window_size_x;

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
				auto viewport_min_region = ImGui::GetWindowContentRegionMin();
				auto viewport_max_region = ImGui::GetWindowContentRegionMax();
				auto viewport_offset = ImGui::GetWindowPos();
				viewport_bounds[0] = { viewport_min_region.x + viewport_offset.x, viewport_min_region.y + viewport_offset.y };
				viewport_bounds[1] = { viewport_max_region.x + viewport_offset.x, viewport_max_region.y + viewport_offset.y };

				viewport_focused = ImGui::IsWindowFocused();
				viewport_hovered = ImGui::IsWindowHovered();

				const auto& imgui_layer = Application::the().get_imgui_layer();
				imgui_layer->block(!viewport_focused && !viewport_hovered);

				ImVec2 viewport_panel_size = ImGui::GetContentRegionAvail();
				viewport_size = { viewport_panel_size.x, viewport_panel_size.y };

				ImVec2 vp_size = ImVec2 { viewport_size.x, viewport_size.y };

				const auto& image = geometry_pipeline->get_specification().render_pass->get_specification().target_framebuffer->get_image(0);
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
	const auto& button_hovered = colors[ImGuiCol_ButtonHovered];
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(button_hovered.x, button_hovered.y, button_hovered.z, 0.5f));
	const auto& button_active = colors[ImGuiCol_ButtonActive];
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(button_active.x, button_active.y, button_active.z, 0.5f));

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
	renderer->set_target_render_pass(geometry_pipeline->get_specification().render_pass);
	renderer->begin_scene(projection_matrix, glm::mat4(1.0f));

	// Add font size to this after each line
	float y = 30.0f;

	auto fps = 1.0f / (float)Application::the().get_frametime();

	draw_string(fmt::format("{:.2f} ms ({}ms FPS)", frame_time, frames_per_second), { 30.0f, y });
	y += 50.0f;
	draw_string(fmt::format("{} fps", (uint32_t)fps), { 30.0f, y });

	renderer->end_scene();
}

void ForgottenLayer::draw_string(const std::string& string, const glm::vec2& position, const glm::vec4& color, float size)
{
	glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(size));
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), { position.x, position.y, -0.2f }) * scale;
	renderer->draw_string(string, Font::get_default_font(), transform, 1000.0f);
}
