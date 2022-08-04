#include "fg_pch.hpp"

#include "vulkan/VulkanShader.hpp"

#include <filesystem>

#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanRenderer.hpp"

namespace ForgottenEngine {

VulkanShader::VulkanShader(const std::string& path, bool forceCompile, bool disableOptimization)
	: asset_path(path)
	, disable_optimisations(disableOptimization)
{
	name = Assets::path_without_extensions(path, { ".vert", ".frag" });

	reload(forceCompile);
}

void VulkanShader::release()
{
	auto& pipelineCIs = stage_create_infos;
	Renderer::submit_resource_free([pipelineCIs]() {
		const auto vulkanDevice = VulkanContext::get_current_device()->get_vulkan_device();

		for (const auto& ci : pipelineCIs)
			if (ci.module)
				vkDestroyShaderModule(vulkanDevice, ci.module, nullptr);
	});

	for (auto& ci : pipelineCIs)
		ci.module = nullptr;

	stage_create_infos.clear();
	descriptor_set_layouts.clear();
	type_counts.clear();
}

VulkanShader::~VulkanShader()
{
	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
	Renderer::submit_resource_free([device, instance = Reference(this)]() {
		for (const auto& ci : instance->stage_create_infos)
			if (ci.module)
				vkDestroyShaderModule(device, ci.module, nullptr);
	});
}

void VulkanShader::rt_reload(const bool forceCompile) { }

void VulkanShader::reload(bool forceCompile)
{
	Renderer::submit([instance = Reference(this), forceCompile]() mutable { instance->rt_reload(forceCompile); });
}

size_t VulkanShader::get_hash() const { return 1; }

void VulkanShader::load_and_create_shaders(
	const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
{
	shader_data = shaderData;

	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
	stage_create_infos.clear();
	std::string moduleName;
	for (auto [stage, data] : shaderData) {
		CORE_ASSERT(data.size(), "");
		VkShaderModuleCreateInfo moduleCreateInfo{};

		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = data.size() * sizeof(uint32_t);
		moduleCreateInfo.pCode = data.data();

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule));

		VkPipelineShaderStageCreateInfo& shaderStage = stage_create_infos.emplace_back();
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = stage;
		shaderStage.module = shaderModule;
		shaderStage.pName = "main";
	}
}

