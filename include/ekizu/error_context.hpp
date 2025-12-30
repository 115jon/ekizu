#ifndef EKIZU_ERROR_CONTEXT_HPP
#define EKIZU_ERROR_CONTEXT_HPP

#include <ekizu/export.hpp>
#include <string>
#include <string_view>

namespace ekizu {

// Best-effort debugging context, stored per-thread (GetLastError-style).
// Note: concurrent async operations on the same thread may overwrite this.
extern thread_local std::string g_last_error_context;

EKIZU_EXPORT void clear_error_context() noexcept;

EKIZU_EXPORT void set_error_context(std::string ctx);

EKIZU_EXPORT std::string_view last_error_context() noexcept;

}  // namespace ekizu

#endif	// EKIZU_ERROR_CONTEXT_HPP
