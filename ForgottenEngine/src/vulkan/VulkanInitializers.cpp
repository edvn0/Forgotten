//
// Created by Edwin Carlsson on 2022-07-04.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanInitializers.hpp"

namespace ForgottenEngine::VI {

	VkCommandPoolCreateInfo command_pool_create_info(uint32_t family_index, VkCommandPoolCreateFlags flags)
	{
		VkCommandPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.pNext = nullptr;

		info.queueFamilyIndex = family_index;
		info.flags = flags;
		return info;
	}

	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level)
	{
		VkCommandBufferAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.pNext = nullptr;

		info.commandPool = pool;
		info.commandBufferCount = count;
		info.level = level;
		return info;
	}

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags)
	{
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.pNext = nullptr;
		fenceCreateInfo.flags = flags;
		return fenceCreateInfo;
	}

	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags)
	{
		VkSemaphoreCreateInfo semCreateInfo = {};
		semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semCreateInfo.pNext = nullptr;
		semCreateInfo.flags = flags;
		return semCreateInfo;
	}

	VkPipelineShaderStageCreateInfo Pipeline::pipeline_shader_stage_create_info(
		VkShaderStageFlagBits stage, VkShaderModule module, const char* entrypoint_name)
	{
		VkPipelineShaderStageCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;

		// shader stage
		info.stage = stage;
		// module containing the code for this shader stage
		info.module = module;
		// the entry point of the shader
		info.pName = entrypoint_name;
		return info;
	}

	VkPipelineVertexInputStateCreateInfo Pipeline::vertex_input_state_create_info()
	{
		VkPipelineVertexInputStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		info.pNext = nullptr;

		// no vertex bindings or attributes
		info.vertexBindingDescriptionCount = 0;
		info.vertexAttributeDescriptionCount = 0;
		return info;
	}
	VkPipelineInputAssemblyStateCreateInfo Pipeline::input_assembly_create_info(VkPrimitiveTopology topology)
	{
		VkPipelineInputAssemblyStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.topology = topology;
		// we are not going to use primitive restart on the entire tutorial so leave it on false
		info.primitiveRestartEnable = VK_FALSE;
		return info;
	}

	VkPipelineRasterizationStateCreateInfo Pipeline::rasterization_state_create_info(VkPolygonMode polygonMode)
	{
		VkPipelineRasterizationStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.depthClampEnable = VK_FALSE;
		// discards all primitives before the rasterization stage if enabled which we don't want
		info.rasterizerDiscardEnable = VK_FALSE;

		info.polygonMode = polygonMode;
		info.lineWidth = 1.0f;
		// no backface cull
		info.cullMode = VK_CULL_MODE_NONE;
		info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		// no depth bias
		info.depthBiasEnable = VK_FALSE;
		info.depthBiasConstantFactor = 0.0f;
		info.depthBiasClamp = 0.0f;
		info.depthBiasSlopeFactor = 0.0f;

		return info;
	}

	VkPipelineMultisampleStateCreateInfo Pipeline::multisampling_state_create_info()
	{
		VkPipelineMultisampleStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.sampleShadingEnable = VK_FALSE;
		// multisampling defaulted to no multisampling (1 sample per pixel)
		info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		info.minSampleShading = 1.0f;
		info.pSampleMask = nullptr;
		info.alphaToCoverageEnable = VK_FALSE;
		info.alphaToOneEnable = VK_FALSE;
		return info;
	}

	VkPipelineColorBlendAttachmentState Pipeline::color_blend_attachment_state()
	{
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask
			= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		return colorBlendAttachment;
	}

	VkPipelineLayoutCreateInfo Pipeline::pipeline_layout_create_info()
	{
		VkPipelineLayoutCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.pNext = nullptr;

		// empty defaults
		info.flags = 0;
		info.setLayoutCount = 0;
		info.pSetLayouts = nullptr;
		info.pushConstantRangeCount = 0;
		info.pPushConstantRanges = nullptr;
		return info;
	}

	VkImageCreateInfo Image::image_create_info(VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent)
	{
		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;

		info.imageType = VK_IMAGE_TYPE_2D;

		info.format = format;
		info.extent = extent;

		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = usage_flags;

		return info;
	}

	VkImageViewCreateInfo Image::image_view_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspect_flags)
	{
		// build a image-view for the depth image to use for rendering
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.pNext = nullptr;

		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.image = image;
		info.format = format;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;
		info.subresourceRange.aspectMask = aspect_flags;

		return info;
	}

	// implementation
	VkSamplerCreateInfo Image::sampler_create_info(VkFilter filters, VkSamplerAddressMode sampler_address_mode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/)
	{
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.pNext = nullptr;

		info.magFilter = filters;
		info.minFilter = filters;
		info.addressModeU = sampler_address_mode;
		info.addressModeV = sampler_address_mode;
		info.addressModeW = sampler_address_mode;

		return info;
	}
	VkWriteDescriptorSet Image::write_descriptor_image(
		VkDescriptorType type, VkDescriptorSet dst_set, VkDescriptorImageInfo* image_info, uint32_t binding)
	{
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstBinding = binding;
		write.dstSet = dst_set;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pImageInfo = image_info;

		return write;
	}

	VkPipelineDepthStencilStateCreateInfo Pipeline::depth_stencil_create_info(bool depth_test, bool depth_write, VkCompareOp compare_op)
	{
		VkPipelineDepthStencilStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.depthTestEnable = depth_test ? VK_TRUE : VK_FALSE;
		info.depthWriteEnable = depth_write ? VK_TRUE : VK_FALSE;
		info.depthCompareOp = depth_test ? compare_op : VK_COMPARE_OP_ALWAYS;
		info.depthBoundsTestEnable = VK_FALSE;
		info.minDepthBounds = 0.0f; // Optional
		info.maxDepthBounds = 1.0f; // Optional
		info.stencilTestEnable = VK_FALSE;

		return info;
	}

	VkDescriptorSetLayoutBinding Descriptor::descriptor_set_layout_binding(VkDescriptorType type, VkShaderStageFlags stage_flags, uint32_t binding)
	{
		VkDescriptorSetLayoutBinding info = {};
		info.binding = binding;
		info.descriptorCount = 1;
		info.descriptorType = type;
		info.pImmutableSamplers = nullptr;
		info.stageFlags = stage_flags;

		return info;
	}

	VkWriteDescriptorSet Descriptor::write_descriptor_buffer(
		VkDescriptorType type, VkDescriptorSet dst_set, VkDescriptorBufferInfo* buffer_info, uint32_t binding)
	{
		VkWriteDescriptorSet info = {};
		info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		info.pNext = nullptr;

		info.dstBinding = binding;
		info.dstSet = dst_set;
		info.descriptorCount = 1;
		info.descriptorType = type;
		info.pBufferInfo = buffer_info;

		return info;
	}

	VkCommandBufferBeginInfo Upload::command_buffer_begin_info(VkCommandBufferUsageFlags flags)
	{
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.pNext = nullptr;

		info.pInheritanceInfo = nullptr;
		info.flags = flags;
		return info;
	}

	VkSubmitInfo Upload::submit_info(VkCommandBuffer* cmd)
	{
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.pNext = nullptr;

		info.waitSemaphoreCount = 0;
		info.pWaitSemaphores = nullptr;
		info.pWaitDstStageMask = nullptr;
		info.commandBufferCount = 1;
		info.pCommandBuffers = cmd;
		info.signalSemaphoreCount = 0;
		info.pSignalSemaphores = nullptr;

		return info;
	}
} // namespace ForgottenEngine::VI