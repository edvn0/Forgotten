#include "fg_pch.hpp"

#include "vulkan/compiler/VulkanShaderCompiler.hpp"

#include "serialize/FileStream.hpp"
#include "utilities/StringUtils.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanShader.hpp"
#include "vulkan/compiler/VulkanShaderCache.hpp"
#include "vulkan/compiler/preprocessor/GlslIncluder.hpp"

#include <libshaderc_util/file_finder.h>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/libspirv.h>
#include <spirv_cross/spirv_glsl.hpp>

namespace ForgottenEngine {

	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource::UniformBuffer>>
		compiler_uniform_buffers; // set -> binding point -> buffer
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource::StorageBuffer>>
		compiler_storage_buffers; // set -> binding point -> buffer

	namespace Utils {

		static const char* get_cache_directory()
		{
			// TODO: make sure the assets directory is valid
			return "shaders/cache/vulkan";
		}

		static void create_cache_directory_if_needed()
		{
			std::string cacheDirectory = get_cache_directory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}

		static ShaderUniformType SPIRTypeToShaderUniformType(spirv_cross::SPIRType type)
		{
			switch (type.basetype) {
			case spirv_cross::SPIRType::Boolean:
				return ShaderUniformType::Bool;
			case spirv_cross::SPIRType::Int:
				if (type.vecsize == 1)
					return ShaderUniformType::Int;
				if (type.vecsize == 2)
					return ShaderUniformType::IVec2;
				if (type.vecsize == 3)
					return ShaderUniformType::IVec3;
				if (type.vecsize == 4)
					return ShaderUniformType::IVec4;

			case spirv_cross::SPIRType::UInt:
				return ShaderUniformType::UInt;
			case spirv_cross::SPIRType::Float:
				if (type.columns == 3)
					return ShaderUniformType::Mat3;
				if (type.columns == 4)
					return ShaderUniformType::Mat4;

				if (type.vecsize == 1)
					return ShaderUniformType::Float;
				if (type.vecsize == 2)
					return ShaderUniformType::Vec2;
				if (type.vecsize == 3)
					return ShaderUniformType::Vec3;
				if (type.vecsize == 4)
					return ShaderUniformType::Vec4;
				break;

			default:
				CORE_ASSERT(false, "Unknown type!");
			}

			return ShaderUniformType::None;
		}
	} // namespace Utils

	VulkanShaderCompiler::VulkanShaderCompiler(const std::filesystem::path& shaderSourcePath, bool disableOptimization)
		: shader_source_path(shaderSourcePath)
		, disable_optimization(disableOptimization)
	{
		language = ShaderUtils::ShaderLangFromExtension(shaderSourcePath.extension().string());
	}

	bool VulkanShaderCompiler::reload(bool forceCompile)
	{
		shader_source.clear();
		stages_metadata.clear();
		spirv_debug_data.clear();
		spirv_data.clear();

		Utils::create_cache_directory_if_needed();
		const std::string source = StringUtils::read_file_and_skip_bom(shader_source_path);
		CORE_VERIFY(source.size(), "Failed to load shader!");

		shader_source = pre_process(source);
		const VkShaderStageFlagBits changedStages = VulkanShaderCache::has_changed(this);

		bool compileSucceeded
			= compile_or_get_vulkan_binaries(spirv_debug_data, spirv_data, changedStages, forceCompile);
		if (!compileSucceeded) {
			CORE_ASSERT(false, "");
			return false;
		}

		// Reflection
		if (forceCompile || changedStages || !try_read_cached_reflection_data()) {
			reflect_all_shader_stages(spirv_debug_data);
			serialize_reflection_data();
		}

		return true;
	}

	void VulkanShaderCompiler::clear_uniform_buffers()
	{
		compiler_uniform_buffers.clear();
		compiler_storage_buffers.clear();
	}

	std::map<VkShaderStageFlagBits, std::string> VulkanShaderCompiler::pre_process(const std::string& source)
	{
		switch (language) {
		case ShaderUtils::SourceLang::GLSL:
			return pre_process_glsl(source);
		default:
			CORE_ASSERT(false, "Only GLSL supported.");
		}
		return {};
	}

