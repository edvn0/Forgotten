#pragma once

#include "TimeStep.hpp"
#include "UUID.hpp"
#include "render/Camera.hpp"
#include "render/Material.hpp"
#include "render/RenderCommandBuffer.hpp"
#include "render/Renderer2D.hpp"
#include "render/SceneEnvironment.hpp"
#include "render/Texture.hpp"
#include "scene/SceneCamera.hpp"
#include "user/UserCamera.hpp"

#include <entt/entt.hpp>

namespace ForgottenEngine {

	class SceneRenderer;
	class Prefab;

	struct DirLight {
		glm::vec3 Direction = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Radiance = { 0.0f, 0.0f, 0.0f };

		float Intensity = 1.0f;
	};

	struct DirectionalLight {
		glm::vec3 Direction = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Radiance = { 0.0f, 0.0f, 0.0f };
		float Intensity = 0.0f;
		float ShadowAmount = 1.0f;
		// C++ only
		bool CastShadows = true;
	};

	struct PointLight {
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		float Intensity = 0.0f;
		glm::vec3 Radiance = { 0.0f, 0.0f, 0.0f };
		float MinRadius = 0.001f;
		float Radius = 25.0f;
		float Falloff = 1.f;
		float SourceSize = 0.1f;
		bool CastsShadows = true;
		char Padding[3] { 0, 0, 0 };
	};

	struct SpotLight {
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		float Intensity = 0.0f;
		glm::vec3 Direction = { 0.0f, 0.0f, 0.0f };
		float AngleAttenuation = 0.0f;
		glm::vec3 Radiance = { 0.0f, 0.0f, 0.0f };
		float Range = 0.1f;
		float Angle = 0.0f;
		float Falloff = 1.0f;
		bool SoftShadows = true;
		char Padding0[3] { 0, 0, 0 };
		bool CastsShadows = true;
		char Padding1[3] { 0, 0, 0 };
	};

	struct LightEnvironment {
		DirectionalLight DirectionalLights[4];
		std::vector<PointLight> PointLights;
		std::vector<SpotLight> SpotLights;
		[[nodiscard]] uint32_t GetPointLightsSize() const { return (uint32_t)(PointLights.size() * sizeof PointLight); }
		[[nodiscard]] uint32_t GetSpotLightsSize() const { return (uint32_t)(SpotLights.size() * sizeof SpotLight); }
	};

	class Entity;
	using EntityMap = std::unordered_map<UUID, Entity>;

	struct TransformComponent;

	class PhysicsScene;

	class Scene : public Asset {
	public:
		Scene(const std::string& name = "UntitledScene", bool isEditorScene = false, bool initalize = true);
		~Scene();

		void Init();

		void OnUpdate(TimeStep ts);
		void OnRenderRuntime(Reference<SceneRenderer> renderer, TimeStep ts);
		void OnRenderEditor(Reference<SceneRenderer> renderer, TimeStep ts, const UserCamera& editorCamera);
		void OnRenderSimulation(Reference<SceneRenderer> renderer, TimeStep ts, const UserCamera& editorCamera);

		void RenderPhysicsDebug(Reference<SceneRenderer> renderer, bool runtime);

		void OnEvent(Event& e);

		// Runtime
		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnSimulationStart();
		void OnSimulationStop();

		void SetViewportSize(uint32_t width, uint32_t height);

		const Reference<SceneEnvironment>& GetEnvironment() const { return m_Environment; }

		DirLight& GetLight() { return m_Light; }
		const DirLight& GetLight() const { return m_Light; }

		Entity GetMainCameraEntity();

		float& GetSkyboxLod() { return m_SkyboxLod; }
		float GetSkyboxLod() const { return m_SkyboxLod; }

		Entity CreateEntity(const std::string& name = "");
		Entity CreateChildEntity(Entity parent, const std::string& name = "");
		Entity CreateEntityWithID(UUID uuid, const std::string& name = "", bool runtimeMap = false);
		void SubmitToDestroyEntity(Entity entity);
		void DestroyEntity(Entity entity, bool excludeChildren = false, bool first = true);

		void ResetTransformsToMesh(Entity entity, bool resetChildren);

		Entity DuplicateEntity(Entity entity);
		Entity CreatePrefabEntity(
			Entity entity, const glm::vec3* translation = nullptr, const glm::vec3* rotation = nullptr, const glm::vec3* scale = nullptr);
		Entity CreatePrefabEntity(Entity entity, Entity parent, const glm::vec3* translation = nullptr, const glm::vec3* rotation = nullptr,
			const glm::vec3* scale = nullptr);

