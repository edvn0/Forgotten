#include "fg_pch.hpp"

#include "render/ShaderPack.hpp"

#include "serialize/FileStream.hpp"
#include "vulkan/VulkanShader.hpp"

namespace ForgottenEngine {

	namespace Utils {

		enum class ShaderStage : uint8_t { None = 0,
			Vertex = 1,
			Fragment = 2,
			Compute = 3 };

		VkShaderStageFlagBits ShaderStageToVkShaderStage(ShaderStage stage)
		{
			switch (stage) {
			case ShaderStage::Vertex:
				return VK_SHADER_STAGE_VERTEX_BIT;
			case ShaderStage::Fragment:
				return VK_SHADER_STAGE_FRAGMENT_BIT;
			case ShaderStage::Compute:
				return VK_SHADER_STAGE_COMPUTE_BIT;
			default:
				CORE_ERROR("Unexisting type");
			}
			return (VkShaderStageFlagBits)0;
		}

		ShaderStage ShaderStageFromVkShaderStage(VkShaderStageFlagBits stage)
		{
			switch (stage) {
			case VK_SHADER_STAGE_VERTEX_BIT:
				return ShaderStage::Vertex;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				return ShaderStage::Fragment;
			case VK_SHADER_STAGE_COMPUTE_BIT:
				return ShaderStage::Compute;
			default:
				CORE_ERROR("Unexisting type");
			}
			return (ShaderStage)0;
		}

	} // namespace Utils

	ShaderPack::ShaderPack(const std::filesystem::path& path)
		: path(path)
	{
		// Read index
		FileStreamReader serializer(path);
		if (!serializer)
			return;

		serializer.read_raw(file.header);
		if (memcmp(file.header.HEADER, "HZSP", 4) != 0)
			return;

		loaded = true;
		for (uint32_t i = 0; i < file.header.ShaderProgramCount; i++) {
			uint32_t key;
			serializer.read_raw(key);
			auto& shaderProgramInfo = file.index.shader_programs[key];
			serializer.read_raw(shaderProgramInfo.ReflectionDataOffset);
			serializer.read_array(shaderProgramInfo.ModuleIndices);
		}

		auto sp = serializer.get_stream_position();
		serializer.read_array(file.index.shader_modules, file.header.ShaderModuleCount);
	}

	bool ShaderPack::contains(std::string_view name) const
	{
		auto hash = Hash::generate_fnv_hash(name);
		return file.index.shader_programs.find(hash) != file.index.shader_programs.end();
	}

	Reference<Shader> ShaderPack::load_shader(std::string_view name)
	{
		uint32_t nameHash = Hash::generate_fnv_hash(name);
		CORE_VERIFY(contains(name), "");

		const auto& shaderProgramInfo = file.index.shader_programs.at(nameHash);

		FileStreamReader serializer(path);

		serializer.set_stream_position(shaderProgramInfo.ReflectionDataOffset);

		// Debug only
		std::string shaderName;
		{
			std::string path(name);
			size_t found = path.find_last_of("/\\");
			shaderName = found != std::string::npos ? path.substr(found + 1) : path;
			found = shaderName.find_last_of('.');
			shaderName = found != std::string::npos ? shaderName.substr(0, found) : name;
		}

		Reference<VulkanShader> vulkanShader = Reference<VulkanShader>::create();
		vulkanShader->name = shaderName;
		vulkanShader->asset_path = name;
		vulkanShader->try_read_reflection_data(&serializer);
		// vulkanShader->m_DisableOptimization =

		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> shader_modules;
		for (uint32_t index : shaderProgramInfo.ModuleIndices) {
			const auto& info = file.index.shader_modules[index];
			auto& moduleData = shader_modules[Utils::ShaderStageToVkShaderStage((Utils::ShaderStage)info.Stage)];

			serializer.set_stream_position(info.PackedOffset);
			serializer.read_array(moduleData, (uint32_t)info.PackedSize);
		}

		serializer.set_stream_position(shaderProgramInfo.ReflectionDataOffset);
		vulkanShader->try_read_reflection_data(&serializer);

		vulkanShader->load_and_create_shaders(shader_modules);
		vulkanShader->create_descriptors();

		// Renderer::AcknowledgeParsedGlobalMacros(compiler->GetAcknowledgedMacros(), vulkanShader);
		// Renderer::OnShaderReloaded(vulkanShader->GetHash());
		return vulkanShader;
	}

