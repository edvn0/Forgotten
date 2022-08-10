//
// Created by Edwin Carlsson on 2022-08-10.
//

#pragma once

#include "TimeStep.hpp"
#include "events/KeyEvent.hpp"
#include "events/MouseEvent.hpp"
#include "render/Camera.hpp"

#include <concepts>
#include <glm/detail/type_quat.hpp>

namespace ForgottenEngine {

	constexpr static float MINIMUM_SPEED { 0.0005f }, MAXIMUM_SPEED { 2.0f };

	template <typename T>
	concept Number = std::integral<T> or std::floating_point<T>;

	enum class CameraMode {
		NONE,
		FLYCAM,
		ARCBALL
	};

	class UserCamera : public Camera {
	public:
		UserCamera(float deg_fov, float width, float height, float near_p, float far_p);
		void init();

		void focus(const glm::vec3& focusPoint);
		void on_update(const TimeStep& ts);
		void on_event(Event& e);

		[[nodiscard]] bool is_active() const { return active; }
		void set_active(bool is_active) { active = is_active; }

		[[nodiscard]] CameraMode get_current_mode() const { return camera_mode; }

		[[nodiscard]] inline float get_distance() const { return distance; }
		inline void set_distance(float in_distance) { distance = in_distance; }

		[[nodiscard]] const glm::vec3& get_focal_point() const { return focal_point; }

		template <Number T>
		inline void set_viewport_size(T width, T height)
		{
			auto in_w = static_cast<uint32_t>(width);
			auto in_h = static_cast<uint32_t>(height);

			if (vp_width == in_w && vp_height == in_h)
				return;

			set_perspective_projection_matrix(vertical_fov, (float)in_w, (float)in_h, near_clip, far_clip);

			vp_width = in_w;
			vp_height = in_h;
		}

		[[nodiscard]] const glm::mat4& get_view_matrix() const { return view_matrix; }
		[[nodiscard]] glm::mat4 get_view_projection() const { return get_projection_matrix() * view_matrix; }
		[[nodiscard]] glm::mat4 get_reversed_view_projection() const { return get_reversed_projection_matrix() * view_matrix; }

		[[nodiscard]] glm::vec3 get_up_direction() const;
		[[nodiscard]] glm::vec3 get_right_direction() const;
		[[nodiscard]] glm::vec3 get_forward_direction() const;

		[[nodiscard]] const glm::vec3& get_position() const { return position; }

		[[nodiscard]] glm::quat get_orientation() const;

		[[nodiscard]] float get_vertical_fov() const { return vertical_fov; }
		[[nodiscard]] float get_aspect_ratio() const { return aspect_ratio; }
		[[nodiscard]] float get_neat_clip() const { return near_clip; }
		[[nodiscard]] float get_far_clip() const { return far_clip; }
		[[nodiscard]] float get_pitch() const { return pitch; }
		[[nodiscard]] float get_yaw() const { return yaw; }
		[[nodiscard]] float get_camera_speed() const;

	private:
		void update_camera_view();

		bool on_mouse_scroll(MouseScrolledEvent& e);

		void mouse_pan(const glm::vec2& delta);
		void mouse_rotate(const glm::vec2& delta);
		void mouse_zoom(float delta);

		[[nodiscard]] glm::vec3 calculate_position() const;

		[[nodiscard]] std::pair<float, float> pan_speed() const;

		[[nodiscard]] float rotation_speed() const;
		[[nodiscard]] float zoom_speed() const;

	private:
		glm::mat4 view_matrix {};
		glm::vec3 position {}, direction {}, focal_point {};

		// Perspective projection params
		float vertical_fov {}, aspect_ratio {}, near_clip {}, far_clip {};

		bool active = false;
		bool is_panning {}, is_rotating {};
		glm::vec2 initial_mouse_position {};
		glm::vec3 initial_focal_point {}, initial_rotation {};

		float distance {};
		float normal_speed { 0.002f };

		float pitch {}, yaw {};
		float pitch_delta { 0.0f }, yaw_delta { 0.0f };
		glm::vec3 position_delta {};
		glm::vec3 right_direction {};

		CameraMode camera_mode { CameraMode::ARCBALL };

		float min_focus_distance { 100.0f };

		uint32_t vp_width { 1280 }, vp_height { 720 };
		friend class EditorLayer;
	};

} // namespace ForgottenEngine
