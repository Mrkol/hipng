#include "assets/AssetSubsystem.hpp"

#include <unifex/on.hpp>

#include "core/EngineHandle.hpp"


AssetSubsystem::AssetSubsystem(CreateInfo info)
	: base_path_{info.base_path}
{
}

unifex::task<tinygltf::Model> AssetSubsystem::loadModel(std::filesystem::path relative_path)
{
	tinygltf::TinyGLTF loader;

	// TODO: Trick tinygltf saying stuff was loaded, but don't actually load anything
	// accumulate required files here, then load them ASYNCHRONOUSLY, then put them back into the model
	// should be EZ for buffers, but a bit harder for images (due to parsing)
	loader.SetFsCallbacks(tinygltf::FsCallbacks{
		.FileExists = nullptr,
		.ExpandFilePath = nullptr,
		.ReadWholeFile = nullptr,
		.WriteWholeFile = nullptr,
		.user_data = nullptr
	});

	auto asset_path = base_path_ / relative_path;

	// note: throws on error
	auto file = unifex::open_file_read_only(g_engine.ioScheduler(), asset_path);
	auto data = co_await file.read();

	auto ext = relative_path.extension().string();


	tinygltf::Model result;
	std::string error;
	std::string warn;
	bool res;

	if (ext == ".gltf")
	{
		res = loader.LoadASCIIFromString(&result, &error, &warn,
			reinterpret_cast<const char*>(data.data()), data.size(),
			asset_path.parent_path().string());
	}
	else if (ext == ".glb")
	{
		res = loader.LoadBinaryFromMemory(&result, &error, &warn,
			reinterpret_cast<const unsigned char*>(data.data()), data.size(),
			asset_path.parent_path().string());
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
}
