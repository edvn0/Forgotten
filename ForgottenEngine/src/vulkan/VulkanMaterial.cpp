#include "fg_pch.hpp"

#include "vulkan/VulkanMaterial.hpp"

#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanTexture.hpp"
#include "vulkan/VulkanUniformBuffer.hpp"

#include <utility>

namespace ForgottenEngine {

	VulkanMaterial::VulkanMaterial(const Reference<Shader>& shader, std::string name)
		: material_shader(shader)
		, material_name(std::move(name))
		, write_descriptors(Renderer::get_config().frames_in_flight)
		, dirty_descriptor_sets(Renderer::get_config().frames_in_flight, true)
	{
		init();
		Renderer::register_shader_dependency(shader, this);
	}

	VulkanMaterial::VulkanMaterial(Reference<Material> material, const std::string& name)
		: material_shader(material->get_shader())
		, material_name(name)
		, write_descriptors(Renderer::get_config().frames_in_flight)
		, dirty_descriptor_sets(Renderer::get_config().frames_in_flight, true)
	{
		if (name.empty())
			material_name = material->get_name();

		Renderer::register_shader_dependency(material_shader, this);

		auto vulkan_material = material.as<VulkanMaterial>();
		uniform_storage_buffer = Buffer::copy(vulkan_material->uniform_storage_buffer.data, vulkan_material->uniform_storage_buffer.size);

		resident_descriptors = vulkan_material->resident_descriptors;
		resident_descriptors_array = vulkan_material->resident_descriptors_array;
		pending_descriptors = vulkan_material->pending_descriptors;
		material_textures = vulkan_material->material_textures;
		texture_arrays = vulkan_material->texture_arrays;
		images = vulkan_material->images;
	}

	VulkanMaterial::~VulkanMaterial() { uniform_storage_buffer.release(); }

	void VulkanMaterial::init()
	{
		allocate_storage();

		material_flags |= (uint32_t)MaterialFlag::DepthTest;
		material_flags |= (uint32_t)MaterialFlag::Blend;

		Renderer::submit([this]() mutable { this->invalidate(); });
	}

	void VulkanMaterial::invalidate()
	{
		auto shader = material_shader.as<VulkanShader>();
		if (shader->has_descriptor_set(0)) {
			const auto& shader_descriptor_sets = shader->get_shader_descriptor_sets();
			if (!shader_descriptor_sets.empty()) {
				for (auto&& [binding, descriptor] : resident_descriptors)
					pending_descriptors.push_back(descriptor);
			}
		}
	}

	void VulkanMaterial::allocate_storage()
	{
		const auto& shader_buffers = material_shader->get_shader_buffers();

		if (shader_buffers.size() > 0) {
			uint32_t size = 0;
			for (auto [name, shaderBuffer] : shader_buffers)
				size += shaderBuffer.Size;

			uniform_storage_buffer.allocate(size);
			uniform_storage_buffer.zero_initialise();
		}
	}

	void VulkanMaterial::on_shader_reloaded()
	{
		std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptor>> new_descriptors;
		std::unordered_map<uint32_t, std::shared_ptr<PendingDescriptorArray>> new_descriptor_arrays;
		for (auto [name, resource] : material_shader->get_resources()) {
			const uint32_t binding = resource.get_register();

			if (resident_descriptors.find(binding) != resident_descriptors.end())
				new_descriptors[binding] = std::move(resident_descriptors.at(binding));
			else if (resident_descriptors_array.find(binding) != resident_descriptors_array.end())
				new_descriptor_arrays[binding] = std::move(resident_descriptors_array.at(binding));
		}
		resident_descriptors = std::move(new_descriptors);
		resident_descriptors_array = std::move(new_descriptor_arrays);

		invalidate();
		invalidate_descriptor_sets();
	}

	const ShaderUniform* VulkanMaterial::find_uniform_declaration(const std::string& name)
	{
		const auto& shader_buffers = material_shader->get_shader_buffers();

		core_assert(shader_buffers.size() <= 1, "We currently only support ONE material buffer!");

		if (shader_buffers.size() > 0) {
			const ShaderBuffer& buffer = (*shader_buffers.begin()).second;
			if (buffer.Uniforms.find(name) == buffer.Uniforms.end())
				return nullptr;

			return &buffer.Uniforms.at(name);
		}
		return nullptr;
	}