		Entity Instantiate(
			Reference<Prefab> prefab, const glm::vec3* translation = nullptr, const glm::vec3* rotation = nullptr, const glm::vec3* scale = nullptr);
		Entity InstantiateMesh(Reference<Mesh> mesh, bool generateColliders);

		template <typename... Components> auto GetAllEntitiesWith() { return m_Registry.view<Components...>(); }

		// return entity with id as specified. entity is expected to exist (runtime error if it doesn't)
		Entity GetEntityWithUUID(UUID id) const;

		// return entity with id as specified, or empty entity if cannot be found - caller must check
		Entity TryGetEntityWithUUID(UUID id) const;

		// return entity with tag as specified, or empty entity if cannot be found - caller must check
		Entity TryGetEntityWithTag(const std::string& tag);

		void ConvertToLocalSpace(Entity entity);
		void ConvertToWorldSpace(Entity entity);
		glm::mat4 GetWorldSpaceTransformMatrix(Entity entity, bool accountForActor = false);
		TransformComponent GetWorldSpaceTransform(Entity entity);

		void ParentEntity(Entity entity, Entity parent);
		void UnparentEntity(Entity entity, bool convertToWorldSpace = true);

		void CopyTo(Reference<Scene>& target);

		UUID GetUUID() const { return m_SceneID; }

		static Reference<Scene> GetScene(UUID uuid);

		bool IsEditorScene() const { return m_IsEditorScene; }
		bool IsPlaying() const { return m_IsPlaying; }

		float GetPhysics2DGravity() const;
		void SetPhysics2DGravity(float gravity);

		Reference<PhysicsScene> GetPhysicsScene() const;

		void OnSceneTransition(const std::string& scene);

		static AssetType get_static_type() { return AssetType::Scene; }
		virtual AssetType get_asset_type() const override { return get_static_type(); }

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		void SetSceneTransitionCallback(const std::function<void(const std::string&)>& callback) { m_OnSceneTransitionCallback = callback; }
		void SetEntityDestroyedCallback(const std::function<void(Entity)>& callback) { m_OnEntityDestroyedCallback = callback; }

		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		const EntityMap& GetEntityMap() const { return m_EntityIDMap; }

		template <typename TComponent> void CopyComponentIfExists(entt::entity dst, entt::registry& dstRegistry, entt::entity src)
		{
			if (m_Registry.any_of<TComponent>(src)) {
				auto& srcComponent = m_Registry.get<TComponent>(src);
				dstRegistry.emplace_or_replace<TComponent>(dst, srcComponent);
			}
		}

		template <typename TComponent>
		static void CopyComponentFromScene(Entity dst, Reference<Scene> dstScene, Entity src, Reference<Scene> srcScene)
		{
			srcScene->CopyComponentIfExists<TComponent>((entt::entity)dst, dstScene->m_Registry, (entt::entity)src);
		}

	public:
		static Reference<Scene> CreateEmpty();

	private:
		void OnScriptComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnScriptComponentDestroy(entt::registry& registry, entt::entity entity);
		void OnAudioComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnAudioComponentDestroy(entt::registry& registry, entt::entity entity);
		void OnMeshColliderComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnMeshColliderComponentDestroy(entt::registry& registry, entt::entity entity);
		void OnRigidBody2DComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnRigidBody2DComponentDestroy(entt::registry& registry, entt::entity entity);
		void OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);
		void OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);

		void BuildMeshEntityHierarchy(Entity parent, Reference<Mesh> mesh, const void* assimpScene, void* assimpNode, bool generateColliders);

		void SortEntities();

		template <typename Fn> void SubmitPostUpdateFunc(Fn&& func) { m_PostUpdateQueue.emplace_back(func); }

	private:
		UUID m_SceneID;
		entt::entity m_SceneEntity = entt::null;
		entt::registry m_Registry;

		std::function<void(const std::string&)> m_OnSceneTransitionCallback;
		std::function<void(Entity)> m_OnEntityDestroyedCallback;

		std::string m_Name;
		bool m_IsEditorScene = false;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		EntityMap m_EntityIDMap;

		DirLight m_Light;
		float m_LightMultiplier = 0.3f;

		LightEnvironment m_LightEnvironment;

		Reference<SceneEnvironment> m_Environment;
		float m_EnvironmentIntensity = 1.0f;

		std::vector<std::function<void()>> m_PostUpdateQueue;

		float m_SkyboxLod = 1.0f;
		bool m_IsPlaying = false;
		bool m_ShouldSimulate = false;

		friend class Entity;
		friend class Prefab;
		friend class Physics2D;
		friend class SceneRenderer;
		friend class SceneSerializer;
		friend class PrefabSerializer;
		friend class SceneHierarchyPanel;
		friend class ECSDebugPanel;
	};

} // namespace ForgottenEngine
