//
// Created by Edwin Carlsson on 2022-08-10.
//

#include "fg_pch.hpp"

#include "user/UserCamera.hpp"

#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "Input.hpp"

namespace ForgottenEngine {

	static inline void disable_mouse() { }

	static inline void enable_mouse() { }

	UserCamera::UserCamera(const float deg_fov, const float width, const float height, const float near_p, const float far_p)
		: Camera(deg_fov, width, height, near_p, far_p)
		, focal_point(0.0f)
		, vertical_fov(glm::radians(deg_fov))
		, near_clip(near_p)
		, far_clip(far_p)
	{
		init();
	}
	void UserCamera::init()
	{
		constexpr glm::vec3 pos = { -5, 5, 5 };
		distance = glm::distance(pos, focal_point);

		yaw = 3.0f * glm::pi<float>() / 4.0f;
		pitch = glm::pi<float>() / 4.0f;

		position = calculate_position();
		const glm::quat orientation = get_orientation();
		direction = glm::eulerAngles(orientation) * (180.0f / glm::pi<float>());
		view_matrix = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(orientation);
		view_matrix = glm::inverse(view_matrix);
	}

	void UserCamera::focus(const glm::vec3& focus_point)
	{
		focal_point = focus_point;
		camera_mode = CameraMode::FLYCAM;
		if (distance > min_focus_distance) {
			distance -= distance - min_focus_distance;
			position = focal_point - get_forward_direction() * distance;
		}
		position = focal_point - get_forward_direction() * distance;
		update_camera_view();
	}

	void UserCamera::on_update(const TimeStep& ts)
	{
		const glm::vec2& mouse = Input::mouse_position();
		const glm::vec2 delta = (mouse - initial_mouse_position) * 0.002f;

		if (!is_active()) {
			// if (!UI::IsInputEnabled())
			//	UI::SetInputEnabled(true);

			return;
		}

		if (Input::mouse(Mouse::Right) && !Input::key(Key::LeftAlt)) {
			camera_mode = CameraMode::FLYCAM;
			disable_mouse();
			const float yaw_sign = get_up_direction().y < 0 ? -1.0f : 1.0f;

			const float speed = get_camera_speed();

			if (Input::key(Key::Q))
				position_delta -= ts.get_milli_seconds() * speed * glm::vec3 { 0.f, yaw_sign, 0.f };
			if (Input::key(Key::E))
				position_delta += ts.get_milli_seconds() * speed * glm::vec3 { 0.f, yaw_sign, 0.f };
			if (Input::key(Key::S))
				position_delta -= ts.get_milli_seconds() * speed * direction;
			if (Input::key(Key::W))
				position_delta += ts.get_milli_seconds() * speed * direction;
			if (Input::key(Key::A))
				position_delta -= ts.get_milli_seconds() * speed * right_direction;
			if (Input::key(Key::D))
				position_delta += ts.get_milli_seconds() * speed * right_direction;

			static constexpr float max_rate { 0.12f };
			yaw_delta += glm::clamp(yaw_sign * delta.x * rotation_speed(), -max_rate, max_rate);
			pitch_delta += glm::clamp(delta.y * rotation_speed(), -max_rate, max_rate);

			right_direction = glm::cross(direction, glm::vec3 { 0.f, yaw_sign, 0.f });

			direction = glm::rotate(glm::normalize(glm::cross(
										glm::angleAxis(-pitch_delta, right_direction), glm::angleAxis(-yaw_delta, glm::vec3 { 0.f, yaw_sign, 0.f }))),
				direction);

			const float dist = glm::distance(focal_point, position);
			focal_point = position + get_forward_direction() * dist;
			distance = dist;
		} else if (Input::key(Key::LeftAlt)) {
			camera_mode = CameraMode::ARCBALL;

			if (Input::mouse(Mouse::Middle)) {
				disable_mouse();
				mouse_pan(delta);
			} else if (Input::mouse(Mouse::Left)) {
				disable_mouse();
				mouse_rotate(delta);
			} else if (Input::mouse(Mouse::Right)) {
				disable_mouse();
				mouse_zoom(delta.x + delta.y);
			} else
				enable_mouse();
		} else {
			enable_mouse();
		}

		initial_mouse_position = mouse;
		position += position_delta;
		yaw += yaw_delta;
		pitch += pitch_delta;

		if (camera_mode == CameraMode::ARCBALL)
			position = calculate_position();

		update_camera_view();
	}

