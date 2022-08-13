#pragma once

#include "AssetTypes.hpp"
#include "UUID.hpp"

namespace ForgottenEngine {

	using AssetHandle = UUID;

	class Asset : public ReferenceCounted {
	public:
		AssetHandle handle = 0;
		uint16_t flags = (uint16_t)AssetFlag::None;

		virtual ~Asset() { }

		static AssetType get_static_type() { return AssetType::None; }
		virtual AssetType get_asset_type() const { return AssetType::None; }

		bool is_valid() const { return ((flags & (uint16_t)AssetFlag::Missing) | (flags & (uint16_t)AssetFlag::Invalid)) == 0; }

		virtual bool operator==(const Asset& other) const { return handle == other.handle; }

		virtual bool operator!=(const Asset& other) const { return !(*this == other); }

		bool is_flag_set(AssetFlag flag) const { return (uint16_t)flag & flags; }
		void set_flag(AssetFlag flag, bool value = true)
		{
			if (value)
				flags |= (uint16_t)flag;
			else
				flags &= ~(uint16_t)flag;
		}
	};

	class PhysicsMaterial : public Asset {
	public:
		float StaticFriction;
		float DynamicFriction;
		float Bounciness;

		PhysicsMaterial() = default;
		PhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness)
			: StaticFriction(staticFriction)
			, DynamicFriction(dynamicFriction)
			, Bounciness(bounciness)
		{
		}

		static AssetType get_static_type() { return AssetType::PhysicsMat; }
		AssetType get_asset_type() const override { return get_static_type(); }
	};

	class AudioFile : public Asset {
	public:
		double Duration;
		uint32_t SamplingRate;
		uint16_t BitDepth;
		uint16_t NumChannels;
		uint64_t FileSize;

		AudioFile() = default;
		AudioFile(double duration, uint32_t samplingRate, uint16_t bitDepth, uint16_t numChannels, uint64_t fileSize)
			: Duration(duration)
			, SamplingRate(samplingRate)
			, BitDepth(bitDepth)
			, NumChannels(numChannels)
			, FileSize(fileSize)
		{
		}

		static AssetType get_static_type() { return AssetType::Audio; }
		AssetType get_asset_type() const override { return get_static_type(); }
	};
} // namespace ForgottenEngine
