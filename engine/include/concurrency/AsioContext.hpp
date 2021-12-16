#pragma once

#include <filesystem>
#include <span>

#include <asio.hpp>
#include <unifex/file_concepts.hpp>
#include <unifex/receiver_concepts.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/task.hpp>
#include <unifex/on.hpp>
#include <unifex/let_error.hpp>
#include <unifex/just_error.hpp>

#include "util/EnumFlags.hpp"





enum class FileIOFlags
{
    None  = 0,
    Read  = 1 << 1,
    Write = 1 << 2,
};

NG_MAKE_ENUM_FLAGS(FileIOFlags);


template<FileIOFlags FLAGS>
class AsyncFile;

/**
 * Context capable of async file IO using uring/iocp. This should be replaced with a custom implementation further
 * down the line probably.
 */
class AsioContext
{
public:
    class Scheduler;
    class ScheduleSender;
    class ReadSender;

    AsioContext();

    ~AsioContext();

    Scheduler get_scheduler();

private:
    friend AsyncFile<FileIOFlags::Read> tag_invoke(
        unifex::tag_t<unifex::open_file_read_only>,
        Scheduler s, const std::filesystem::path& path);

private:
    asio::io_context context_;
    std::thread polling_thread_;
};

class AsioContext::ScheduleSender
{
public:
	template <
	    template <typename...> class Variant,
	    template <typename...> class Tuple>
	using value_types = Variant<Tuple<>>;

	template <template <typename...> class Variant>
	using error_types = Variant<>;

	static constexpr bool sends_done = true;

    ScheduleSender(asio::io_context& context)
	    : context_{&context}
    {
    }

private:
    template<class Receiver>
    class Operation
    {
    public:
        Operation(asio::io_context& c, auto&& rec)
            : context_{c}
            , receiver_{std::forward<decltype(rec)>(rec)}
        {
        }

        void start() noexcept
        {
            asio::dispatch(context_, [this](){ receiver_.set_value(); });
        }

    private:
	    asio::io_context& context_;
        Receiver receiver_;
    };


private:
    asio::io_context* context_;
};

class AsioContext::Scheduler
{
public:
    explicit Scheduler(asio::io_context& ctx) : context_{&ctx} {}

    ScheduleSender schedule() const
    {
	    return ScheduleSender(*context_);
    }

private:
    friend class AsioContext;

    
    friend AsyncFile<FileIOFlags::Read> tag_invoke(
        unifex::tag_t<unifex::open_file_read_only>,
        Scheduler s, const std::filesystem::path& path);

    friend bool operator==(const Scheduler&, const Scheduler&) = default;

    // TODO: schedule_at

private:
    asio::io_context* context_;
};

inline AsioContext::Scheduler AsioContext::get_scheduler()
{
    return Scheduler{context_};
}

class AsioContext::ReadSender
{
    template<class Receiver>
    class Operation
    {
    public:
        template<class Receiver2>
        Operation(asio::random_access_file& file, std::uint64_t offset, std::span<std::byte> buffer, Receiver2&& receiver)
            : file_{file}
            , offset_{offset}
            , buffer_{buffer}
            , receiver_{std::forward<Receiver2>(receiver)}
        {
            
        }

        void start() noexcept
        {
            file_.async_read_some_at(offset_,
                asio::mutable_buffer(buffer_.data(), buffer_.size()),
                [this](asio::error_code errc, std::size_t n)
                    { on_read_complete(errc, n); });
        }

    private:
        void on_read_complete(asio::error_code errc, std::size_t n) noexcept
        {
            if (errc)
            {
                unifex::set_error(std::move(receiver_), errc);
            }
            else
            {
                if constexpr (noexcept(unifex::set_value(std::move(receiver_), n)))
                {
                    unifex::set_value(std::move(receiver_), n);
                }
                else
                {
                    try
                    {
                        unifex::set_value(std::move(receiver_), n);
                    }
                    catch (...)
                    {
                        unifex::set_error(std::move(receiver_), std::current_exception());
                    }
                }
            }
        }

    private:
        asio::random_access_file& file_;
        std::uint64_t offset_;
        std::span<std::byte> buffer_;
        Receiver receiver_;
    };

public:
    // Produces number of bytes read.
    template <
        template <typename...> class Variant,
        template <typename...> class Tuple>
    using value_types = Variant<Tuple<std::size_t>>;

    // Note: Only case it might complete with exception_ptr is if the
    // receiver's set_value() exits with an exception.
    template <template <typename...> class Variant>
    using error_types = Variant<asio::error_code, std::exception_ptr>;

    static constexpr bool sends_done = true;
    
    // TODO: this should be private, but friending AsyncFile does not work for some reason...
    ReadSender(asio::random_access_file& file, std::uint64_t offset, std::span<std::byte> buffer)
        : file_{&file}
        , offset_{offset}
        , buffer_{buffer}
    {}
    
    template <typename Receiver>
    Operation<std::remove_cvref_t<Receiver>> connect(Receiver&& r) &&
    {
        return Operation<std::remove_cvref_t<Receiver>>{*file_, offset_, buffer_, std::forward<Receiver>(r)};
    }

private:
    asio::random_access_file* file_;
    std::uint64_t offset_;
    std::span<std::byte> buffer_;
};

template<FileIOFlags FLAGS>
class AsyncFile
{
public:
    using offset_t = std::uint64_t;

    AsyncFile(asio::random_access_file file)
	    : file_{std::move(file)}
    {}

private:
    friend AsioContext::ReadSender tag_invoke(
        unifex::tag_t<unifex::async_read_some_at>,
        AsyncFile& file,
        offset_t offset,
        std::span<std::byte> buffer) noexcept
        requires (static_cast<bool>(FLAGS & FileIOFlags::Read))
    {
        return AsioContext::ReadSender{file.file_, offset, buffer};
    }

    // TODO: write

public:
    // Runs on the asio context
    unifex::task<std::vector<std::byte>> read()
        requires (static_cast<bool>(FLAGS & FileIOFlags::Read))
    {
        // TODO: rewrite without coroutines
        std::vector<std::byte> result(file_.size());

        // hack for simplicity
        AsioContext::Scheduler sched(static_cast<asio::io_context&>(file_.get_executor().context()));

        offset_t already_read = 0;
        while (already_read < result.size())
        {
            std::span buf(result.data() + already_read, result.data() + result.size());
            
	        already_read += co_await
        		unifex::let_error(unifex::async_read_some_at(*this, already_read, buf),
                [](auto&&)
				{
					return unifex::just_error(std::runtime_error("IO error idk"));
				});
        }

        co_return result;
    }

private:
    asio::random_access_file file_;
};
