#pragma once

#include "render/Camera.hpp"

namespace ForgottenEngine {

	class SceneCamera : public Camera {
	public:
		enum class ProjectionType { Perspective = 0,
			Orthographic = 1 };

	public:
		void set_perspective(float vertical_fov, float near_clip = 0.1f, float far_clip = 1000.0f);
		void set_orthographic(float size, float near_clip = -1.0f, float far_clip = 1.0f);
		void set_viewport_size(uint32_t width, uint32_t height);

		void set_deg_perspective_vertical_fov(const float deg_vertical_fov) { deg_perspective_fov = deg_vertical_fov; }
		void set_rad_perspective_vertical_fov(const float deg_vertical_fov) { deg_perspective_fov = glm::degrees(deg_vertical_fov); }
		float get_deg_perspective_vertical_fov() const { return deg_perspective_fov; }
		float get_rad_perspective_vertical_fov() const { return glm::radians(deg_perspective_fov); }
		void set_perspective_near_clip(const float near_clip) { perspective_near = near_clip; }
		float get_perspective_near_clip() const { return perspective_near; }
		void set_perspective_far_clip(const float far_clip) { perspective_far = far_clip; }
		float get_perspective_far_clip() const { return perspective_far; }

		void set_orthographic_size(const float size) { orthographic_size = size; }
		float get_orthographic_size() const { return orthographic_size; }
		void set_orthographic_near_clip(const float near_clip) { orthographic_near = near_clip; }
		float get_orthographic_near_clip() const { return orthographic_near; }
		void set_orthographic_far_clip(const float far_clip) { orthographic_far = far_clip; }
		float get_orthographic_far_clip() const { return orthographic_far; }

		void set_projection_type(ProjectionType type) { projection_type = type; }
		ProjectionType get_projection_type() const { return projection_type; }

	private:
		ProjectionType projection_type = ProjectionType::Perspective;

		float deg_perspective_fov = 45.0f;
		float perspective_near = 0.1f, perspective_far = 1000.0f;

		float orthographic_size = 10.0f;
		float orthographic_near = -1.0f, orthographic_far = 1.0f;
	};

} // namespace ForgottenEngine