	std::map<VkShaderStageFlagBits, std::string> VulkanShaderCompiler::pre_process_glsl(const std::string& source)
	{
		std::map<VkShaderStageFlagBits, std::string> shaderSources
			= ShaderPreprocessor::PreprocessShader<ShaderUtils::SourceLang::GLSL>(source, acknowledged_macros);

		static shaderc::Compiler compiler;

		shaderc_util::FileFinder fileFinder;
		fileFinder.search_path().emplace_back("Include/GLSL/"); // Main include directory
		fileFinder.search_path().emplace_back("Include/Common/"); // Shared include directory
		for (auto& [stage, shaderSource] : shaderSources) {
			shaderc::CompileOptions options;
			options.AddMacroDefinition("__GLSL__");
			options.AddMacroDefinition(std::string(ShaderUtils::VKStageToShaderMacro(stage)));

			const auto& globalMacros = Renderer::get_global_shader_macros();
			for (const auto& [name, value] : globalMacros)
				options.AddMacroDefinition(name, value);

			// Deleted by shaderc and created per stage
			GlslIncluder* includer = new GlslIncluder(&fileFinder);

			options.SetIncluder(std::unique_ptr<GlslIncluder>(includer));
			const auto preProcessingResult = compiler.PreprocessGlsl(
				shaderSource, ShaderUtils::ShaderStageToShaderC(stage), shader_source_path.string().c_str(), options);
			if (preProcessingResult.GetCompilationStatus() != shaderc_compilation_status_success)
				CORE_ERROR("Renderer",
					fmt::format("Failed to pre-process \"{}\"'s {} shader.\nError: {}", shader_source_path.string(),
						ShaderUtils::ShaderStageToString(stage), preProcessingResult.GetErrorMessage()));

			stages_metadata[stage].HashValue = Hash::generate_fnv_hash(shaderSource);
			stages_metadata[stage].Headers = std::move(includer->get_include_data());

			acknowledged_macros.merge(includer->get_parsed_special_macros());

			shaderSource = std::string(preProcessingResult.begin(), preProcessingResult.end());
		}
		return shaderSources;
	}

	std::string VulkanShaderCompiler::compile(
		std::vector<uint32_t>& outputBinary, const VkShaderStageFlagBits stage, CompilationOptions options) const
	{
		const std::string& stageSource = shader_source.at(stage);

		if (language == ShaderUtils::SourceLang::GLSL) {
			static shaderc::Compiler compiler;
			shaderc::CompileOptions shaderCOptions;
			shaderCOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
			shaderCOptions.SetWarningsAsErrors();
			if (options.GenerateDebugInfo)
				shaderCOptions.SetGenerateDebugInfo();

			if (options.Optimize)
				shaderCOptions.SetOptimizationLevel(shaderc_optimization_level_performance);

			// compile shader
			const shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(stageSource,
				ShaderUtils::ShaderStageToShaderC(stage), shader_source_path.string().c_str(), shaderCOptions);

			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
				return fmt::format("\n{}While compiling shader file: {} \nAt stage: {}", module.GetErrorMessage(),
					shader_source_path.string(), ShaderUtils::ShaderStageToString(stage));

			outputBinary = std::vector<uint32_t>(module.begin(), module.end());
			return {}; // Success
		}
		return "Unknown language!";
	}

	Reference<VulkanShader> VulkanShaderCompiler::compile(
		const std::filesystem::path& shaderSourcePath, bool forceCompile, bool disableOptimization)
	{
		// Set name
		std::string path = shaderSourcePath.string();
		size_t found = path.find_last_of("/\\");
		std::string name = found != std::string::npos ? path.substr(found + 1) : path;
		found = name.find_last_of('.');
		name = found != std::string::npos ? name.substr(0, found) : name;

		Reference<VulkanShader> shader = Reference<VulkanShader>::create();
		shader->asset_path = shaderSourcePath;
		shader->name = name;
		shader->disable_optimisations = disableOptimization;

		Reference<VulkanShaderCompiler> compiler
			= Reference<VulkanShaderCompiler>::create(shaderSourcePath, disableOptimization);
		compiler->reload(forceCompile);

		shader->load_and_create_shaders(compiler->get_spirv_data());
		shader->set_reflection_data(compiler->reflection_data);
		shader->create_descriptors();

		Renderer::acknowledge_parsed_global_macros(compiler->get_acknowledged_macros(), shader);
		Renderer::on_shader_reloaded(shader->get_hash());
		return shader;
	}

	bool VulkanShaderCompiler::try_recompile(Reference<VulkanShader> shader)
	{
		Reference<VulkanShaderCompiler> compiler
			= Reference<VulkanShaderCompiler>::create(shader->asset_path, shader->disable_optimisations);
		bool compileSucceeded = compiler->reload(true);
		if (!compileSucceeded)
			return false;

		shader->release();

		shader->load_and_create_shaders(compiler->get_spirv_data());
		shader->set_reflection_data(compiler->reflection_data);
		shader->create_descriptors();

		Renderer::acknowledge_parsed_global_macros(compiler->get_acknowledged_macros(), shader);
		Renderer::on_shader_reloaded(shader->get_hash());

		return true;
	}

