#include "fg_pch.hpp"

#include "scene/Scene.hpp"

#include "UUID.hpp"
#include "maths/Mathematics.hpp"
#include "render/Font.hpp"
#include "render/Renderer.hpp"
#include "render/Renderer2D.hpp"
#include "render/SceneRenderer.hpp"
#include "scene/Components.hpp"
#include "scene/Entity.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

// Box2D
#include <assimp/scene.h>
#include <box2d/box2d.h>

namespace ForgottenEngine {

	class Prefab : public ReferenceCounted { };

	class PhysicsScene : public ReferenceCounted { };

	std::unordered_map<UUID, Scene*> s_ActiveScenes;

	struct SceneComponent {
		UUID SceneID;
	};

	namespace Utils {
		glm::mat4 Mat4FromAssimpMat4(const aiMatrix4x4& matrix);
	}

	struct PhysicsSceneComponent {
		Reference<PhysicsScene> World;
	};

	Scene::Scene(const std::string& name, bool isEditorScene, bool initalize)
		: m_Name(name)
		, m_IsEditorScene(isEditorScene)
	{
		m_SceneEntity = m_Registry.create();
		m_Registry.emplace<SceneComponent>(m_SceneEntity, m_SceneID);
		s_ActiveScenes[m_SceneID] = this;

		if (!initalize)
			return;

		// m_Registry.on_construct<ScriptComponent>().connect<&Scene::OnScriptComponentConstruct>(this);
		// m_Registry.on_destroy<ScriptComponent>().connect<&Scene::OnScriptComponentDestroy>(this);

		// This might not be the the best way, but Audio Engine needs to keep track of all audio component
		// to have an easy way to lookup a component associated with active sound.
		// m_Registry.on_construct<AudioComponent>().connect<&Scene::OnAudioComponentConstruct>(this);
		// m_Registry.on_destroy<AudioComponent>().connect<&Scene::OnAudioComponentDestroy>(this);

		m_Registry.on_construct<MeshColliderComponent>().connect<&Scene::OnMeshColliderComponentConstruct>(this);
		m_Registry.on_destroy<MeshColliderComponent>().connect<&Scene::OnMeshColliderComponentDestroy>(this);

		m_Registry.on_construct<RigidBody2DComponent>().connect<&Scene::OnRigidBody2DComponentConstruct>(this);
		m_Registry.on_destroy<RigidBody2DComponent>().connect<&Scene::OnRigidBody2DComponentDestroy>(this);
		m_Registry.on_construct<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentConstruct>(this);
		m_Registry.on_destroy<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentDestroy>(this);
		m_Registry.on_construct<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentConstruct>(this);
		m_Registry.on_destroy<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentDestroy>(this);

		// Box2DWorldComponent& b2dWorld = m_Registry.emplace<Box2DWorldComponent>(m_SceneEntity,
		// std::make_unique<b2World>(b2Vec2 { 0.0f, -9.8f }));
		// b2dWorld.World->SetContactListener(&b2dWorld.ContactListener);

		m_Registry.emplace<PhysicsSceneComponent>(m_SceneEntity, Reference<PhysicsScene>::create());

		Init();
	}

	Scene::~Scene()
	{
		// Clear the registry so that all callbacks are called
		m_Registry.clear();

		// Disconnect EnTT callbacks
		// m_Registry.on_construct<ScriptComponent>().disconnect(this);
		// m_Registry.on_destroy<ScriptComponent>().disconnect(this);

		// m_Registry.on_construct<AudioComponent>().disconnect();
		// m_Registry.on_destroy<AudioComponent>().disconnect();

		m_Registry.on_construct<MeshColliderComponent>().disconnect();
		m_Registry.on_destroy<MeshColliderComponent>().disconnect();

		m_Registry.on_construct<RigidBody2DComponent>().disconnect();
		m_Registry.on_destroy<RigidBody2DComponent>().disconnect();

		s_ActiveScenes.erase(m_SceneID);
		// MiniAudioEngine::OnSceneDestruct(m_SceneID);
	}

	void Scene::Init() { }

