#pragma once

#include <filesystem>

#include <unifex/task.hpp>
#include <tiny_gltf.h>

#include "assets/AssetHandle.hpp"


class AssetSubsystem
{
public:
	struct CreateInfo
	{
		// TODO: archive support
		// Can only be a folder for now
		std::filesystem::path base_path;
	};

	explicit AssetSubsystem(CreateInfo info);

	unifex::task<tinygltf::Model> loadModel(AssetHandle handle);

private:
	std::filesystem::path base_path_;
	tinygltf::TinyGLTF loader_;
};
