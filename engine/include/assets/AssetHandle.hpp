#pragma once

#include <filesystem>


// TODO: replace with FName-like thing (from unreal)
struct AssetHandle
{
	std::filesystem::path path;

	friend bool operator==(const AssetHandle&, const AssetHandle&) = default;
};

namespace std
{
	template <>
	struct hash<AssetHandle>
	{
		size_t operator()(const AssetHandle& x) const noexcept
		{
			return hash_value(x.path);
		}
	};
}

