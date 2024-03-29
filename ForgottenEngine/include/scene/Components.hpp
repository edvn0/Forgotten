#pragma once

#include "maths/Mathematics.hpp"
#include "render/MaterialAsset.hpp"
#include "render/Mesh.hpp"
#include "render/SceneEnvironment.hpp"
#include "render/Texture.hpp"
#include "scene/SceneCamera.hpp"
#include "UUID.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "Asset.hpp"
#include "maths/Mathematics.hpp"
#include "render/MaterialAsset.hpp"
#include "render/Mesh.hpp"
#include "render/SceneEnvironment.hpp"
#include "render/Texture.hpp"
#include "scene/SceneCamera.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <limits>

namespace ForgottenEngine {

	struct IDComponent {
		UUID ID = 0;
	};

	struct TagComponent {
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent& other) = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{
		}

		operator std::string&() { return Tag; }
		operator const std::string&() const { return Tag; }
	};

	struct RelationshipComponent {
		UUID ParentHandle = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent& other) = default;
		RelationshipComponent(UUID parent)
			: ParentHandle(parent)
		{
		}
	};

	struct PrefabComponent {
		UUID PrefabID = 0;
		UUID EntityID = 0;

		PrefabComponent() = default;
		PrefabComponent(const PrefabComponent& other) = default;
	};

	struct TransformComponent {
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

	private:
		// These are private so that you are forced to set them via
		// SetRotation() or SetRotationEuler()
		// This avoids situation where one of them gets set and the other is forgotten.
		//
		// Why do we need both a quat and Euler angle representation for rotation?
		// Because Euler suffers from gimbal lock -> rotations should be stored as quaternions.
		//
		// BUT: quaternions are confusing, and humans like to work with Euler angles.
		// We cannot store just the quaternions and translate to/from Euler because the conversion
		// Euler -> quat -> Euler is not invariant.
		//
		// It's also sometimes useful to be able to store rotations > 360 degrees which
		// quats do not support.
		//
		// Accordingly, we store Euler for "editor" stuff that humans work with,
		// and quats for everything else.  The two are maintained in-sync via the SetRotation()
		// methods.
		glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
		glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };

	public:
		TransformComponent() = default;
		TransformComponent(const TransformComponent& other) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation)
		{
		}

		glm::mat4 GetTransform() const
		{
			return glm::translate(glm::mat4(1.0f), Translation) * glm::toMat4(Rotation) * glm::scale(glm::mat4(1.0f), Scale);
		}

		void SetTransform(const glm::mat4& transform)
		{
			Maths::decompose_transform(transform, Translation, Rotation, Scale);
			RotationEuler = glm::eulerAngles(Rotation);
		}

		glm::vec3 GetRotationEuler() const { return RotationEuler; }

		void SetRotationEuler(const glm::vec3& euler)
		{
			RotationEuler = euler;
			Rotation = glm::quat(RotationEuler);
		}

		glm::quat GetRotation() const { return Rotation; }

		void SetRotation(const glm::quat& quat)
		{
			Rotation = quat;
			RotationEuler = glm::eulerAngles(Rotation);
		}

		friend class SceneSerializer;
	};

	struct MeshComponent {
		AssetHandle Mesh;
		uint32_t SubmeshIndex = 0;
		Reference<ForgottenEngine::MaterialTable> MaterialTable = Reference<ForgottenEngine::MaterialTable>::create();
		std::vector<UUID> BoneEntityIds; // If mesh is rigged, these are the entities whose transforms will used to "skin" the rig.

		MeshComponent() = default;
		MeshComponent(const MeshComponent& other)
			: Mesh(other.Mesh)
			, SubmeshIndex(other.SubmeshIndex)
			, MaterialTable(Reference<ForgottenEngine::MaterialTable>::create(other.MaterialTable))
			, BoneEntityIds(other.BoneEntityIds)
		{
		}
		MeshComponent(AssetHandle mesh, uint32_t submeshIndex = 0)
			: Mesh(mesh)
			, SubmeshIndex(submeshIndex)
		{
		}
	};

	struct StaticMeshComponent {
		AssetHandle StaticMesh;
		Reference<ForgottenEngine::MaterialTable> MaterialTable = Reference<ForgottenEngine::MaterialTable>::create();

		StaticMeshComponent() = default;
		StaticMeshComponent(const StaticMeshComponent& other)
			: StaticMesh(other.StaticMesh)
			, MaterialTable(Reference<ForgottenEngine::MaterialTable>::create(other.MaterialTable))
		{
		}
		StaticMeshComponent(AssetHandle staticMesh)
			: StaticMesh(staticMesh)
		{
		}
	};

	struct ScriptComponent {
		AssetHandle ScriptClassHandle = 0;
		std::vector<uint32_t> FieldIDs;

		// NOTE(Peter): Get's set to true when OnCreate has been called for this entity
		bool IsRuntimeInitialized = false;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent& other) = default;
		ScriptComponent(AssetHandle scriptClassHandle)
			: ScriptClassHandle(scriptClassHandle)
		{
		}
	};

	struct CameraComponent {
		SceneCamera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent& other) = default;

		operator SceneCamera&() { return Camera; }
		operator const SceneCamera&() const { return Camera; }
	};

	struct SpriteRendererComponent {
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		AssetHandle Texture;
		float TilingFactor = 1.0f;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent& other) = default;
	};

	struct TextComponent {
		std::string TextString = "";
		size_t TextHash;

		// Font
		AssetHandle FontHandle;
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float LineSpacing = 0.0f;
		float Kerning = 0.0f;

		// Layout
		float MaxWidth = 10.0f;

		TextComponent() = default;
		TextComponent(const TextComponent& other) = default;
	};

	struct RigidBody2DComponent {
		enum class Type { None = -1, Static, Dynamic, Kinematic };
		Type BodyType;
		bool FixedRotation = false;
		float Mass = 1.0f;
		float LinearDrag = 0.01f;
		float AngularDrag = 0.05f;
		float GravityScale = 1.0f;
		bool IsBullet = false;
		// Storage for runtime
		void* RuntimeBody = nullptr;

		RigidBody2DComponent() = default;
		RigidBody2DComponent(const RigidBody2DComponent& other) = default;
	};

	struct BoxCollider2DComponent {
		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 0.5f, 0.5f };

		float Density = 1.0f;
		float Friction = 1.0f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent& other) = default;
	};

	struct CircleCollider2DComponent {
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 1.0f;

		float Density = 1.0f;
		float Friction = 1.0f;

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent& other) = default;
	};

	struct RigidBodyComponent {
		enum class Type { None = -1, Static, Dynamic };

		Type BodyType = Type::Static;
		uint32_t LayerID = 0;

		float Mass = 1.0f;
		float LinearDrag = 0.01f;
		float AngularDrag = 0.05f;
		bool DisableGravity = false;
		bool IsKinematic = false;
		uint8_t LockFlags = 0;

		RigidBodyComponent() = default;
		RigidBodyComponent(const RigidBodyComponent& other) = default;
	};

	// A physics actor specifically tailored to character movement
	// For now we support capsule character volume only
	struct CharacterControllerComponent {
		float SlopeLimitDeg;
		float StepOffset;
		uint32_t LayerID = 0;
		bool DisableGravity = false;
	};

	// Fixed Joints restricts an object's movement to be dependent upon another object.
	// This is somewhat similar to Parenting but is implemented through physics rather than Transform hierarchy
	struct FixedJointComponent {
		UUID ConnectedEntity;

		bool IsBreakable = true;
		float BreakForce = 100.0f;
		float BreakTorque = 10.0f;

		bool EnableCollision = false;
		bool EnablePreProcessing = true;
	};

	struct BoxColliderComponent {
		glm::vec3 HalfSize = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
		AssetHandle Material = 0;

		BoxColliderComponent() = default;
		BoxColliderComponent(const BoxColliderComponent& other) = default;
	};

	struct SphereColliderComponent {
		float Radius = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
		AssetHandle Material = 0;

		SphereColliderComponent() = default;
		SphereColliderComponent(const SphereColliderComponent& other) = default;
	};

	struct CapsuleColliderComponent {
		float Radius = 0.5f;
		float Height = 1.0f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
		AssetHandle Material = 0;

		CapsuleColliderComponent() = default;
		CapsuleColliderComponent(const CapsuleColliderComponent& other) = default;
	};

	struct MeshColliderComponent {
		AssetHandle ColliderAsset = 0;
		uint32_t SubmeshIndex = 0;
		bool IsTrigger = false;
		bool UseSharedShape = false;
		AssetHandle OverrideMaterial = 0;

		MeshColliderComponent() = default;
		MeshColliderComponent(const MeshColliderComponent& other) = default;
		MeshColliderComponent(AssetHandle colliderAsset, uint32_t submeshIndex = 0)
			: ColliderAsset(colliderAsset)
			, SubmeshIndex(submeshIndex)
		{
		}
	};

	enum class LightType { None = 0, Directional = 1, Point = 2, Spot = 3 };

	struct DirectionalLightComponent {
		glm::vec3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		bool CastShadows = true;
		bool SoftShadows = true;
		float LightSize = 0.5f; // For PCSS
		float ShadowAmount = 1.0f;
	};

	struct PointLightComponent {
		glm::vec3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float LightSize = 0.5f; // For PCSS
		float MinRadius = 1.f;
		float Radius = 10.0f;
		bool CastsShadows = true;
		bool SoftShadows = true;
		float Falloff = 1.0f;
	};

	struct SpotLightComponent {
		glm::vec3 Radiance { 1.0f };
		float Intensity = 1.0f;
		float Range = 10.0f;
		float Angle = 60.0f;
		float AngleAttenuation = 5.0f;
		bool CastsShadows = false;
		bool SoftShadows = false;
		float Falloff = 1.0f;
	};

	struct SkyLightComponent {
		AssetHandle SceneEnvironment;
		float Intensity = 1.0f;
		float Lod = 0.0f;

		bool DynamicSky = false;
		glm::vec3 TurbidityAzimuthInclination = { 2.0, 0.0, 0.0 };
	};

	struct AudioListenerComponent {
		// int ListenerID = -1;
		bool Active = false;
		float ConeInnerAngleInRadians = 6.283185f; /* 360 degrees. */
		;
		float ConeOuterAngleInRadians = 6.283185f; /* 360 degrees. */
		;
		float ConeOuterGain = 0.0f;
		AudioListenerComponent() = default;
		AudioListenerComponent(const AudioListenerComponent& other) = default;
	};

} // namespace ForgottenEngine