	// Merge OnUpdate/Render into one function?
	void Scene::OnUpdate(TimeStep ts)
	{
		/*// Box2D physics
		auto sceneView = m_Registry.view<Box2DWorldComponent>();
		auto& box2DWorld = m_Registry.get<Box2DWorldComponent>(sceneView.front()).World;
		int32_t velocityIterations = 6;
		int32_t positionIterations = 2;
		{
			box2DWorld->Step(ts, velocityIterations, positionIterations);
		}

		{
			auto view = m_Registry.view<RigidBody2DComponent>();
			for (auto entity : view) {
				Entity e = { entity, this };
				auto& rb2d = e.GetComponent<RigidBody2DComponent>();

				if (rb2d.RuntimeBody == nullptr)
					continue;

				b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);

				auto& position = body->GetPosition();
				auto& transform = e.GetComponent<TransformComponent>();
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
				transform.Rotation.z = body->GetAngle();
			}
		}

		auto physicsScene = GetPhysicsScene();*/
	}

	void Scene::OnRenderRuntime(Reference<SceneRenderer> renderer, TimeStep ts)
	{
		/////////////////////////////////////////////////////////////////////
		// RENDER 3D SCENE
		/////////////////////////////////////////////////////////////////////
		Entity cameraEntity = GetMainCameraEntity();
		if (!cameraEntity)
			return;

		glm::mat4 cameraViewMatrix = glm::inverse(GetWorldSpaceTransformMatrix(cameraEntity));
		(cameraEntity, "Scene does not contain any cameras!");
		SceneCamera& camera = cameraEntity.GetComponent<CameraComponent>();
		camera.set_viewport_size(m_ViewportWidth, m_ViewportHeight);

		// Process lights
		{
			m_LightEnvironment = LightEnvironment();
			// Directional Lights
			{
				auto lights = m_Registry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
				uint32_t directionalLightIndex = 0;
				for (auto entity : lights) {
					auto [transformComponent, lightComponent] = lights.get<TransformComponent, DirectionalLightComponent>(entity);
					glm::vec3 direction = -glm::normalize(glm::mat3(transformComponent.GetTransform()) * glm::vec3(1.0f));
					m_LightEnvironment.DirectionalLights[directionalLightIndex++] = {
						direction,
						lightComponent.Radiance,
						lightComponent.Intensity,
						lightComponent.ShadowAmount,
						lightComponent.CastShadows,
					};
				}
				// Point Lights
				{
					auto pointLights = m_Registry.group<PointLightComponent>(entt::get<TransformComponent>);
					m_LightEnvironment.PointLights.resize(pointLights.size());
					uint32_t pointLightIndex = 0;
					for (auto e : pointLights) {
						Entity entity(e, this);
						auto [transformComponent, lightComponent] = pointLights.get<TransformComponent, PointLightComponent>(e);
						auto transform = entity.HasComponent<RigidBodyComponent>() ? entity.Transform() : GetWorldSpaceTransform(entity);
						m_LightEnvironment.PointLights[pointLightIndex++] = {
							transform.Translation,
							lightComponent.Intensity,
							lightComponent.Radiance,
							lightComponent.MinRadius,
							lightComponent.Radius,
							lightComponent.Falloff,
							lightComponent.LightSize,
							lightComponent.CastsShadows,
						};
					}
				}
			}
		}

		// TODO: only one sky light at the moment!
		{
			/* auto lights = m_Registry.group<SkyLightComponent>(entt::get<TransformComponent>);
			if (lights.empty())
				m_Environment = Reference<SceneEnvironment>::create(Renderer::GetBlackCubeTexture(),
			Renderer::GetBlackCubeTexture());

			for (auto entity : lights) {
				auto [transformComponent, skyLightComponent] = lights.get<TransformComponent,
			SkyLightComponent>(entity); if (!AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) &&
			skyLightComponent.DynamicSky) { Reference<TextureCube> preethamEnv =
			Renderer::CreatePreethamSky(skyLightComponent.TurbidityAzimuthInclination.x,
			skyLightComponent.TurbidityAzimuthInclination.y, skyLightComponent.TurbidityAzimuthInclination.z);
					skyLightComponent.SceneEnvironment =
			AssetManager::CreateMemoryOnlyAsset<SceneEnvironment>(preethamEnv, preethamEnv);
				}
				m_Environment = AssetManager::GetAsset<SceneEnvironment>(skyLightComponent.SceneEnvironment);
				m_EnvironmentIntensity = skyLightComponent.Intensity;
				m_SkyboxLod = skyLightComponent.Lod;
			}*/
		}

		renderer->SetScene(this);
		renderer->BeginScene({ camera, cameraViewMatrix, camera.get_perspective_near_clip(), camera.get_perspective_far_clip(),
			camera.get_rad_perspective_vertical_fov() });

		// Render Static Meshes
		{
			/*
			auto group = m_Registry.group<StaticMeshComponent>(entt::get<TransformComponent>);
			for (auto entity : group) {
				auto [transformComponent, staticMeshComponent] = group.get<TransformComponent,
			StaticMeshComponent>(entity); auto staticMesh =
			AssetManager::GetAsset<StaticMesh>(staticMeshComponent.StaticMesh); if
			(!staticMesh->IsFlagSet(AssetFlag::Missing)) { Entity e = Entity(entity, this); glm::mat4 transform =
			GetWorldSpaceTransformMatrix(e, true); renderer->SubmitStaticMesh(staticMesh,
			staticMeshComponent.MaterialTable, transform);
					}
			}
			*/
		}

		// Render Dynamic Meshes
		{
			/*
			auto view = m_Registry.view<MeshComponent, TransformComponent>();
			for (auto entity : view) {
				HZ_PROFILE_FUNC("Scene-SubmitDynamicMesh");
				auto [transformComponent, meshComponent] = view.get<TransformComponent, MeshComponent>(entity);
				if (AssetManager::IsAssetHandleValid(meshComponent.Mesh)) {
					auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.Mesh);
					if (!mesh->IsFlagSet(AssetFlag::Missing)) {
						mesh->OnUpdate(ts);
						Entity e = Entity(entity, this);
						glm::mat4 transform = GetWorldSpaceTransformMatrix(e, true);
						renderer->SubmitMesh(mesh, meshComponent.SubmeshIndex, meshComponent.MaterialTable, transform);
					}
				}
			}
			*/
		}

		RenderPhysicsDebug(renderer, true);

		renderer->EndScene();
		/////////////////////////////////////////////////////////////////////
		// Render 2D
		if (renderer->GetFinalPassImage()) {
			Reference<Renderer2D> renderer2D = renderer->GetRenderer2D();

			renderer2D->begin_scene(camera.get_projection_matrix() * cameraViewMatrix, cameraViewMatrix);
			renderer2D->set_target_render_pass(renderer->GetExternalCompositeRenderPass());
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<TextComponent>);
				for (auto entity : group) {
					auto [transformComponent, textComponent] = group.get<TransformComponent, TextComponent>(entity);
					Entity e = Entity(entity, this);
					renderer2D->draw_string(textComponent.TextString, Font::get_default_font(), GetWorldSpaceTransformMatrix(e),
						textComponent.MaxWidth, textComponent.Color, textComponent.LineSpacing, textComponent.Kerning);
				}
			}

