#include "concurrency/AsioContext.hpp"


AsioContext::AsyncFile<FileIOFlags::Read> tag_invoke(
	unifex::tag_t<unifex::open_file_read_only>,
	AsioContext::Scheduler s, const std::filesystem::path& path)
{
	return AsioContext::AsyncFile<FileIOFlags::Read>(*s.context_,
		asio::stream_file(
			s.context_->context_, path.string().c_str(),
			asio::stream_file::read_only
		));
}
