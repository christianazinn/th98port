#ifndef TH03_PLATFORM_CURL_RUNTIME_INTERNAL_HPP
#define TH03_PLATFORM_CURL_RUNTIME_INTERNAL_HPP

#include <curl/curl.h>

namespace th03::platform::detail {

class CurlRuntime final {
public:
	CurlRuntime() noexcept : ready_(
		curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK
	) {}
	~CurlRuntime() { if(ready_) { curl_global_cleanup(); } }

	CurlRuntime(const CurlRuntime &) = delete;
	CurlRuntime &operator=(const CurlRuntime &) = delete;

	[[nodiscard]] bool ready() const noexcept { return ready_; }

private:
	bool ready_ = false;
};

// Function-local statics in an inline function are shared across translation
// units. Update and online workers therefore own one process-wide libcurl
// lifetime even when both adapters are used concurrently.
[[nodiscard]] inline bool curl_runtime_ready() noexcept
{
	static CurlRuntime runtime;
	return runtime.ready();
}

} // namespace th03::platform::detail

#endif