void VulkanShader::create_descriptor()
{
	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

	//////////////////////////////////////////////////////////////////////
	// Descriptor Pool
	//////////////////////////////////////////////////////////////////////

	type_counts.clear();
	for (uint32_t set = 0; set < reflection_data.shader_descriptor_sets.size(); set++) {
		auto& shader_desc_set = reflection_data.shader_descriptor_sets[set];

		if (!shader_desc_set.uniform_buffers.empty()) {
			VkDescriptorPoolSize& typeCount = type_counts[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			typeCount.descriptorCount = (uint32_t)(shader_desc_set.uniform_buffers.size());
		}
		if (!shader_desc_set.StorageBuffers.empty()) {
			VkDescriptorPoolSize& typeCount = type_counts[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			typeCount.descriptorCount = (uint32_t)(shader_desc_set.StorageBuffers.size());
		}
		if (!shader_desc_set.ImageSamplers.empty()) {
			VkDescriptorPoolSize& typeCount = type_counts[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			typeCount.descriptorCount = (uint32_t)(shader_desc_set.ImageSamplers.size());
		}
		if (!shader_desc_set.SeparateTextures.empty()) {
			VkDescriptorPoolSize& typeCount = type_counts[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			typeCount.descriptorCount = (uint32_t)(shader_desc_set.SeparateTextures.size());
		}
		if (!shader_desc_set.SeparateSamplers.empty()) {
			VkDescriptorPoolSize& typeCount = type_counts[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLER;
			typeCount.descriptorCount = (uint32_t)(shader_desc_set.SeparateSamplers.size());
		}
		if (!shader_desc_set.StorageImages.empty()) {
			VkDescriptorPoolSize& typeCount = type_counts[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			typeCount.descriptorCount = (uint32_t)(shader_desc_set.StorageImages.size());
		}

		//////////////////////////////////////////////////////////////////////
		// Descriptor Set Layout
		//////////////////////////////////////////////////////////////////////

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
		for (auto& [binding, uniformBuffer] : shader_desc_set.uniform_buffers) {
			VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
			layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			layoutBinding.descriptorCount = 1;
			layoutBinding.stageFlags = uniformBuffer.ShaderStage;
			layoutBinding.pImmutableSamplers = nullptr;
			layoutBinding.binding = binding;

			VkWriteDescriptorSet& set = shader_desc_set.write_descriptor_sets[uniformBuffer.Name];
			set = {};
			set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			set.descriptorType = layoutBinding.descriptorType;
			set.descriptorCount = 1;
			set.dstBinding = layoutBinding.binding;
		}

		for (auto& [binding, storageBuffer] : shader_desc_set.StorageBuffers) {
			VkDescriptorSetLayoutBinding& layoutBinding = layoutBindings.emplace_back();
			layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			layoutBinding.descriptorCount = 1;
			layoutBinding.stageFlags = storageBuffer.ShaderStage;
			layoutBinding.pImmutableSamplers = nullptr;
			layoutBinding.binding = binding;
			CORE_ASSERT(shader_desc_set.uniform_buffers.find(binding) == shader_desc_set.uniform_buffers.end(),
				"Binding is already present!");

			VkWriteDescriptorSet& set = shader_desc_set.write_descriptor_sets[storageBuffer.Name];
			set = {};
			set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			set.descriptorType = layoutBinding.descriptorType;
			set.descriptorCount = 1;
			set.dstBinding = layoutBinding.binding;
		}

		for (auto& [binding, imageSampler] : shader_desc_set.ImageSamplers) {
			auto& layoutBinding = layoutBindings.emplace_back();
			layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			layoutBinding.descriptorCount = imageSampler.ArraySize;
			layoutBinding.stageFlags = imageSampler.ShaderStage;
			layoutBinding.pImmutableSamplers = nullptr;
			layoutBinding.binding = binding;

			CORE_ASSERT(shader_desc_set.uniform_buffers.find(binding) == shader_desc_set.uniform_buffers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.StorageBuffers.find(binding) == shader_desc_set.StorageBuffers.end(),
				"Binding is already present!");

			VkWriteDescriptorSet& set = shader_desc_set.write_descriptor_sets[imageSampler.Name];
			set = {};
			set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			set.descriptorType = layoutBinding.descriptorType;
			set.descriptorCount = imageSampler.ArraySize;
			set.dstBinding = layoutBinding.binding;
		}

		for (auto& [binding, imageSampler] : shader_desc_set.SeparateTextures) {
			auto& layoutBinding = layoutBindings.emplace_back();
			layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			layoutBinding.descriptorCount = imageSampler.ArraySize;
			layoutBinding.stageFlags = imageSampler.ShaderStage;
			layoutBinding.pImmutableSamplers = nullptr;
			layoutBinding.binding = binding;

			CORE_ASSERT(shader_desc_set.uniform_buffers.find(binding) == shader_desc_set.uniform_buffers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.ImageSamplers.find(binding) == shader_desc_set.ImageSamplers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.StorageBuffers.find(binding) == shader_desc_set.StorageBuffers.end(),
				"Binding is already present!");

			VkWriteDescriptorSet& set = shader_desc_set.write_descriptor_sets[imageSampler.Name];
			set = {};
			set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			set.descriptorType = layoutBinding.descriptorType;
			set.descriptorCount = imageSampler.ArraySize;
			set.dstBinding = layoutBinding.binding;
		}

		for (auto& [binding, imageSampler] : shader_desc_set.SeparateSamplers) {
			auto& layoutBinding = layoutBindings.emplace_back();
			layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			layoutBinding.descriptorCount = imageSampler.ArraySize;
			layoutBinding.stageFlags = imageSampler.ShaderStage;
			layoutBinding.pImmutableSamplers = nullptr;
			layoutBinding.binding = binding;

			CORE_ASSERT(shader_desc_set.uniform_buffers.find(binding) == shader_desc_set.uniform_buffers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.ImageSamplers.find(binding) == shader_desc_set.ImageSamplers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.StorageBuffers.find(binding) == shader_desc_set.StorageBuffers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.SeparateTextures.find(binding) == shader_desc_set.SeparateTextures.end(),
				"Binding is already present!");

			VkWriteDescriptorSet& set = shader_desc_set.write_descriptor_sets[imageSampler.Name];
			set = {};
			set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			set.descriptorType = layoutBinding.descriptorType;
			set.descriptorCount = imageSampler.ArraySize;
			set.dstBinding = layoutBinding.binding;
		}

		for (auto& [bindingAndSet, imageSampler] : shader_desc_set.StorageImages) {
			auto& layoutBinding = layoutBindings.emplace_back();
			layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			layoutBinding.descriptorCount = imageSampler.ArraySize;
			layoutBinding.stageFlags = imageSampler.ShaderStage;
			layoutBinding.pImmutableSamplers = nullptr;

			uint32_t binding = bindingAndSet & 0xffffffff;
			// uint32_t descriptorSet = (bindingAndSet >> 32);
			layoutBinding.binding = binding;

			CORE_ASSERT(shader_desc_set.uniform_buffers.find(binding) == shader_desc_set.uniform_buffers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.StorageBuffers.find(binding) == shader_desc_set.StorageBuffers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.ImageSamplers.find(binding) == shader_desc_set.ImageSamplers.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.SeparateTextures.find(binding) == shader_desc_set.SeparateTextures.end(),
				"Binding is already present!");
			CORE_ASSERT(shader_desc_set.SeparateSamplers.find(binding) == shader_desc_set.SeparateSamplers.end(),
				"Binding is already present!");

			VkWriteDescriptorSet& set = shader_desc_set.write_descriptor_sets[imageSampler.Name];
			set = {};
			set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			set.descriptorType = layoutBinding.descriptorType;
			set.descriptorCount = 1;
			set.dstBinding = layoutBinding.binding;
		}

		VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.pNext = nullptr;
		descriptorLayout.bindingCount = (uint32_t)(layoutBindings.size());
		descriptorLayout.pBindings = layoutBindings.data();

		if (set >= descriptor_set_layouts.size())
			descriptor_set_layouts.resize((set + 1));
		VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptor_set_layouts[set]));
	}
}

VulkanShader::ShaderMaterialDescriptorSet VulkanShader::allocate_descriptor_set(uint32_t set)
{
	CORE_ASSERT(set < descriptor_set_layouts.size(), "");
	ShaderMaterialDescriptorSet result;

	if (reflection_data.shader_descriptor_sets.empty())
		return result;

	// TODO: remove
	result.Pool = nullptr;

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &descriptor_set_layouts[set];
	VkDescriptorSet descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(alloc_info);
	CORE_ASSERT(descriptorSet, "");
	result.descriptor_sets.push_back(descriptorSet);
	return result;
}

VulkanShader::ShaderMaterialDescriptorSet VulkanShader::create_descriptor_sets(uint32_t set)
{
	ShaderMaterialDescriptorSet result;

	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

	CORE_ASSERT(type_counts.find(set) != type_counts.end(), "");

	// TODO: Move this to the centralized renderer
	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = nullptr;
	descriptorPoolInfo.poolSizeCount = (uint32_t)type_counts.at(set).size();
	descriptorPoolInfo.pPoolSizes = type_counts.at(set).data();
	descriptorPoolInfo.maxSets = 1;

	VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &result.Pool));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = result.Pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptor_set_layouts[set];
	result.descriptor_sets.emplace_back();
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, result.descriptor_sets.data()));
	return result;
}

