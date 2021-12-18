#include "assets/AssetSubsystem.hpp"

#include <unifex/on.hpp>

#include "core/EngineHandle.hpp"


AssetSubsystem::AssetSubsystem(CreateInfo info)
	: base_path_{info.base_path}
{
}

unifex::task<tinygltf::Model> AssetSubsystem::loadModel(AssetHandle handle)
{
	co_await unifex::schedule(g_engine.blockingScheduler());

	auto asset_path = base_path_ / handle.path;

	auto ext = handle.path.extension().string();

	tinygltf::Model result;
	std::string error;
	std::string warn;
	bool res;

	if (ext == ".gltf")
	{
		res = loader_.LoadASCIIFromFile(&result, &error, &warn, asset_path.string());
	}
	else if (ext == ".glb")
	{
		res = loader_.LoadBinaryFromFile(&result, &error, &warn, asset_path.string());
	}
	else
	{
		throw std::runtime_error("Unsupported model format!");
	}

	if (!res)
	{
		throw std::runtime_error(error);
	}

	if (!warn.empty())
	{
		spdlog::warn("Asset {} loaded with warnings: {}", asset_path.string(), warn);
	}

	co_return result;
}