	void UserCamera::on_event(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.dispatch_event<MouseScrolledEvent>([this](MouseScrolledEvent& e) { return on_mouse_scroll(e); });
	}

	glm::vec3 UserCamera::get_up_direction() const { return glm::rotate(get_orientation(), glm::vec3(0.0f, 1.0f, 0.0f)); }

	glm::vec3 UserCamera::get_right_direction() const { return glm::rotate(get_orientation(), glm::vec3(1.f, 0.f, 0.f)); }

	glm::vec3 UserCamera::get_forward_direction() const { return glm::rotate(get_orientation(), glm::vec3(0.0f, 0.0f, -1.0f)); }

	glm::vec3 UserCamera::calculate_position() const { return focal_point - get_forward_direction() * distance + position_delta; }

	glm::quat UserCamera::get_orientation() const { return glm::quat(glm::vec3(-pitch - pitch_delta, -yaw - yaw_delta, 0.0f)); }

	float UserCamera::get_camera_speed() const
	{
		float speed = normal_speed;
		if (Input::key(Key::LeftControl))
			speed /= 2 - glm::log(normal_speed);
		if (Input::key(Key::LeftShift))
			speed *= 2 - glm::log(normal_speed);

		return glm::clamp(speed, MINIMUM_SPEED, MAXIMUM_SPEED);
	}

	void UserCamera::update_camera_view()
	{
		const float yaw_sign = get_up_direction().y < 0 ? -1.0f : 1.0f;

		// Extra step to handle the problem when the camera direction is the same as the up vector
		const float cos_angle = glm::dot(get_forward_direction(), get_up_direction());
		if (cos_angle * yaw_sign > 0.99f)
			pitch_delta = 0.f;

		const glm::vec3 look_at = position + get_forward_direction();
		direction = glm::normalize(look_at - position);
		distance = glm::distance(position, focal_point);
		view_matrix = glm::lookAt(position, look_at, glm::vec3 { 0.f, yaw_sign, 0.f });

		// damping for smooth camera
		yaw_delta *= 0.6f;
		pitch_delta *= 0.6f;
		position_delta *= 0.8f;
	}

	bool UserCamera::on_mouse_scroll(MouseScrolledEvent& e)
	{
		if (Input::mouse(Key::Right)) {
			normal_speed += e.get_offset_y() * 0.3f * normal_speed;
			normal_speed = std::clamp(normal_speed, MINIMUM_SPEED, MAXIMUM_SPEED);
		} else {
			mouse_zoom(e.get_offset_y() * 0.1f);
			update_camera_view();
		}

		return true;
	}

	void UserCamera::mouse_pan(const glm::vec2& delta)
	{
		auto [xSpeed, ySpeed] = pan_speed();
		focal_point -= get_right_direction() * delta.x * xSpeed * distance;
		focal_point += get_up_direction() * delta.y * ySpeed * distance;
	}

	void UserCamera::mouse_rotate(const glm::vec2& delta)
	{
		const float yaw_sign = get_up_direction().y < 0.0f ? -1.0f : 1.0f;
		yaw_delta += yaw_sign * delta.x * rotation_speed();
		pitch_delta += delta.y * rotation_speed();
	}

	void UserCamera::mouse_zoom(float delta)
	{
		distance -= delta * zoom_speed();
		const glm::vec3 forward_dir = get_forward_direction();
		position = focal_point - forward_dir * distance;
		if (distance < 1.0f) {
			focal_point += forward_dir * distance;
			distance = 1.0f;
		}
		position_delta += delta * zoom_speed() * forward_dir;
	}

	std::pair<float, float> UserCamera::pan_speed() const
	{
		const float x = glm::min(float(vp_width) / 1000.0f, 2.4f);
		const float factor_x = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		const float y = glm::min(float(vp_height) / 1000.0f, 2.4f);
		const float factor_y = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		return { factor_x, factor_y };
	}

	float UserCamera::rotation_speed() const { return 0.3; }

	float UserCamera::zoom_speed() const
	{
		float dist = distance * 0.2f;
		dist = glm::max(dist, 0.0f);
		float speed = dist * dist;
		speed = glm::min(speed, 50.0f); // max speed = 50
		return speed;
	}
} // namespace ForgottenEngine