VulkanShader::ShaderMaterialDescriptorSet VulkanShader::create_descriptor_sets(uint32_t set, uint32_t numberOfSets)
{
	ShaderMaterialDescriptorSet result;

	VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

	std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> poolSizes;
	for (uint32_t set = 0; set < reflection_data.shader_descriptor_sets.size(); set++) {
		auto& shader_desc_set = reflection_data.shader_descriptor_sets[set];
		if (!shader_desc_set) // Empty descriptor set
			continue;

		if (!shader_desc_set.uniform_buffers.empty()) {
			VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			typeCount.descriptorCount = (uint32_t)shader_desc_set.uniform_buffers.size() * numberOfSets;
		}

		if (!shader_desc_set.StorageBuffers.empty()) {
			VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			typeCount.descriptorCount = (uint32_t)shader_desc_set.StorageBuffers.size() * numberOfSets;
		}

		if (!shader_desc_set.ImageSamplers.empty()) {
			VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			uint32_t descriptorSetCount = 0;
			for (auto&& [binding, imageSampler] : shader_desc_set.ImageSamplers)
				descriptorSetCount += imageSampler.ArraySize;

			typeCount.descriptorCount = descriptorSetCount * numberOfSets;
		}

		if (!shader_desc_set.SeparateTextures.empty()) {
			VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			uint32_t descriptorSetCount = 0;
			for (auto&& [binding, imageSampler] : shader_desc_set.SeparateTextures)
				descriptorSetCount += imageSampler.ArraySize;

			typeCount.descriptorCount = descriptorSetCount * numberOfSets;
		}

		if (!shader_desc_set.SeparateTextures.empty()) {
			VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_SAMPLER;
			uint32_t descriptorSetCount = 0;
			for (auto&& [binding, imageSampler] : shader_desc_set.SeparateSamplers)
				descriptorSetCount += imageSampler.ArraySize;

			typeCount.descriptorCount = descriptorSetCount * numberOfSets;
		}

		if (!shader_desc_set.StorageImages.empty()) {
			VkDescriptorPoolSize& typeCount = poolSizes[set].emplace_back();
			typeCount.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			typeCount.descriptorCount = (uint32_t)shader_desc_set.StorageImages.size() * numberOfSets;
		}
	}

	CORE_ASSERT(poolSizes.find(set) != poolSizes.end(), "");

	// TODO: Move this to the centralized renderer
	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = nullptr;
	descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.at(set).size();
	descriptorPoolInfo.pPoolSizes = poolSizes.at(set).data();
	descriptorPoolInfo.maxSets = numberOfSets;

	VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &result.Pool));

	result.descriptor_sets.resize(numberOfSets);

	for (uint32_t i = 0; i < numberOfSets; i++) {
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = result.Pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptor_set_layouts[set];
		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &result.descriptor_sets[i]));
	}
	return result;
}