	bool VulkanShaderCompiler::compile_or_get_vulkan_binaries(
		std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputDebugBinary,
		std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputBinary,
		const VkShaderStageFlagBits changedStages, const bool forceCompile)
	{
		for (auto [stage, source] : shader_source) {
			if (!compile_or_get_vulkan_binary(stage, outputDebugBinary[stage], true, changedStages, forceCompile))
				return false;
			if (!compile_or_get_vulkan_binary(stage, outputBinary[stage], false, changedStages, forceCompile))
				return false;
		}
		return true;
	}

	bool VulkanShaderCompiler::compile_or_get_vulkan_binary(VkShaderStageFlagBits stage,
		std::vector<uint32_t>& outputBinary, bool debug, VkShaderStageFlagBits changedStages, bool forceCompile)
	{
		const std::filesystem::path cacheDirectory = Utils::get_cache_directory();

		// compile shader with debug info so we can reflect
		const auto extension = ShaderUtils::ShaderStageCachedFileExtension(stage, debug);
		if (!forceCompile && stage & ~changedStages) // Per-stage cache is found and is unchanged
		{
			try_get_vulkan_cached_binary(cacheDirectory, extension, outputBinary);
		}

		if (outputBinary.empty()) {
			CompilationOptions options;
			if (debug) {
				options.GenerateDebugInfo = true;
				options.Optimize = false;
			} else {
				options.GenerateDebugInfo = false;
				// Disable optimization for compute shaders because of shaderc internal error
				options.Optimize = !disable_optimization && stage != VK_SHADER_STAGE_COMPUTE_BIT;
			}

			if (std::string error = compile(outputBinary, stage, options); error.size()) {
				CORE_ERR("Renderer {}", error);
				try_get_vulkan_cached_binary(cacheDirectory, extension, outputBinary);
				if (outputBinary.empty()) {
					CORE_ERR("Failed to compile shader and couldn't find a cached version.");
				} else {
					CORE_ERR("Failed to compile {}:{} so a cached version was loaded instead.",
						shader_source_path.string(), ShaderUtils::ShaderStageToString(stage));
				}
				return false;
			} else // compile success
			{
				auto path = cacheDirectory / (shader_source_path.filename().string() + extension);
				std::string cachedFilePath = path.string();

				FILE* f = fopen(cachedFilePath.c_str(), "wb");
				if (!f)
					CORE_ASSERT(false, "Failed to cache shader binary!");
				fwrite(outputBinary.data(), sizeof(uint32_t), outputBinary.size(), f);
				fclose(f);
			}
		}

		return true;
	}

	void VulkanShaderCompiler::clear_reflection_data()
	{
		reflection_data.shader_descriptor_sets.clear();
		reflection_data.resources.clear();
		reflection_data.constant_buffers.clear();
		reflection_data.push_constant_ranges.clear();
	}

	void VulkanShaderCompiler::try_get_vulkan_cached_binary(const std::filesystem::path& cacheDirectory,
		const std::string& extension, std::vector<uint32_t>& outputBinary) const
	{
		const auto path = cacheDirectory / (shader_source_path.filename().string() + extension);
		const std::string cachedFilePath = path.string();

		FILE* f = fopen(cachedFilePath.data(), "rb");
		if (!f)
			return;

		fseek(f, 0, SEEK_END);
		uint64_t size = ftell(f);
		fseek(f, 0, SEEK_SET);
		outputBinary = std::vector<uint32_t>(size / sizeof(uint32_t));
		fread(outputBinary.data(), sizeof(uint32_t), outputBinary.size(), f);
		fclose(f);
	}

	bool VulkanShaderCompiler::try_read_cached_reflection_data()
	{
		struct ReflectionFileHeader {
			char Header[4] = { 'F', 'G', 'S', 'R' };
		} header;

		std::filesystem::path cacheDirectory = Utils::get_cache_directory();
		const auto path = cacheDirectory / (shader_source_path.filename().string() + ".cached_vulkan.refl");
		FileStreamReader serializer(path);
		if (!serializer)
			return false;

		serializer.read_raw(header);

		bool validHeader = memcmp(&header, "FGSR", 4) == 0;
		CORE_ASSERT(validHeader, "");

		clear_reflection_data();

		uint32_t shaderDescriptorSetCount;
		serializer.read_raw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++) {
			auto& descriptorSet = reflection_data.shader_descriptor_sets.emplace_back();
			serializer.read_map(descriptorSet.uniform_buffers);
			serializer.read_map(descriptorSet.storage_buffers);
			serializer.read_map(descriptorSet.image_samplers);
			serializer.read_map(descriptorSet.storage_images);
			serializer.read_map(descriptorSet.separate_textures);
			serializer.read_map(descriptorSet.separate_samplers);
			serializer.read_map(descriptorSet.write_descriptor_sets);
		}

