#pragma once

#define FG_SERIALIZE_PROPERTY(propName, propVal, outputNode) \
	outputNode << YAML::Key << #propName << YAML::Value << propVal

#define FG_SERIALIZE_PROPERTY_ASSET(propName, propVal, outputData) \
	outputData << YAML::Key << #propName << YAML::Value << ((propVal) ? (uint64_t)(propVal)->Handle : 0);

#define FG_DESERIALIZE_PROPERTY(propName, destination, node, defaultValue) \
	destination = (node)[#propName] ? (node)[#propName].as<decltype(defaultValue)>() : defaultValue

#define FG_DESERIALIZE_PROPERTY_ASSET(propName, destination, inputData, assetClass)                   \
	{                                                                                                 \
		AssetHandle assetHandle = (inputData)[#propName] ? (inputData)[#propName].as<uint64_t>() : 0; \
		if (AssetManager::IsAssetHandleValid(assetHandle)) {                                          \
			(destination) = AssetManager::GetAsset<assetClass>(assetHandle);                          \
		} else {                                                                                      \
			FG_CORE_ERROR_TAG("AssetManager", "Tried to load invalid asset {0}.", #assetClass);       \
		}                                                                                             \
	}