const VkWriteDescriptorSet* VulkanShader::get_descriptor_set(const std::string& desc_set_name, uint32_t set) const
{
	CORE_ASSERT(set < reflection_data.shader_descriptor_sets.size(), "");
	CORE_ASSERT(reflection_data.shader_descriptor_sets[set], "");
	if (reflection_data.shader_descriptor_sets.at(set).write_descriptor_sets.find(desc_set_name)
		== reflection_data.shader_descriptor_sets.at(set).write_descriptor_sets.end()) {
		CORE_WARN(
			"Renderer", "Shader {0} does not contain requested descriptor set {1}", desc_set_name, desc_set_name);
		return nullptr;
	}
	return &reflection_data.shader_descriptor_sets.at(set).write_descriptor_sets.at(desc_set_name);
}

std::vector<VkDescriptorSetLayout> VulkanShader::get_all_descriptor_set_layouts()
{
	std::vector<VkDescriptorSetLayout> result;
	result.reserve(descriptor_set_layouts.size());
	for (auto& layout : descriptor_set_layouts)
		result.emplace_back(layout);

	return result;
}

const std::unordered_map<std::string, ShaderResourceDeclaration>& VulkanShader::get_resources() const
{
	return reflection_data.resources;
}

void VulkanShader::add_shader_reloaded_callback(const ShaderReloadedCallback& callback) { }

void VulkanShader::set_reflection_data(const ForgottenEngine::VulkanShader::ReflectionData& reflectionData)
{
	reflection_data = reflectionData;
}
}