			renderer2D->end_scene();
		}
	}

	void Scene::OnRenderEditor(Reference<SceneRenderer> renderer, TimeStep ts, const UserCamera& editorCamera)
	{
		/////////////////////////////////////////////////////////////////////
		// RENDER 3D SCENE
		/////////////////////////////////////////////////////////////////////

		// Lighting
		{
			m_LightEnvironment = LightEnvironment();
			// Directional Lights
			{
				auto dirLights = m_Registry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
				uint32_t directionalLightIndex = 0;
				for (auto entity : dirLights) {
					auto [transformComponent, lightComponent] = dirLights.get<TransformComponent, DirectionalLightComponent>(entity);
					glm::vec3 direction = -glm::normalize(glm::mat3(transformComponent.GetTransform()) * glm::vec3(1.0f));
					m_LightEnvironment.DirectionalLights[directionalLightIndex++]
						= { direction, lightComponent.Radiance, lightComponent.Intensity, lightComponent.ShadowAmount, lightComponent.CastShadows };
				}
			}
			// Point Lights
			{
				auto pointLights = m_Registry.group<PointLightComponent>(entt::get<TransformComponent>);
				m_LightEnvironment.PointLights.resize(pointLights.size());
				uint32_t pointLightIndex = 0;
				for (auto entity : pointLights) {
					auto [transformComponent, lightComponent] = pointLights.get<TransformComponent, PointLightComponent>(entity);
					auto transform = GetWorldSpaceTransform(Entity(entity, this));
					m_LightEnvironment.PointLights[pointLightIndex++] = {
						transform.Translation,
						lightComponent.Intensity,
						lightComponent.Radiance,
						lightComponent.MinRadius,
						lightComponent.Radius,
						lightComponent.Falloff,
						lightComponent.LightSize,
						lightComponent.CastsShadows,
					};
				}
			}
		}

		{
			/*
			auto lights = m_Registry.group<SkyLightComponent>(entt::get<TransformComponent>);
			if (lights.empty())
				m_Environment = Reference<SceneEnvironment>::create(Renderer::GetBlackCubeTexture(),
			Renderer::GetBlackCubeTexture());

			for (auto entity : lights) {
				auto [transformComponent, skyLightComponent] = lights.get<TransformComponent,
			SkyLightComponent>(entity); if (!AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) &&
			skyLightComponent.DynamicSky) { Reference<TextureCube> preethamEnv =
			Renderer::CreatePreethamSky(skyLightComponent.TurbidityAzimuthInclination.x,
			skyLightComponent.TurbidityAzimuthInclination.y, skyLightComponent.TurbidityAzimuthInclination.z);
					skyLightComponent.SceneEnvironment =
			AssetManager::CreateMemoryOnlyAsset<SceneEnvironment>(preethamEnv, preethamEnv);
				}
				m_Environment = AssetManager::GetAsset<SceneEnvironment>(skyLightComponent.SceneEnvironment);
				m_EnvironmentIntensity = skyLightComponent.Intensity;
				m_SkyboxLod = skyLightComponent.Lod;
			}
			*/
		}

		renderer->SetScene(this);
		renderer->BeginScene({ editorCamera, editorCamera.get_view_matrix(), editorCamera.get_near_clip(), editorCamera.get_far_clip(),
			editorCamera.get_vertical_fov() });

		// Render Static Meshes
		{
			/*
			auto group = m_Registry.group<StaticMeshComponent>(entt::get<TransformComponent>);
			for (auto entity : group) {
				auto [transformComponent, staticMeshComponent] = group.get<TransformComponent,
			StaticMeshComponent>(entity); auto staticMesh =
			AssetManager::GetAsset<StaticMesh>(staticMeshComponent.StaticMesh); if (staticMesh &&
			!staticMesh->IsFlagSet(AssetFlag::Missing)) { Entity e = Entity(entity, this); glm::mat4 transform =
			GetWorldSpaceTransformMatrix(e);

					if (SelectionManager::IsSelected(e.GetUUID()))
						renderer->SubmitSelectedStaticMesh(staticMesh, staticMeshComponent.MaterialTable, transform);
					else
						renderer->SubmitStaticMesh(staticMesh, staticMeshComponent.MaterialTable, transform);
				}
			}
			*/
		}

		// Render Dynamic Meshes
		{
			/*
			auto view = m_Registry.view<MeshComponent, TransformComponent>();
			for (auto entity : view) {
				auto [transformComponent, meshComponent] = view.get<TransformComponent, MeshComponent>(entity);
				auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.Mesh);
				if (mesh && !mesh->IsFlagSet(AssetFlag::Missing)) {
					mesh->OnUpdate(ts);
					Entity e = Entity(entity, this);
					glm::mat4 transform = GetWorldSpaceTransformMatrix(e);

					// TODO: Should we render (logically)
					if (SelectionManager::IsSelected(e.GetUUID()))
						renderer->SubmitSelectedMesh(mesh, meshComponent.SubmeshIndex, meshComponent.MaterialTable,
			transform); else renderer->SubmitMesh(mesh, meshComponent.SubmeshIndex, meshComponent.MaterialTable,
			transform);
				}
			}
			*/
		}

		RenderPhysicsDebug(renderer, false);

		renderer->EndScene();
		/////////////////////////////////////////////////////////////////////

		// Render 2D
		if (renderer->GetFinalPassImage()) {
			Reference<Renderer2D> renderer2D = renderer->GetRenderer2D();

			renderer2D->begin_scene(editorCamera.get_view_projection(), editorCamera.get_view_matrix());
			renderer2D->set_target_render_pass(renderer->GetExternalCompositeRenderPass());
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<TextComponent>);
				for (auto entity : group) {
					auto [transformComponent, textComponent] = group.get<TransformComponent, TextComponent>(entity);
					Entity e = Entity(entity, this);
					renderer2D->draw_string(textComponent.TextString, Font::get_default_font(), GetWorldSpaceTransformMatrix(e),
						textComponent.MaxWidth, textComponent.Color, textComponent.LineSpacing, textComponent.Kerning);
				}
			}

			renderer2D->end_scene();
		}
	}

	void Scene::OnRenderSimulation(Reference<SceneRenderer> renderer, TimeStep ts, const UserCamera& editorCamera)
	{
		/////////////////////////////////////////////////////////////////////
		// RENDER 3D SCENE
		/////////////////////////////////////////////////////////////////////

		// Lighting
		{
			m_LightEnvironment = LightEnvironment();
			// Directional Lights
			{
				auto dirLights = m_Registry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
				uint32_t directionalLightIndex = 0;
				for (auto entity : dirLights) {
					auto [transformComponent, lightComponent] = dirLights.get<TransformComponent, DirectionalLightComponent>(entity);
					glm::vec3 direction = -glm::normalize(glm::mat3(transformComponent.GetTransform()) * glm::vec3(1.0f));
					m_LightEnvironment.DirectionalLights[directionalLightIndex++]
						= { direction, lightComponent.Radiance, lightComponent.Intensity, lightComponent.ShadowAmount, lightComponent.CastShadows };
				}
			}
			// Point Lights
			{
				auto pointLights = m_Registry.group<PointLightComponent>(entt::get<TransformComponent>);
				m_LightEnvironment.PointLights.resize(pointLights.size());
				uint32_t pointLightIndex = 0;
				for (auto e : pointLights) {
					Entity entity(e, this);
					auto [transformComponent, lightComponent] = pointLights.get<TransformComponent, PointLightComponent>(entity);
					auto transform = entity.HasComponent<RigidBodyComponent>() ? entity.Transform() : GetWorldSpaceTransform(entity);
					m_LightEnvironment.PointLights[pointLightIndex++] = {
						transform.Translation,
						lightComponent.Intensity,
						lightComponent.Radiance,
						lightComponent.MinRadius,
						lightComponent.Radius,
						lightComponent.Falloff,
						lightComponent.LightSize,
						lightComponent.CastsShadows,
					};
				}
			}
		}

		{
			/*
			auto lights = m_Registry.group<SkyLightComponent>(entt::get<TransformComponent>);
			if (lights.empty())
				m_Environment = Reference<SceneEnvironment>::Create(Renderer::GetBlackCubeTexture(),
			Renderer::GetBlackCubeTexture());

			for (auto entity : lights) {
				auto [transformComponent, skyLightComponent] = lights.get<TransformComponent,
			SkyLightComponent>(entity); if (!AssetManager::IsAssetHandleValid(skyLightComponent.SceneEnvironment) &&
			skyLightComponent.DynamicSky) { Reference<TextureCube> preethamEnv =
			Renderer::CreatePreethamSky(skyLightComponent.TurbidityAzimuthInclination.x,
			skyLightComponent.TurbidityAzimuthInclination.y, skyLightComponent.TurbidityAzimuthInclination.z);
					skyLightComponent.SceneEnvironment =
			AssetManager::CreateMemoryOnlyAsset<SceneEnvironment>(preethamEnv, preethamEnv);
				}
				m_Environment = AssetManager::GetAsset<SceneEnvironment>(skyLightComponent.SceneEnvironment);
				m_EnvironmentIntensity = skyLightComponent.Intensity;
				m_SkyboxLod = skyLightComponent.Lod;
			}
			*/
		}

		auto group = m_Registry.group<MeshComponent>(entt::get<TransformComponent>);
		renderer->SetScene(this);
		renderer->BeginScene({ editorCamera, editorCamera.get_view_matrix(), editorCamera.get_near_clip(), editorCamera.get_far_clip(),
			editorCamera.get_vertical_fov() });

		// Render Static Meshes
		{
			/*
			auto group = m_Registry.group<StaticMeshComponent>(entt::get<TransformComponent>);
			for (auto entity : group) {
				auto [transformComponent, staticMeshComponent] = group.get<TransformComponent,
			StaticMeshComponent>(entity); if (AssetManager::IsAssetHandleValid(staticMeshComponent.StaticMesh)) { auto
			staticMesh = AssetManager::GetAsset<StaticMesh>(staticMeshComponent.StaticMesh); if
			(!staticMesh->IsFlagSet(AssetFlag::Missing)) { Entity e = Entity(entity, this); glm::mat4 transform =
			GetWorldSpaceTransformMatrix(e, true);

						if (SelectionManager::IsSelected(e.GetUUID()))
							renderer->SubmitSelectedStaticMesh(staticMesh, staticMeshComponent.MaterialTable,
			transform); else renderer->SubmitStaticMesh(staticMesh, staticMeshComponent.MaterialTable, transform);
					}
				}
			}
			*/
		}

		// Render Dynamic Meshes
		{
			/*
			auto view = m_Registry.view<MeshComponent, TransformComponent>();
			for (auto entity : view) {
				auto [transformComponent, meshComponent] = view.get<TransformComponent, MeshComponent>(entity);
				if (AssetManager::IsAssetHandleValid(meshComponent.Mesh)) {
					auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.Mesh);
					if (mesh && !mesh->IsFlagSet(AssetFlag::Missing)) {
						mesh->OnUpdate(ts);
						Entity e = Entity(entity, this);
						glm::mat4 transform = GetWorldSpaceTransformMatrix(e, true);

						if (SelectionManager::IsSelected(e.GetUUID()))
							renderer->SubmitSelectedMesh(mesh, meshComponent.SubmeshIndex, meshComponent.MaterialTable,
			transform); else renderer->SubmitMesh(mesh, meshComponent.SubmeshIndex, meshComponent.MaterialTable,
			transform);
					}
				}
			}
			*/
		}

		RenderPhysicsDebug(renderer, true);

		renderer->EndScene();

		// Render 2D
		if (renderer->GetFinalPassImage()) {
			Reference<Renderer2D> renderer2D = renderer->GetRenderer2D();

			renderer2D->begin_scene(editorCamera.get_view_projection(), editorCamera.get_view_matrix());
			renderer2D->set_target_render_pass(renderer->GetExternalCompositeRenderPass());
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<TextComponent>);
				for (auto entity : group) {
					auto [transformComponent, textComponent] = group.get<TransformComponent, TextComponent>(entity);
					Entity e = Entity(entity, this);
					renderer2D->draw_string(textComponent.TextString, Font::get_default_font(), GetWorldSpaceTransformMatrix(e),
						textComponent.MaxWidth, textComponent.Color, textComponent.LineSpacing, textComponent.Kerning);
				}
			}

			renderer2D->end_scene();
		}
	}

	void Scene::RenderPhysicsDebug(Reference<SceneRenderer> renderer, bool runtime) { }

	void Scene::OnEvent(Event& e) { }

	void Scene::OnRuntimeStart() { }

	void Scene::OnRuntimeStop() { }

	void Scene::OnSimulationStart() { }
	void Scene::OnSimulationStop() { }

	void Scene::OnScriptComponentConstruct(entt::registry& registry, entt::entity entity) { }
	void Scene::OnScriptComponentDestroy(entt::registry& registry, entt::entity entity) { }

	void Scene::OnAudioComponentConstruct(entt::registry& registry, entt::entity entity) { }

	void Scene::OnAudioComponentDestroy(entt::registry& registry, entt::entity entity) { }
	void Scene::OnMeshColliderComponentConstruct(entt::registry& registry, entt::entity entity) { }

	void Scene::OnMeshColliderComponentDestroy(entt::registry& registry, entt::entity entity) { }

	void Scene::OnRigidBody2DComponentConstruct(entt::registry& registry, entt::entity entity) { }

	void Scene::OnRigidBody2DComponentDestroy(entt::registry& registry, entt::entity entity) { }
	void Scene::OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity) { }

	void Scene::OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity) { }

	void Scene::OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity) { }

	void Scene::OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity) { }

	void Scene::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	Entity Scene::GetMainCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view) {
			auto& comp = view.get<CameraComponent>(entity);
			if (comp.Primary) {
				CORE_ASSERT(comp.Camera.get_orthographic_size() || comp.Camera.get_deg_perspective_vertical_fov(), "Camera is not fully initialized");
				return { entity, this };
			}
		}
		return {};
	}

	Entity Scene::CreateEntity(const std::string& name) { return CreateChildEntity({}, name); }

	Entity Scene::CreateChildEntity(Entity parent, const std::string& name) { return Entity(); }

	Entity Scene::CreateEntityWithID(UUID uuid, const std::string& name, bool runtimeMap) { return Entity(); }

	void Scene::SubmitToDestroyEntity(Entity entity) { }

	void Scene::DestroyEntity(Entity entity, bool excludeChildren, bool first) { }

	void Scene::ResetTransformsToMesh(Entity entity, bool resetChildren) { }

	template <typename T>
	static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		auto components = srcRegistry.view<T>();
		for (auto srcEntity : components) {
			entt::entity destEntity = enttMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);

			auto& srcComponent = srcRegistry.get<T>(srcEntity);
			auto& destComponent = dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
		}
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		auto parentNewEntity = [&entity, scene = this](Entity newEntity) {
			if (auto parent = entity.GetParent(); parent) {
				newEntity.SetParentUUID(parent.GetUUID());
				parent.Children().push_back(newEntity.GetUUID());
			}
		};

		Entity newEntity;
		if (entity.HasComponent<TagComponent>())
			newEntity = CreateEntity(entity.GetComponent<TagComponent>().Tag);
		else
			newEntity = CreateEntity();

		CopyComponentIfExists<TransformComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		// NOTE(Peter): We can't use this method for copying the RelationshipComponent since we
		//				need to duplicate the entire child heirarchy and basically reconstruct the entire
		// RelationshipComponent from the ground up
		// CopyComponentIfExists<RelationshipComponent>(newEntity.m_EntityHandle, entity.m_EntityHandle, m_Registry);
		CopyComponentIfExists<MeshComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<StaticMeshComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<CameraComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<SpriteRendererComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<TextComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<RigidBody2DComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<BoxCollider2DComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<CircleCollider2DComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<RigidBodyComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<CharacterControllerComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<FixedJointComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<BoxColliderComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<SphereColliderComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<CapsuleColliderComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<MeshColliderComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<DirectionalLightComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<PointLightComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<SkyLightComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);
		CopyComponentIfExists<AudioListenerComponent>(newEntity.m_EntityHandle, m_Registry, entity.m_EntityHandle);

		auto childIds = entity.Children(); // need to take a copy of children here, because the collection is mutated below
		for (auto childId : childIds) {
			Entity childDuplicate = DuplicateEntity(GetEntityWithUUID(childId));

			// At this point childDuplicate is a child of entity, we need to remove it from that entity
			UnparentEntity(childDuplicate, false);

			childDuplicate.SetParentUUID(newEntity.GetUUID());
			newEntity.Children().push_back(childDuplicate.GetUUID());
		}

		parentNewEntity(newEntity);

		return newEntity;
	}

	Entity Scene::CreatePrefabEntity(Entity entity, Entity parent, const glm::vec3* translation, const glm::vec3* rotation, const glm::vec3* scale)
	{
		return Entity();
	}

	Entity Scene::Instantiate(Reference<Prefab> prefab, const glm::vec3* translation, const glm::vec3* rotation, const glm::vec3* scale)
	{
		Entity result;
		return result;
	}

	void Scene::BuildMeshEntityHierarchy(Entity parent, Reference<Mesh> mesh, const void* assimpScene, void* assimpNode, bool generateColliders) { }

	Entity Scene::InstantiateMesh(Reference<Mesh> mesh, bool generateColliders) { return Entity(); }

	Entity Scene::GetEntityWithUUID(UUID id) const { return Entity(); }
	Entity Scene::TryGetEntityWithUUID(UUID id) const
	{
		if (const auto iter = m_EntityIDMap.find(id); iter != m_EntityIDMap.end())
			return iter->second;
		return Entity {};
	}

	Entity Scene::TryGetEntityWithTag(const std::string& tag)
	{
		auto entities = GetAllEntitiesWith<TagComponent>();
		for (auto e : entities) {
			if (entities.get<TagComponent>(e).Tag == tag)
				return Entity(e, const_cast<Scene*>(this));
		}

		return Entity {};
	}

	void Scene::SortEntities()
	{
		m_Registry.sort<IDComponent>([&](const auto lhs, const auto rhs) {
			auto lhsEntity = m_EntityIDMap.find(lhs.ID);
			auto rhsEntity = m_EntityIDMap.find(rhs.ID);
			return static_cast<uint32_t>(lhsEntity->second) < static_cast<uint32_t>(rhsEntity->second);
		});
	}

	void Scene::ConvertToLocalSpace(Entity entity)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

		if (!parent)
			return;

		auto& transform = entity.Transform();
		glm::mat4 parentTransform = GetWorldSpaceTransformMatrix(parent);
		glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
		transform.SetTransform(localTransform);
	}

	void Scene::ConvertToWorldSpace(Entity entity)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());

		if (!parent)
			return;

		glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
		auto& entityTransform = entity.Transform();
		entityTransform.SetTransform(transform);
	}

	glm::mat4 Scene::GetWorldSpaceTransformMatrix(Entity entity, bool accountForActor)
	{
		if (accountForActor && entity.HasComponent<RigidBodyComponent>()) {
			const auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

			if (rigidBodyComponent.BodyType == RigidBodyComponent::Type::Dynamic)
				return entity.Transform().GetTransform();
		}

		glm::mat4 transform(1.0f);

		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
		if (parent)
			transform = GetWorldSpaceTransformMatrix(parent);

		return transform * entity.Transform().GetTransform();
	}

	// TODO: Definitely cache this at some point
	TransformComponent Scene::GetWorldSpaceTransform(Entity entity)
	{
		glm::mat4 transform = GetWorldSpaceTransformMatrix(entity);
		TransformComponent transformComponent;
		transformComponent.SetTransform(transform);
		return transformComponent;
	}

	void Scene::ParentEntity(Entity entity, Entity parent)
	{
		if (parent.IsDescendantOf(entity)) {
			UnparentEntity(parent);

			Entity newParent = TryGetEntityWithUUID(entity.GetParentUUID());
			if (newParent) {
				UnparentEntity(entity);
				ParentEntity(parent, newParent);
			}
		} else {
			Entity previousParent = TryGetEntityWithUUID(entity.GetParentUUID());

			if (previousParent)
				UnparentEntity(entity);
		}

		entity.SetParentUUID(parent.GetUUID());
		parent.Children().push_back(entity.GetUUID());

		ConvertToLocalSpace(entity);
	}

	void Scene::UnparentEntity(Entity entity, bool convertToWorldSpace)
	{
		Entity parent = TryGetEntityWithUUID(entity.GetParentUUID());
		if (!parent)
			return;

		auto& parentChildren = parent.Children();
		parentChildren.erase(std::remove(parentChildren.begin(), parentChildren.end(), entity.GetUUID()), parentChildren.end());

		if (convertToWorldSpace)
			ConvertToWorldSpace(entity);

		entity.SetParentUUID(0);
	}

	// Copy to runtime
	void Scene::CopyTo(Reference<Scene>& target)
	{
		// TODO(Yan): hack to prevent Box2D bodies from being created on copy via entt signals
		target->m_IsEditorScene = true;

		// Environment
		target->m_Light = m_Light;
		target->m_LightMultiplier = m_LightMultiplier;

		target->m_Environment = m_Environment;
		target->m_SkyboxLod = m_SkyboxLod;

		std::unordered_map<UUID, entt::entity> enttMap;
		auto idComponents = m_Registry.view<IDComponent>();
		for (auto entity : idComponents) {
			auto uuid = m_Registry.get<IDComponent>(entity).ID;
			auto name = m_Registry.get<TagComponent>(entity).Tag;
			Entity e = target->CreateEntityWithID(uuid, name, true);
			enttMap[uuid] = e.m_EntityHandle;
		}

		// 0x: This code is a bug generator:  what's the bet someone adds a component and forgets to update here...?
		//     TODO: come up with a more robust way of doing this.  At least ASSERT if something is missed.
		CopyComponent<PrefabComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<TagComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<TransformComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<RelationshipComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<MeshComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<StaticMeshComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<DirectionalLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<PointLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<SkyLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CameraComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<SpriteRendererComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<TextComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<RigidBody2DComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<BoxCollider2DComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CircleCollider2DComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<RigidBodyComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CharacterControllerComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<FixedJointComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<BoxColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<SphereColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CapsuleColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<MeshColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<AudioListenerComponent>(target->m_Registry, m_Registry, enttMap);

		target->SetPhysics2DGravity(GetPhysics2DGravity());

		// Sort IdComponent by by entity handle (which is essentially the order in which they were created)
		// This ensures a consistent ordering when iterating IdComponent (for example: when rendering scene hierarchy
		// panel)
		target->SortEntities();

		target->m_ViewportWidth = m_ViewportWidth;
		target->m_ViewportHeight = m_ViewportHeight;

		target->m_IsEditorScene = false;
	}

	Reference<Scene> Scene::GetScene(UUID uuid)
	{
		if (s_ActiveScenes.find(uuid) != s_ActiveScenes.end())
			return s_ActiveScenes.at(uuid);

		return {};
	}

	float Scene::GetPhysics2DGravity() const { return -9.82; }

	void Scene::SetPhysics2DGravity(float gravity) { }

	Reference<PhysicsScene> Scene::GetPhysicsScene() const { return {}; }

	void Scene::OnSceneTransition(const std::string& scene)
	{
		if (m_OnSceneTransitionCallback)
			m_OnSceneTransitionCallback(scene);

		// Debug
		if (!m_OnSceneTransitionCallback) {
			CORE_WARN("Cannot transition scene - no callback set!");
		}
	}

	Reference<Scene> Scene::CreateEmpty() { return Reference<Scene>::create("Empty", false, false); }

} // namespace ForgottenEngine
