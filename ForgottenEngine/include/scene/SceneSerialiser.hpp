#pragma once

#include "Reference.hpp"

#include <filesystem>

namespace YAML {
	class Emitter;
	class Node;
} // namespace YAML

namespace ForgottenEngine {

	class Scene;
	class Entity;

	class SceneSerializer {
	public:
		SceneSerializer(const Reference<Scene>& scene);

		void serialize(const std::filesystem::path& filepath);
		void serialize_runtime(const std::filesystem::path& filepath);

		bool deserialize(const std::filesystem::path& filepath);
		bool deserialize_runtime(const std::filesystem::path& filepath);

	public:
		static void serialize_entity(YAML::Emitter& out, Entity entity);
		static void deserialize_entities(YAML::Node& entitiesNode, Reference<Scene> scene);

	public:
		inline static std::string_view file_filter = "ForgottenScene (*.fgscene)\0*.fgscene\0";
		inline static std::string_view default_extension = ".fgscene";

	private:
		Reference<Scene> m_Scene;
	};

} // namespace ForgottenEngine
