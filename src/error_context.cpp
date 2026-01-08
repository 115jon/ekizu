#include <ekizu/error_context.hpp>

namespace ekizu {

thread_local std::string g_last_error_context;

void clear_error_context() noexcept { g_last_error_context.clear(); }

void set_error_context(std::string ctx) {
	g_last_error_context = std::move(ctx);
}

std::string_view last_error_context() noexcept { return g_last_error_context; }

}  // namespace ekizu