	const ShaderResourceDeclaration* VulkanMaterial::find_resource_declaration(const std::string& name)
	{
		auto& resources = material_shader->get_resources();
		if (resources.find(name) != resources.end())
			return &resources.at(name);

		return nullptr;
	}

	void VulkanMaterial::set_vulkan_descriptor(const std::string& name, const Reference<Texture2D>& texture)
	{
		const ShaderResourceDeclaration* resource = find_resource_declaration(name);
		core_assert_bool(resource);

		uint32_t binding = resource->get_register();

		// Texture is already set
		// TODO(Karim): Shouldn't need to check resident descriptors..
		if (binding < material_textures.size() && material_textures[binding] && texture->get_hash() == material_textures[binding]->get_hash()
			&& resident_descriptors.find(binding) != resident_descriptors.end())
			return;

		if (binding >= material_textures.size())
			material_textures.resize(binding + 1);
		material_textures[binding] = texture;

		const VkWriteDescriptorSet* wds = material_shader.as<VulkanShader>()->get_descriptor_set(name);
		core_assert_bool(wds);
		resident_descriptors[binding]
			= std::make_shared<PendingDescriptor>(PendingDescriptor { PendingDescriptorType::Texture2D, *wds, {}, texture.as<Texture>(), nullptr });
		pending_descriptors.push_back(resident_descriptors.at(binding));

		invalidate_descriptor_sets();
	}

	void VulkanMaterial::set_vulkan_descriptor(const std::string& name, const Reference<Texture2D>& texture, uint32_t array_index)
	{
		const ShaderResourceDeclaration* resource = find_resource_declaration(name);
		core_assert_bool(resource);

		uint32_t binding = resource->get_register();
		// Texture is already set
		if (binding < texture_arrays.size() && texture_arrays[binding].size() < array_index
			&& texture->get_hash() == texture_arrays[binding][array_index]->get_hash())
			return;

		if (binding >= texture_arrays.size())
			texture_arrays.resize(binding + 1);

		if (array_index >= texture_arrays[binding].size())
			texture_arrays[binding].resize(array_index + 1);

		texture_arrays[binding][array_index] = texture;

		const VkWriteDescriptorSet* wds = material_shader.as<VulkanShader>()->get_descriptor_set(name);
		core_assert_bool(wds);
		if (resident_descriptors_array.find(binding) == resident_descriptors_array.end()) {
			resident_descriptors_array[binding]
				= std::make_shared<PendingDescriptorArray>(PendingDescriptorArray { PendingDescriptorType::Texture2D, *wds, {}, {}, {} });
		}

		auto& resident_descriptor_array = resident_descriptors_array.at(binding);
		if (array_index >= resident_descriptor_array->textures.size())
			resident_descriptor_array->textures.resize(array_index + 1);

		resident_descriptor_array->textures[array_index] = texture;

		// pending_descriptors.push_back(resident_descriptors.at(binding));

		invalidate_descriptor_sets();
	}

	void VulkanMaterial::set_vulkan_descriptor(const std::string& name, const Reference<TextureCube>& texture)
	{
		const ShaderResourceDeclaration* resource = find_resource_declaration(name);
		core_assert_bool(resource);

		uint32_t binding = resource->get_register();
		// Texture is already set
		// TODO(Karim): Shouldn't need to check resident descriptors..
		if (binding < material_textures.size() && material_textures[binding] && texture->get_hash() == material_textures[binding]->get_hash()
			&& resident_descriptors.find(binding) != resident_descriptors.end())
			return;

		if (binding >= material_textures.size())
			material_textures.resize(binding + 1);
		material_textures[binding] = texture;

		const VkWriteDescriptorSet* wds = material_shader.as<VulkanShader>()->get_descriptor_set(name);
		core_assert_bool(wds);
		resident_descriptors[binding]
			= std::make_shared<PendingDescriptor>(PendingDescriptor { PendingDescriptorType::TextureCube, *wds, {}, texture.as<Texture>(), nullptr });
		pending_descriptors.push_back(resident_descriptors.at(binding));

		invalidate_descriptor_sets();
	}

