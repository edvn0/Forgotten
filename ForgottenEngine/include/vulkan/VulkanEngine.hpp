//
// Created by Edwin Carlsson on 2022-07-02.
//

#pragma once

#include <utility>

#include "Common.hpp"

#include "DeletionQueue.hpp"
#include "Shader.hpp"
#include "VulkanContext.hpp"
#include "VulkanMesh.hpp"

struct GLFWwindow;
typedef struct VmaAllocator_T* VmaAllocator;

namespace ForgottenEngine {

class VulkanEngine {
private:
	struct SwapchainImage {
		VkImage image;
		VkImageView view;
	};

	struct WindowSpecification {
		uint32_t w, h;
		std::string name;
	};

private:
	WindowSpecification spec;

	VmaAllocator allocator;

	int frame_number{ 0 };

	VkExtent2D window_extent{ 800, 900 };

	std::vector<SwapchainImage> swapchain_images;
	VkSwapchainKHR swapchain{ nullptr };
	VkFormat swapchain_image_format{};

	VkImageView depth_view;
	AllocatedImage depth_image;
	VkFormat depth_format;

	VkCommandPool command_pool{ nullptr };
	VkCommandBuffer main_command_buffer{ nullptr };

	VkRenderPass render_pass{ nullptr };
	std::vector<VkFramebuffer> framebuffers{};

	VkSemaphore present_sema{ nullptr };
	VkSemaphore render_sema{ nullptr };
	VkFence render_fence{ nullptr };

	DeletionQueue cleanup_queue;

	// Temporary objects
	std::unique_ptr<Shader> triangle_vertex;
	std::unique_ptr<Shader> triangle_fragment;
	std::unique_ptr<Shader> triangle_coloured_vertex;
	std::unique_ptr<Shader> triangle_coloured_fragment;
	VkPipelineLayout triangle_layout;
	VkPipeline triangle_pipeline;
	VkPipeline coloured_triangle_pipeline;

	std::unique_ptr<Shader> mesh_vertex;
	std::unique_ptr<Shader> mesh_fragment;
	VkPipeline mesh_pipeline;
	VkPipelineLayout mesh_layout;
	DynamicMesh triangle_mesh;

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
	bool upload_mesh(DynamicMesh& mesh);

	template <size_t T> bool upload_fixed_size_mesh(FixedSizeMesh<T>& mesh)
	{
		upload_mesh({ .vertices = mesh.vertices, .vertex_buffer = mesh.vertex_buffer });
	};

	void render_and_present();
};

} // ForgottenEngine