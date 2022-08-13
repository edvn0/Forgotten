//
// Created by Edwin Carlsson on 2022-07-04.
//

#pragma once

#include <vulkan/vulkan.h>

// VI = Vulkan Initializer
namespace ForgottenEngine::VI {

	VkCommandPoolCreateInfo command_pool_create_info(uint32_t family_index, VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo command_buffer_allocate_info(
		VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

	namespace Pipeline {

		VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(
			VkShaderStageFlagBits stage, VkShaderModule module, const char* entrypoint_name = "main");

		VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

		VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);

		VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);

		VkPipelineMultisampleStateCreateInfo multisampling_state_create_info();

		VkPipelineColorBlendAttachmentState color_blend_attachment_state();

		VkPipelineLayoutCreateInfo pipeline_layout_create_info();

		VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool depth_test, bool depth_write, VkCompareOp compare_op);

	} // namespace Pipeline

	namespace Image {

		VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent);

		VkImageViewCreateInfo image_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags);

		VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

		VkWriteDescriptorSet write_descriptor_image(
			VkDescriptorType type, VkDescriptorSet dst_set, VkDescriptorImageInfo* image_info, uint32_t binding);

	} // namespace Image

	namespace Descriptor {

		VkDescriptorSetLayoutBinding descriptor_set_layout_binding(VkDescriptorType type, VkShaderStageFlags stage_flags, uint32_t binding);

		VkWriteDescriptorSet write_descriptor_buffer(
			VkDescriptorType type, VkDescriptorSet dst_set, VkDescriptorBufferInfo* buffer_info, uint32_t binding);

	} // namespace Descriptor

	namespace Upload {

		VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);

		VkSubmitInfo submit_info(VkCommandBuffer* cmd);

	} // namespace Upload

} // namespace ForgottenEngine::VI