	void VulkanMaterial::set_vulkan_descriptor(const std::string& name, const Reference<Image2D>& image)
	{
		core_verify_bool(image);
		core_assert(image.as<VulkanImage2D>()->get_image_info().image_view, "ImageView is null");

		const ShaderResourceDeclaration* resource = find_resource_declaration(name);
		core_verify(resource, "Could not find resource with name {1} declared in shader {0}.", this->material_shader->get_name(), name);

		uint32_t binding = resource->get_register();
		if (binding < images.size() && images[binding] && resident_descriptors.find(binding) != resident_descriptors.end())
			return;

		if (resource->get_register() >= images.size())
			images.resize(resource->get_register() + 1);
		images[resource->get_register()] = image;

		const VkWriteDescriptorSet* wds = material_shader.as<VulkanShader>()->get_descriptor_set(name);
		core_assert_bool(wds);
		resident_descriptors[binding]
			= std::make_shared<PendingDescriptor>(PendingDescriptor { PendingDescriptorType::Image2D, *wds, {}, nullptr, image.as<Image>() });
		pending_descriptors.push_back(resident_descriptors.at(binding));

		invalidate_descriptor_sets();
	}

	void VulkanMaterial::set(const std::string& name, float value) { set<float>(name, value); }

	void VulkanMaterial::set(const std::string& name, int value) { set<int>(name, value); }

	void VulkanMaterial::set(const std::string& name, uint32_t value) { set<uint32_t>(name, value); }

	void VulkanMaterial::set(const std::string& name, bool value)
	{
		// Bools are 4-byte ints
		set<int>(name, (int)value);
	}

	void VulkanMaterial::set(const std::string& name, const glm::ivec2& value) { set<glm::ivec2>(name, value); }

	void VulkanMaterial::set(const std::string& name, const glm::ivec3& value) { set<glm::ivec3>(name, value); }

	void VulkanMaterial::set(const std::string& name, const glm::ivec4& value) { set<glm::ivec4>(name, value); }

	void VulkanMaterial::set(const std::string& name, const glm::vec2& value) { set<glm::vec2>(name, value); }

	void VulkanMaterial::set(const std::string& name, const glm::vec3& value) { set<glm::vec3>(name, value); }

	void VulkanMaterial::set(const std::string& name, const glm::vec4& value) { set<glm::vec4>(name, value); }

	void VulkanMaterial::set(const std::string& name, const glm::mat3& value) { set<glm::mat3>(name, value); }

	void VulkanMaterial::set(const std::string& name, const glm::mat4& value) { set<glm::mat4>(name, value); }

	void VulkanMaterial::set(const std::string& name, const Reference<Texture2D>& texture) { set_vulkan_descriptor(name, texture); }

	void VulkanMaterial::set(const std::string& name, const Reference<Texture2D>& texture, uint32_t array_index)
	{
		set_vulkan_descriptor(name, texture, array_index);
	}

	void VulkanMaterial::set(const std::string& name, const Reference<TextureCube>& texture) { set_vulkan_descriptor(name, texture); }

	void VulkanMaterial::set(const std::string& name, const Reference<Image2D>& image) { set_vulkan_descriptor(name, image); }

	float& VulkanMaterial::get_float(const std::string& name) { return get<float>(name); }

	int32_t& VulkanMaterial::get_int(const std::string& name) { return get<int32_t>(name); }

	uint32_t& VulkanMaterial::get_uint(const std::string& name) { return get<uint32_t>(name); }

	bool& VulkanMaterial::get_bool(const std::string& name) { return get<bool>(name); }

	glm::vec2& VulkanMaterial::get_vector2(const std::string& name) { return get<glm::vec2>(name); }

	glm::vec3& VulkanMaterial::get_vector3(const std::string& name) { return get<glm::vec3>(name); }

	glm::vec4& VulkanMaterial::get_vector4(const std::string& name) { return get<glm::vec4>(name); }

	glm::mat3& VulkanMaterial::get_matrix3(const std::string& name) { return get<glm::mat3>(name); }

	glm::mat4& VulkanMaterial::get_matrix4(const std::string& name) { return get<glm::mat4>(name); }

	Reference<Texture2D> VulkanMaterial::get_texture_2d(const std::string& name) { return get_resource<Texture2D>(name); }

