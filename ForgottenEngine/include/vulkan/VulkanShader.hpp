#pragma once

#include "render/Shader.hpp"
#include "vk_mem_alloc.h"
#include "VulkanShaderResource.hpp"

#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace ForgottenEngine {

	class VulkanShaderCompiler;

	using ShaderType = VkShaderStageFlagBits;

	class VulkanShader : public Shader {
	public:
		struct ReflectionData {
			std::vector<ShaderResource::ShaderDescriptorSet> shader_descriptor_sets;
			std::unordered_map<std::string, ShaderResourceDeclaration> resources;
			std::unordered_map<std::string, ShaderBuffer> constant_buffers;
			std::vector<ShaderResource::PushConstantRange> push_constant_ranges;
		};

	public:
		VulkanShader() = default;

		VulkanShader(const std::string& path, bool forceCompile, bool disableOptimization);

		~VulkanShader() override;

		void release();

		void reload(bool forceCompile) override;

		void rt_reload(bool forceCompile) override;

		size_t get_hash() const override;

		void set_macro(const std::string& name, const std::string& value) override { }

		const std::string& get_name() const override { return name; }

		const std::unordered_map<std::string, ShaderBuffer>& get_shader_buffers() const override { return reflection_data.constant_buffers; }

		const std::unordered_map<std::string, ShaderResourceDeclaration>& get_resources() const override;

		void set_reflection_data(const ReflectionData& reflectionData);

		void add_shader_reloaded_callback(const ShaderReloadedCallback& callback) override;

		// Vulkan-specific
		const std::vector<VkPipelineShaderStageCreateInfo>& get_pipeline_shader_stage_create_infos() const { return stage_create_infos; }

		bool try_read_reflection_data(StreamReader* serializer);

		void serialize_reflection_data(StreamWriter* serializer);

		VkDescriptorSet get_descriptor_set() { return descriptor_set; }

		VkDescriptorSetLayout get_descriptor_set_layout(uint32_t set) { return descriptor_set_layouts.at(set); }

		std::vector<VkDescriptorSetLayout> get_all_descriptor_set_layouts();

		ShaderResource::UniformBuffer& get_uniform_buffer(const uint32_t binding = 0, const uint32_t set = 0)
		{
			core_assert(reflection_data.shader_descriptor_sets.at(set).uniform_buffers.size() > binding, "");
			return reflection_data.shader_descriptor_sets.at(set).uniform_buffers.at(binding);
		}

		uint32_t get_uniform_buffer_count(const uint32_t set = 0)
		{
			if (reflection_data.shader_descriptor_sets.size() < set)
				return 0;

			return static_cast<uint32_t>(reflection_data.shader_descriptor_sets[set].uniform_buffers.size());
		}

		const std::vector<ShaderResource::ShaderDescriptorSet>& get_shader_descriptor_sets() const { return reflection_data.shader_descriptor_sets; }

		bool has_descriptor_set(uint32_t set) const { return is_in_map(type_counts, set); }

		const std::vector<ShaderResource::PushConstantRange>& get_push_constant_ranges() const { return reflection_data.push_constant_ranges; }

		struct ShaderMaterialDescriptorSet {
			VkDescriptorPool pool = nullptr;
			std::vector<VkDescriptorSet> descriptor_sets;
		};

		ShaderMaterialDescriptorSet allocate_descriptor_set(uint32_t set = 0);

		ShaderMaterialDescriptorSet create_descriptor_sets(uint32_t set = 0);

		ShaderMaterialDescriptorSet create_descriptor_sets(uint32_t set, uint32_t number_offsets);

		const VkWriteDescriptorSet* get_descriptor_set(const std::string& name, uint32_t set = 0) const;

	private:
		void load_and_create_shaders(const std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>>& shader_data);

		void create_descriptors();

	private:
		std::vector<VkPipelineShaderStageCreateInfo> stage_create_infos;

		std::filesystem::path asset_path;
		std::string name;
		bool disable_optimisations = false;

		std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>> shader_data;
		ReflectionData reflection_data;

		std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
		VkDescriptorSet descriptor_set {};
		// VkDescriptorPool m_DescriptorPool = nullptr;

		std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> type_counts;
		ShaderType shader_type;

	private:
		friend class ShaderCache;
		friend class ShaderPack;
		friend class VulkanShaderCompiler;
	};

} // namespace ForgottenEngine
