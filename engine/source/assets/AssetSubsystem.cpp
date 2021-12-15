#include "assets/AssetSubsystem.hpp"

#include "core/EngineHandle.hpp"


AssetSubsystem::AssetSubsystem(CreateInfo info)
	: base_path_{info.base_path}
{
}

unifex::task<tinygltf::Model> AssetSubsystem::loadModel(std::filesystem::path relative_path)
{
	co_await unifex::schedule(g_engine.blockingScheduler());
}
