#pragma once

#include "Common.hpp"
#include "serialize/StreamReader.hpp"
#include "serialize/StreamWriter.hpp"

#include <string>
#include <utility>
#include <vector>

namespace ForgottenEngine {

	enum class ShaderDomain {
		None = 0,
		Vertex = 0,
		Pixel = 1 // unused
	};

	class ShaderResourceDeclaration {
	public:
		ShaderResourceDeclaration() = default;
		ShaderResourceDeclaration(std::string name, uint32_t resource_register, uint32_t count)
			: name(std::move(name))
			, resource_register(resource_register)
			, count(count)
		{
		}

		const std::string& get_name() const { return name; }
		uint32_t get_register() const { return resource_register; }
		uint32_t get_count() const { return count; }

		static void serialize(StreamWriter* serializer, const ShaderResourceDeclaration& instance)
		{
			serializer->write_string(instance.name);
			serializer->write_raw(instance.resource_register);
			serializer->write_raw(instance.count);
		}

		static void deserialize(StreamReader* deserializer, ShaderResourceDeclaration& instance)
		{
			deserializer->read_string(instance.name);
			deserializer->read_raw(instance.resource_register);
			deserializer->read_raw(instance.count);
		}

	private:
		std::string name;
		uint32_t resource_register = 0;
		uint32_t count = 0;
	};

	typedef std::vector<ShaderResourceDeclaration*> ShaderResourceList;

} // namespace ForgottenEngine
