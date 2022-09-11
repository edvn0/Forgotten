#pragma once

#include "AABB.hpp"
#include "Asset.hpp"
#include "render/IndexBuffer.hpp"
#include "render/MaterialAsset.hpp"
#include "render/Pipeline.hpp"
#include "render/Shader.hpp"
#include "render/VertexBuffer.hpp"

#include <glm/glm.hpp>
#include <vector>

struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiScene;

namespace Assimp {
	class Importer;
}

namespace ForgottenEngine {

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec3 Binormal;
		glm::vec2 Texcoord;
	};

	static const int NumAttributes = 5;

	struct Index {
		uint32_t V1, V2, V3;
	};

	static_assert(sizeof(Index) == 3 * sizeof(uint32_t));

	struct Triangle {
		Vertex V0, V1, V2;

		Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2)
			: V0(v0)
			, V1(v1)
			, V2(v2)
		{
		}
	};

	class Submesh {
	public:
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		uint32_t MaterialIndex;
		uint32_t IndexCount;
		uint32_t VertexCount;

		glm::mat4 Transform { 1.0f }; // World transform
		glm::mat4 LocalTransform { 1.0f };
		AABB BoundingBox;

		std::string NodeName, MeshName;
	};

	//
	// MeshSource is a representation of an actual asset file on disk
	// Meshes are created from MeshSource
	//
	class MeshSource : public Asset {
	public:
		MeshSource(const std::string& filename);
		MeshSource(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const glm::mat4& transform);
		MeshSource(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const std::vector<Submesh>& submeshes);
		virtual ~MeshSource();

		void DumpVertexBuffer();

		std::vector<Submesh>& GetSubmeshes() { return m_Submeshes; }
		const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }

		const std::vector<Vertex>& GetVertices() const { return m_StaticVertices; }
		const std::vector<Index>& GetIndices() const { return m_Indices; }

		Reference<Shader> GetMeshShader() { return m_MeshShader; }
		std::vector<Reference<Material>>& GetMaterials() { return m_Materials; }
		const std::vector<Reference<Material>>& GetMaterials() const { return m_Materials; }
		const std::vector<Reference<Texture2D>>& GetTextures() const { return m_Textures; }
		const std::string& GetFilePath() const { return m_FilePath; }

		const std::vector<Triangle> GetTriangleCache(uint32_t index) const { return triangle_cache.at(index); }

		Reference<VertexBuffer> GetVertexBuffer() { return m_VertexBuffer; }
		Reference<IndexBuffer> GetIndexBuffer() { return m_IndexBuffer; }
		const VertexBufferLayout& GetVertexBufferLayout() const { return m_VertexBufferLayout; }

		static AssetType get_static_type() { return AssetType::MeshSource; }
		AssetType get_asset_type() const override { return get_static_type(); }

		const AABB& GetBoundingBox() const { return m_BoundingBox; }

	private:
		void TraverseNodes(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);

	private:
		std::vector<Submesh> m_Submeshes;

		std::unique_ptr<Assimp::Importer> m_Importer;

		glm::mat4 m_InverseTransform;

		Reference<VertexBuffer> m_VertexBuffer;
		Reference<IndexBuffer> m_IndexBuffer;
		VertexBufferLayout m_VertexBufferLayout;

		std::vector<Vertex> m_StaticVertices;
		std::vector<Index> m_Indices;
		std::unordered_map<aiNode*, std::vector<uint32_t>> m_NodeMap;
		const aiScene* m_Scene;

		// Materials
		Reference<Shader> m_MeshShader;
		std::vector<Reference<Texture2D>> m_Textures;
		std::vector<Reference<Texture2D>> m_NormalMaps;
		std::vector<Reference<Material>> m_Materials;

		std::unordered_map<uint32_t, std::vector<Triangle>> triangle_cache;

		AABB m_BoundingBox;

		// Animation
		bool m_IsAnimated = false;
		float m_TimeMultiplier = 1.0f;
		bool m_AnimationPlaying = true;

		std::string m_FilePath;

		friend class Scene;
		friend class Renderer;
		friend class VulkanRenderer;
		friend class SceneHierarchyPanel;
		friend class MeshViewerPanel;
	};

	// Dynamic Mesh - supports skeletal animation and retains hierarchy
	class Mesh : public Asset {
	public:
		explicit Mesh(Reference<MeshSource> meshSource);
		Mesh(Reference<MeshSource> meshSource, const std::vector<uint32_t>& submeshes);
		Mesh(const Reference<Mesh>& other);
		virtual ~Mesh();

		std::vector<uint32_t>& GetSubmeshes() { return m_Submeshes; }
		const std::vector<uint32_t>& GetSubmeshes() const { return m_Submeshes; }

		// Pass in an empty vector to set ALL submeshes for MeshSource
		void SetSubmeshes(const std::vector<uint32_t>& submeshes);

		Reference<MeshSource> GetMeshSource() { return m_MeshSource; }
		Reference<MeshSource> GetMeshSource() const { return m_MeshSource; }
		void SetMeshAsset(Reference<MeshSource> meshAsset) { m_MeshSource = meshAsset; }

		Reference<MaterialTable> GetMaterials() const { return m_Materials; }

		static AssetType get_static_type() { return AssetType::Mesh; }
		AssetType get_asset_type() const override { return get_static_type(); }

	private:
		Reference<MeshSource> m_MeshSource;
		std::vector<uint32_t> m_Submeshes; // TODO(Yan): physics/render masks

		// Materials
		Reference<MaterialTable> m_Materials;

		friend class Scene;
		friend class Renderer;
		friend class VulkanRenderer;
		friend class SceneHierarchyPanel;
		friend class MeshViewerPanel;
	};

	// Static Mesh - no skeletal animation, flattened hierarchy
	class StaticMesh : public Asset {
	public:
		explicit StaticMesh(Reference<MeshSource> meshSource);
		StaticMesh(Reference<MeshSource> meshSource, const std::vector<uint32_t>& submeshes);
		StaticMesh(const Reference<StaticMesh>& other);
		virtual ~StaticMesh();

		std::vector<uint32_t>& GetSubmeshes() { return m_Submeshes; }
		const std::vector<uint32_t>& GetSubmeshes() const { return m_Submeshes; }

		// Pass in an empty vector to set ALL submeshes for MeshSource
		void SetSubmeshes(const std::vector<uint32_t>& submeshes);

		Reference<MeshSource> GetMeshSource() { return m_MeshSource; }
		Reference<MeshSource> GetMeshSource() const { return m_MeshSource; }
		void SetMeshAsset(Reference<MeshSource> meshAsset) { m_MeshSource = meshAsset; }

		Reference<MaterialTable> GetMaterials() const { return m_Materials; }

		static AssetType get_static_type() { return AssetType::StaticMesh; }
		AssetType get_asset_type() const override { return get_static_type(); }

	private:
		Reference<MeshSource> m_MeshSource;
		std::vector<uint32_t> m_Submeshes; // TODO(Yan): physics/render masks

		// Materials
		Reference<MaterialTable> m_Materials;

		friend class Scene;
		friend class Renderer;
		friend class VulkanRenderer;
		friend class SceneHierarchyPanel;
		friend class MeshViewerPanel;
	};

} // namespace ForgottenEngine
