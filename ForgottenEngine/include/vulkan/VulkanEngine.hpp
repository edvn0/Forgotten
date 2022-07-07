//
// Created by Edwin Carlsson on 2022-07-02.
//

#pragma once

#include <utility>

#include "Common.hpp"

#include "DeletionQueue.hpp"
#include "Shader.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanContext.hpp"
#include "VulkanMesh.hpp"
#include "VulkanMeshLibrary.hpp"
#include "VulkanUBO.hpp"
#include "VulkanUploadContext.hpp"

#include "vk_mem_alloc.h"
#include <GLFW/glfw3.h>

namespace ForgottenEngine {

static constexpr uint FRAME_OVERLAP = 3;

class VulkanEngine {
private:
	struct SwapchainImage {
		VkImage image;
		VkImageView view;
	};

	struct SceneParameters {
		SceneUBO parameters;
		AllocatedBuffer parameter_buffer;
	};

	struct ObjectData {
		glm::mat4 model_matrix;
	};

	struct FrameInFlight {
		VkSemaphore present_sema;
		VkSemaphore render_sema;
		VkFence render_fence;

		VkCommandPool command_pool;
		VkCommandBuffer main_command_buffer;

		// UBO info
		AllocatedBuffer camera_buffer;
		VkDescriptorSet global_descriptor;

		// Storage buffers
		AllocatedBuffer object_buffer;
		VkDescriptorSet object_descriptor;
	};

	struct WindowSpecification {
		uint32_t w, h;
		std::string name;
	};

private:
	WindowSpecification spec;

	VmaAllocator allocator;

	int frame_number{ 0 };

	VkDescriptorSetLayout global_set_layout;
	VkDescriptorSetLayout object_set_layout;
	VkDescriptorPool descriptor_pool;

	std::array<FrameInFlight, FRAME_OVERLAP> frames_in_flight;
	inline auto& frame() { return frames_in_flight[frame_number % FRAME_OVERLAP]; }

	VkExtent2D window_extent{ 800, 450 };

	std::vector<SwapchainImage> swapchain_images;
	VkSwapchainKHR swapchain{ nullptr };
	VkFormat swapchain_image_format{};

	VkImageView depth_view;
	AllocatedImage depth_image;
	VkFormat depth_format;

	VkRenderPass render_pass{ nullptr };
	std::vector<VkFramebuffer> framebuffers{};

	VulkanMeshLibrary library;
	DeletionQueue cleanup_queue;

	std::vector<VulkanRenderObject> renderables;

	SceneUBO scene_parameters;
	AllocatedBuffer scene_parameter_buffer;

	UploadContext upload_context;

	// Temporary objects
	std::unique_ptr<Shader> mesh_vertex;
	std::unique_ptr<Shader> mesh_fragment;
	VkPipeline mesh_pipeline;
	VkPipelineLayout mesh_layout;
	std::unique_ptr<Mesh> monkey_mesh;

	int chosen_shader{ 0 };

public:
	explicit VulkanEngine(WindowSpecification spec)
		: spec(std::move(spec))
	{
		Logger::init();
		VulkanContext::construct_and_initialize();
	}

	bool initialize();

	void run();

	void cleanup();

private:
	bool init_swapchain();
	bool init_commands();
	bool init_default_renderpass();
	bool init_framebuffers();
	bool init_sync_structures();
	bool init_shader();
	bool init_pipelines();
	bool init_vma();
	bool init_meshes();
	bool init_scene();
	bool init_descriptors();

	void render_and_present();
	void draw_renderables(VkCommandBuffer cmd);
};

} // ForgottenEngine