		serializer.read_map(reflection_data.resources);
		serializer.read_map(reflection_data.constant_buffers);
		serializer.read_array(reflection_data.push_constant_ranges);

		return true;
	}

	void VulkanShaderCompiler::serialize_reflection_data()
	{
		struct ReflectionFileHeader {
			char Header[4] = { 'F', 'G', 'S', 'R' };
		} header;

		std::filesystem::path cacheDirectory = Utils::get_cache_directory();
		const auto path = cacheDirectory / (shader_source_path.filename().string() + ".cached_vulkan.refl");
		FileStreamWriter serializer(path);
		serializer.write_raw(header);
		serialize_reflection_data(&serializer);
	}

	void VulkanShaderCompiler::serialize_reflection_data(StreamWriter* serializer)
	{
		serializer->write_raw<uint32_t>((uint32_t)reflection_data.shader_descriptor_sets.size());
		for (const auto& descriptorSet : reflection_data.shader_descriptor_sets) {
			serializer->write_map(descriptorSet.uniform_buffers);
			serializer->write_map(descriptorSet.storage_buffers);
			serializer->write_map(descriptorSet.image_samplers);
			serializer->write_map(descriptorSet.storage_images);
			serializer->write_map(descriptorSet.separate_textures);
			serializer->write_map(descriptorSet.separate_samplers);
			serializer->write_map(descriptorSet.write_descriptor_sets);
		}

		serializer->write_map(reflection_data.resources);
		serializer->write_map(reflection_data.constant_buffers);
		serializer->write_array(reflection_data.push_constant_ranges);
	}

	void VulkanShaderCompiler::reflect_all_shader_stages(
		const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
	{
		clear_reflection_data();

		for (auto [stage, data] : shaderData) {
			reflect(stage, data);
		}
	}

	void VulkanShaderCompiler::reflect(VkShaderStageFlagBits shaderStage, const std::vector<uint32_t>& shaderData)
	{

		spirv_cross::Compiler compiler(shaderData);
		auto resources = compiler.get_shader_resources();

		for (const auto& resource : resources.uniform_buffers) {
			auto activeBuffers = compiler.get_active_buffer_ranges(resource.id);
			// Discard unused buffers from headers
			if (!activeBuffers.empty()) {
				const auto& name = resource.name;
				auto& bufferType = compiler.get_type(resource.base_type_id);
				int memberCount = (uint32_t)bufferType.member_types.size();
				uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

				if (descriptorSet >= reflection_data.shader_descriptor_sets.size())
					reflection_data.shader_descriptor_sets.resize(descriptorSet + 1);

				ShaderResource::ShaderDescriptorSet& shaderDescriptorSet
					= reflection_data.shader_descriptor_sets[descriptorSet];
				if (compiler_uniform_buffers[descriptorSet].find(binding)
					== compiler_uniform_buffers[descriptorSet].end()) {
					ShaderResource::UniformBuffer uniformBuffer;
					uniformBuffer.BindingPoint = binding;
					uniformBuffer.Size = size;
					uniformBuffer.Name = name;
					uniformBuffer.ShaderStage = VK_SHADER_STAGE_ALL;
					compiler_uniform_buffers.at(descriptorSet)[binding] = uniformBuffer;
				} else {
					ShaderResource::UniformBuffer& uniformBuffer
						= compiler_uniform_buffers.at(descriptorSet).at(binding);
					if (size > uniformBuffer.Size)
						uniformBuffer.Size = size;
				}
				shaderDescriptorSet.uniform_buffers[binding] = compiler_uniform_buffers.at(descriptorSet).at(binding);
			}
		}

		for (const auto& resource : resources.storage_buffers) {
			auto activeBuffers = compiler.get_active_buffer_ranges(resource.id);
			// Discard unused buffers from headers
			if (!activeBuffers.empty()) {
				const auto& name = resource.name;
				auto& bufferType = compiler.get_type(resource.base_type_id);
				uint32_t memberCount = (uint32_t)bufferType.member_types.size();
				uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);

				if (descriptorSet >= reflection_data.shader_descriptor_sets.size())
					reflection_data.shader_descriptor_sets.resize(descriptorSet + 1);

				ShaderResource::ShaderDescriptorSet& shaderDescriptorSet
					= reflection_data.shader_descriptor_sets[descriptorSet];
				if (compiler_storage_buffers[descriptorSet].find(binding)
					== compiler_storage_buffers[descriptorSet].end()) {
					ShaderResource::StorageBuffer storageBuffer;
					storageBuffer.BindingPoint = binding;
					storageBuffer.Size = size;
					storageBuffer.Name = name;
					storageBuffer.ShaderStage = VK_SHADER_STAGE_ALL;
					compiler_storage_buffers.at(descriptorSet)[binding] = storageBuffer;
				} else {
					ShaderResource::StorageBuffer& storageBuffer
						= compiler_storage_buffers.at(descriptorSet).at(binding);
					if (size > storageBuffer.Size)
						storageBuffer.Size = size;
				}

				shaderDescriptorSet.storage_buffers[binding] = compiler_storage_buffers.at(descriptorSet).at(binding);
			}
		}

		for (const auto& resource : resources.push_constant_buffers) {
			const auto& bufferName = resource.name;
			auto& bufferType = compiler.get_type(resource.base_type_id);
			auto bufferSize = (uint32_t)compiler.get_declared_struct_size(bufferType);
			uint32_t memberCount = uint32_t(bufferType.member_types.size());
			uint32_t bufferOffset = 0;
			if (!reflection_data.push_constant_ranges.empty())
				bufferOffset = reflection_data.push_constant_ranges.back().Offset
					+ reflection_data.push_constant_ranges.back().Size;

			auto& pushConstantRange = reflection_data.push_constant_ranges.emplace_back();
			pushConstantRange.ShaderStage = shaderStage;
			pushConstantRange.Size = bufferSize - bufferOffset;
			pushConstantRange.Offset = bufferOffset;

			// Skip empty push constant buffers - these are for the renderer only
			if (bufferName.empty() || bufferName == "u_Renderer")
				continue;

			ShaderBuffer& buffer = reflection_data.constant_buffers[bufferName];
			buffer.Name = bufferName;
			buffer.Size = bufferSize - bufferOffset;

			for (uint32_t i = 0; i < memberCount; i++) {
				const auto& type = compiler.get_type(bufferType.member_types[i]);
				const auto& memberName = compiler.get_member_name(bufferType.self, i);
				auto size = (uint32_t)compiler.get_declared_struct_member_size(bufferType, i);
				auto offset = compiler.type_struct_member_offset(bufferType, i) - bufferOffset;

				std::string uniformName = fmt::format("{}.{}", bufferName, memberName);
				buffer.Uniforms[uniformName]
					= ShaderUniform(uniformName, Utils::SPIRTypeToShaderUniformType(type), size, offset);
			}
		}

		for (const auto& resource : resources.sampled_images) {
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = baseType.image.dim;
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= reflection_data.shader_descriptor_sets.size())
				reflection_data.shader_descriptor_sets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet
				= reflection_data.shader_descriptor_sets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.image_samplers[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = shaderStage;
			imageSampler.ArraySize = arraySize;

			reflection_data.resources[name] = ShaderResourceDeclaration(name, binding, 1);
		}

		for (const auto& resource : resources.separate_images) {
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = baseType.image.dim;
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= reflection_data.shader_descriptor_sets.size())
				reflection_data.shader_descriptor_sets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet
				= reflection_data.shader_descriptor_sets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.separate_textures[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = shaderStage;
			imageSampler.ArraySize = arraySize;

			reflection_data.resources[name] = ShaderResourceDeclaration(name, binding, 1);
		}

		for (const auto& resource : resources.separate_samplers) {
			const auto& name = resource.name;
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= reflection_data.shader_descriptor_sets.size())
				reflection_data.shader_descriptor_sets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet
				= reflection_data.shader_descriptor_sets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.separate_samplers[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = shaderStage;
			imageSampler.ArraySize = arraySize;

			reflection_data.resources[name] = ShaderResourceDeclaration(name, binding, 1);
		}

		for (const auto& resource : resources.storage_images) {
			const auto& name = resource.name;
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = type.image.dim;
			uint32_t arraySize = type.array[0];
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= reflection_data.shader_descriptor_sets.size())
				reflection_data.shader_descriptor_sets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet
				= reflection_data.shader_descriptor_sets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.storage_images[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ArraySize = arraySize;
			imageSampler.ShaderStage = shaderStage;

			reflection_data.resources[name] = ShaderResourceDeclaration(name, binding, 1);
		}

		for (const auto& macro : acknowledged_macros) {
			(void)macro;
		}
	}

} // namespace ForgottenEngine