	Reference<TextureCube> VulkanMaterial::get_texture_cube(const std::string& name) { return get_resource<TextureCube>(name); }

	Reference<Texture2D> VulkanMaterial::try_get_texture_2d(const std::string& name) { return try_get_resource<Texture2D>(name); }

	Reference<TextureCube> VulkanMaterial::try_get_texture_cube(const std::string& name) { return try_get_resource<TextureCube>(name); }

	void VulkanMaterial::rt_update_for_rendering(const std::vector<std::vector<VkWriteDescriptorSet>>& uniform_buffer_write_descriptors)
	{
		auto vulkan_device = VulkanContext::get_current_device()->get_vulkan_device();
		for (auto&& [binding, descriptor] : resident_descriptors) {
			if (descriptor->type == PendingDescriptorType::Image2D) {
				Reference<VulkanImage2D> image = descriptor->image.as<VulkanImage2D>();
				core_assert(image->get_image_info().image_view, "ImageView is null", "");
				if (descriptor->wds.pImageInfo && image->get_image_info().image_view != descriptor->wds.pImageInfo->imageView) {
					// HZ_CORE_WARN("Found out of date Image2D descriptor ({0} vs. {1})",
					// (void*)image->get_image_info().image_view, (void*)descriptor->wds.pImageInfo->imageView);
					pending_descriptors.emplace_back(descriptor);
					invalidate_descriptor_sets();
				}
			}
		}

		std::vector<VkDescriptorImageInfo> array_image_infos;

		uint32_t frame_index = Renderer::get_current_frame_index();
		if (dirty_descriptor_sets[frame_index] || true) {
			dirty_descriptor_sets[frame_index] = false;
			write_descriptors[frame_index].clear();

			if (!uniform_buffer_write_descriptors.empty()) {
				for (auto& wd : uniform_buffer_write_descriptors[frame_index])
					write_descriptors[frame_index].push_back(wd);
			}

			for (auto&& [binding, pd] : resident_descriptors) {
				if (pd->type == PendingDescriptorType::Texture2D) {
					Reference<VulkanTexture2D> texture = pd->texture.as<VulkanTexture2D>();
					pd->image_info = texture->get_vulkan_descriptor_info();
					pd->wds.pImageInfo = &pd->image_info;
				} else if (pd->type == PendingDescriptorType::TextureCube) {
					Reference<VulkanTextureCube> texture = pd->texture.as<VulkanTextureCube>();
					pd->image_info = texture->get_vulkan_descriptor_info();
					pd->wds.pImageInfo = &pd->image_info;
				} else if (pd->type == PendingDescriptorType::Image2D) {
					Reference<VulkanImage2D> image = pd->image.as<VulkanImage2D>();
					pd->image_info = image->get_descriptor_info();
					pd->wds.pImageInfo = &pd->image_info;
				}

				write_descriptors[frame_index].push_back(pd->wds);
			}

			for (auto&& [binding, pd] : resident_descriptors_array) {
				if (pd->type == PendingDescriptorType::Texture2D) {
					for (const auto& tex : pd->textures) {
						if (tex) {
							Reference<VulkanTexture2D> texture = tex.as<VulkanTexture2D>();
							array_image_infos.emplace_back(texture->get_vulkan_descriptor_info());
						}
					}
				}
				pd->wds.pImageInfo = array_image_infos.data();
				pd->wds.descriptorCount = (uint32_t)array_image_infos.size();
				write_descriptors[frame_index].push_back(pd->wds);
			}
		}

		auto vulkan_shader = material_shader.as<VulkanShader>();
		auto descriptor_set = vulkan_shader->allocate_descriptor_set(0);
		descriptor_sets[frame_index] = descriptor_set;
		for (auto& write_descriptor : write_descriptors[frame_index])
			write_descriptor.dstSet = descriptor_set.descriptor_sets[0];

		vkUpdateDescriptorSets(vulkan_device, (uint32_t)write_descriptors[frame_index].size(), write_descriptors[frame_index].data(), 0, nullptr);
		pending_descriptors.clear();
	}

	void VulkanMaterial::invalidate_descriptor_sets()
	{
		const uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;
		for (uint32_t i = 0; i < frames_in_flight; i++)
			dirty_descriptor_sets[i] = true;
	}

} // namespace ForgottenEngine
