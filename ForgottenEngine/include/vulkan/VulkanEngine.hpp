//
// Created by Edwin Carlsson on 2022-07-02.
//

#pragma once

#include <utility>

#include "Common.hpp"

#include "ApplicationProperties.hpp"
#include "DeletionQueue.hpp"
#include "Layer.hpp"
#include "Shader.hpp"
#include "VulkanAssetLibrary.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanContext.hpp"
#include "VulkanMesh.hpp"
#include "VulkanUBO.hpp"
#include "VulkanUploadContext.hpp"

#include "imgui/ImGuiLayer.hpp"
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

private:
	ApplicationProperties spec;

	VmaAllocator allocator;

	int frame_number{ 0 };

	VkDescriptorSetLayout global_set_layout;
	VkDescriptorSetLayout object_set_layout;
	VkDescriptorSetLayout texture_set_layout;
	VkDescriptorPool descriptor_pool;

	std::array<FrameInFlight, FRAME_OVERLAP> frames_in_flight;
	inline auto& frame() { return frames_in_flight[frame_number % FRAME_OVERLAP]; }

	VkExtent2D window_extent{ 1500, 850 };

	std::vector<SwapchainImage> swapchain_images;
	VkSwapchainKHR swapchain{ nullptr };
	VkFormat swapchain_image_format{};

	VkImageView depth_view;
	AllocatedImage depth_image;
	VkFormat depth_format;

	VkRenderPass render_pass{ nullptr };
	std::vector<VkFramebuffer> framebuffers{};

	VulkanAssetLibrary library;
	DeletionQueue cleanup_queue;

	std::vector<VulkanRenderObject> renderables;

	SceneUBO scene_parameters;
	AllocatedBuffer scene_parameter_buffer;

	UploadContext upload_context;

	// Temporary objects
	float frame_time;
	std::unique_ptr<Mesh> monkey_mesh;

	double last_time;
	double last_frame_time;
	int nr_frames;

public:
	explicit VulkanEngine(ApplicationProperties spec)
		: spec(std::move(spec)){};

	bool initialize();

	void update(const TimeStep& step);

	void cleanup();

private:
	void init_swapchain();
	void init_commands();
	void init_default_renderpass();
	void init_framebuffers();
	void init_sync_structures();
	void init_shader();
	void init_pipelines();
	void init_vma();
	void init_meshes();
	void init_scene();
	void init_descriptors();

	void render_and_present(const TimeStep& step);
	void draw_renderables(VkCommandBuffer cmd);
};

} // ForgottenEngine