	Reference<ShaderPack> ShaderPack::create_from_library(
		Reference<ShaderLibrary> shaderLibrary, const std::filesystem::path& path)
	{
		Reference<ShaderPack> shaderPack = Reference<ShaderPack>::create();

		const auto& shaderMap = shaderLibrary->get_shaders();
		auto& shaderPackFile = shaderPack->file;

		shaderPackFile.header.Version = 1;
		shaderPackFile.header.ShaderProgramCount = (uint32_t)shaderMap.size();
		shaderPackFile.header.ShaderModuleCount = 0;

		// Determine number of modules (per shader)
		// NOTE(Yan): this currently doesn't care about duplicated modules, but it should (eventually, not that
		// important atm)
		uint32_t shaderModuleIndex = 0;
		uint32_t shaderModuleIndexArraySize = 0;
		for (const auto& [name, shader] : shaderMap) {
			Reference<VulkanShader> vulkanShader = shader.as<VulkanShader>();
			const auto& shaderData = vulkanShader->shader_data;

			shaderPackFile.header.ShaderModuleCount += (uint32_t)shaderData.size();
			auto& shaderProgramInfo = shaderPackFile.index.shader_programs[(uint32_t)vulkanShader->get_hash()];

			for (int i = 0; i < (int)shaderData.size(); i++)
				shaderProgramInfo.ModuleIndices.emplace_back(shaderModuleIndex++);

			shaderModuleIndexArraySize += sizeof(uint32_t); // size
			shaderModuleIndexArraySize += (uint32_t)shaderData.size() * sizeof(uint32_t); // indices
		}

		uint32_t shaderProgramIndexSize = shaderPackFile.header.ShaderProgramCount
				* (sizeof(std::map<uint32_t, ShaderPackFile::ShaderProgramInfo>::key_type)
					+ sizeof(ShaderPackFile::ShaderProgramInfo::ReflectionDataOffset))
			+ shaderModuleIndexArraySize;

		FileStreamWriter serializer(path);

		// Write header
		serializer.write_raw<ShaderPackFile::FileHeader>(shaderPackFile.header);

		// ===============
		// Write index
		// ===============
		// Write dummy data for shader programs
		uint64_t shaderProgramIndexPos = serializer.get_stream_position();
		serializer.write_zero(shaderProgramIndexSize);

		// Write dummy data for shader modules
		uint64_t shaderModuleIndexPos = serializer.get_stream_position();
		serializer.write_zero(shaderPackFile.header.ShaderModuleCount * sizeof(ShaderPackFile::ShaderModuleInfo));
		for (const auto& [name, shader] : shaderMap) {
			Reference<VulkanShader> vulkanShader = shader.as<VulkanShader>();

			// Serialize reflection data
			shaderPackFile.index.shader_programs[(uint32_t)vulkanShader->get_hash()].ReflectionDataOffset
				= serializer.get_stream_position();
			vulkanShader->serialize_reflection_data(&serializer);

			// Serialize SPIR-V data
			const auto& shaderData = vulkanShader->shader_data;
			for (const auto& [stage, data] : shaderData) {
				auto& indexShaderModule = shaderPackFile.index.shader_modules.emplace_back();
				indexShaderModule.PackedOffset = serializer.get_stream_position();
				indexShaderModule.PackedSize = data.size();
				indexShaderModule.Stage = (uint8_t)Utils::ShaderStageFromVkShaderStage(stage);

				serializer.write_array(data, false);
			}
		}

		// Write program index
		serializer.set_stream_position(shaderProgramIndexPos);
		uint64_t begin = shaderProgramIndexPos;
		for (const auto& [name, programInfo] : shaderPackFile.index.shader_programs) {
			serializer.write_raw(name);
			serializer.write_raw(programInfo.ReflectionDataOffset);
			serializer.write_array(programInfo.ModuleIndices);
		}
		uint64_t end = serializer.get_stream_position();
		uint64_t s = end - begin;

		// Write module index
		serializer.set_stream_position(shaderModuleIndexPos);
		serializer.write_array(shaderPackFile.index.shader_modules, false);

		return shaderPack;
	}

} // namespace ForgottenEngine
