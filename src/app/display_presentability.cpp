#include "th03/app/display_presentability.hpp"

namespace th03::app {

std::optional<std::uint64_t> SourcePresentationMailbox::publish_source(
	bool host_presentable
) noexcept
{
	host_presentable_ = host_presentable;
	newest_source_publication_ = source_publication_count_++;
	return host_presentable_ ? newest_source_publication_ : std::nullopt;
}

std::optional<std::uint64_t> SourcePresentationMailbox::refresh_host(
	bool host_presentable
) noexcept
{
	host_presentable_ = host_presentable;
	if(!host_presentable_ || !submission_pending()) {
		return std::nullopt;
	}
	return newest_source_publication_;
}

bool SourcePresentationMailbox::acknowledge_host_submission(
	std::uint64_t publication
) noexcept
{
	if(!host_presentable_ || !submission_pending() ||
		!newest_source_publication_ ||
		*newest_source_publication_ != publication) {
		return false;
	}
	submitted_publication_ = publication;
	++host_submission_count_;
	return true;
}

bool SourcePresentationMailbox::host_presentable() const noexcept
{
	return host_presentable_;
}

bool SourcePresentationMailbox::submission_pending() const noexcept
{
	return newest_source_publication_ != submitted_publication_;
}

std::uint64_t SourcePresentationMailbox::source_publication_count() const noexcept
{
	return source_publication_count_;
}

std::uint64_t SourcePresentationMailbox::host_submission_count() const noexcept
{
	return host_submission_count_;
}

std::optional<std::uint64_t>
SourcePresentationMailbox::newest_source_publication() const noexcept
{
	return newest_source_publication_;
}

std::optional<std::uint64_t>
SourcePresentationMailbox::submitted_publication() const noexcept
{
	return submitted_publication_;
}

} // namespace th03::app
