#include "concurrency/AsioContext.hpp"

#include <spdlog/spdlog.h>

#include "util/Assert.hpp"


AsioContext::AsioContext()
	: polling_thread_{[this]()
	{
		for (;;)
		{
			try
			{
				context_.run();
				// Run ended gracefully
				break;
			}
			catch (std::exception& e)
			{
				spdlog::error("Exception thrown in ASIO context: {}", e.what());
			}
			catch (...)
			{
				NG_PANIC("Something incomprehensible thrown in ASIO context. Terminating immediately.");
			}
		}
	}}
{
}

AsioContext::~AsioContext()
{
	context_.stop();
	polling_thread_.join();
}

AsyncFile<FileIOFlags::Read> tag_invoke(
	unifex::tag_t<unifex::open_file_read_only>,
	AsioContext::Scheduler s, const std::filesystem::path& path)
{
	return AsyncFile<FileIOFlags::Read>(
		asio::random_access_file(
			*s.context_, path.string().c_str(),
			asio::stream_file::read_only
		));
}
