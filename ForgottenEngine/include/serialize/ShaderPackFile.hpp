#pragma once

#include "serialize/Serialization.hpp"
#include "StreamReader.hpp"
#include "StreamWriter.hpp"

namespace ForgottenEngine {

	struct ShaderPackFile {
		struct ShaderReflectionInfo {
			// Uniform Buffers
			// Storage Buffers
			// Push Constant Buffers
			// Sampled Images
			// Storage Images
		};

		struct ShaderData {
			uint8_t Stage;
			ShaderReflectionInfo ReflectionInfo;
			void* Data;
		};

		struct ShaderModuleInfo {
			uint64_t PackedOffset;
			uint64_t PackedSize; // size of data only
			uint8_t Version;
			uint8_t Stage;
			uint32_t Flags = 0;
			static void serialize(StreamWriter* writer, const ShaderModuleInfo& info) { writer->write_raw(info); }
			static void deserialize(StreamReader* reader, ShaderModuleInfo& info) { reader->read_raw(info); }
		};

		struct ShaderProgramInfo {
			uint64_t ReflectionDataOffset;
			std::vector<uint32_t> ModuleIndices;
		};

		struct ShaderIndex {
			std::unordered_map<uint32_t, ShaderProgramInfo> shader_programs; // Hashed shader name/path
			std::vector<ShaderModuleInfo> shader_modules;

			static uint64_t CalculateSizeRequirements(uint32_t programCount, uint32_t moduleCount)
			{
				return (sizeof(uint32_t) + sizeof(ShaderProgramInfo)) * programCount + sizeof(ShaderModuleInfo) * moduleCount;
			}
		};

		struct FileHeader {
			char HEADER[4] = { 'F', 'G', 'S', 'P' };
			uint32_t Version = 1;
			uint32_t ShaderProgramCount, ShaderModuleCount;
		};

		FileHeader header;
		ShaderIndex index;
		ShaderData* data;
	};

} // namespace ForgottenEngine
