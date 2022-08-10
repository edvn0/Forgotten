//
// Created by Edwin Carlsson on 2022-08-10.
//

#pragma once

#pragma once

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>

namespace ForgottenEngine {

	class Camera {
	public:
		Camera() = default;
		Camera(const glm::mat4& projection, const glm::mat4& reveresed_projection)
			: projection_matrix(projection)
			, projection_matrix_reversed(reveresed_projection)
		{
		}

		Camera(const float deg_fov, const float width, const float height, const float near_p, const float far_p)
			: projection_matrix(glm::perspectiveFov(glm::radians(deg_fov), width, height, far_p, near_p))
			, projection_matrix_reversed(glm::perspectiveFov(glm::radians(deg_fov), width, height, near_p, far_p))
		{
		}

		virtual ~Camera() = default;

		[[nodiscard]] const glm::mat4& get_projection_matrix() const { return projection_matrix; }
		[[nodiscard]] const glm::mat4& get_reversed_projection_matrix() const { return projection_matrix_reversed; }

		void set_projection_matrix(const glm::mat4 projection, const glm::mat4 unReversedProjection)
		{
			projection_matrix = projection;
			projection_matrix_reversed = unReversedProjection;
		}

		void set_perspective_projection_matrix(const float radFov, const float width, const float height, const float nearP, const float farP)
		{
			projection_matrix = glm::perspectiveFov(radFov, width, height, farP, nearP);
			projection_matrix_reversed = glm::perspectiveFov(radFov, width, height, nearP, farP);
		}

		void set_orthographic_project_matrix(const float width, const float height, const float nearP, const float farP)
		{
			// TODO(Karim): Make sure this is correct.
			projection_matrix = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, farP, nearP);
			projection_matrix_reversed = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, nearP, farP);
		}

		[[nodiscard]] float get_expose() const { return exposure; }
		float& get_expose() { return exposure; }

	protected:
		float exposure = 0.8f;

	private:
		glm::mat4 projection_matrix = glm::mat4(1.0f);
		glm::mat4 projection_matrix_reversed = glm::mat4(1.0f);
	};

} // namespace ForgottenEngine
