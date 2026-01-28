#ifndef EKIZU_LOGGER_HPP
#define EKIZU_LOGGER_HPP

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <ekizu/export.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

namespace ekizu {

/// Get the global logger instance. Shared across all DLLs.
EKIZU_EXPORT std::shared_ptr<spdlog::logger> global_logger();

/// Initialize the global logger. Call before any code that logs.
/// @param log_level Optional override (e.g., from EKIZU_LOG_LEVEL env var)
EKIZU_EXPORT void init_logger(std::string_view log_level = {});

/// Log filter management for component-based filtering.
/// Components are identified by their prefix (e.g., "shard", "voice", "dave").
struct LogFilter {
	/// Get the singleton instance
	EKIZU_EXPORT static LogFilter &instance();

	/// Disable logging for a specific component prefix
	/// @param component Component name to disable (e.g., "shard", "voice")
	EKIZU_EXPORT void disable_component(std::string_view component);

	/// Enable logging for a specific component prefix (reverses disable)
	/// @param component Component name to enable
	EKIZU_EXPORT void enable_component(std::string_view component);

	/// Check if a component is enabled for logging
	/// @param prefix Full prefix string (e.g., "shard[0, 1]", "voice{123}")
	/// @return true if logging should proceed, false if filtered out
	EKIZU_EXPORT bool is_enabled(std::string_view prefix) const;

	/// Clear all filter rules (enable all components)
	EKIZU_EXPORT void clear();

	/// Set to whitelist mode (only log enabled components)
	/// @param components List of components to enable (all others disabled)
	EKIZU_EXPORT void set_whitelist(
		std::initializer_list<std::string_view> components);

   private:
	LogFilter() = default;

	mutable std::mutex m_mutex;
	std::unordered_set<std::string> m_disabled;
	std::unordered_set<std::string> m_whitelist;
	bool m_whitelist_mode{false};
};

/// Helper to configure log filtering via environment variable
/// Format: EKIZU_LOG_FILTER="-shard,-gateway" (disable shard and gateway)
/// Format: EKIZU_LOG_FILTER="+voice,+dave" (whitelist mode: only voice and
/// dave)
EKIZU_EXPORT void init_log_filter_from_env();

struct PrefixedLogger {
	std::string prefix;

	template <typename... Args>
	void trace(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(prefix)) {
			l->trace("{}: {}", prefix,
					 fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void debug(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(prefix)) {
			l->debug("{}: {}", prefix,
					 fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void info(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(prefix)) {
			l->info("{}: {}", prefix,
					fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void warn(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(prefix)) {
			l->warn("{}: {}", prefix,
					fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void error(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(prefix)) {
			l->error("{}: {}", prefix,
					 fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void critical(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(prefix)) {
			l->critical("{}: {}", prefix,
						fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}
};

template <typename ContextFn>
struct ContextLogger {
	std::string name;
	ContextFn context_fn;

	ContextLogger(std::string n, ContextFn fn)
		: name(std::move(n)), context_fn(std::move(fn)) {}

	[[nodiscard]] std::string prefix() const {
		return fmt::format("{}{{{}}}", name, context_fn());
	}

	template <typename... Args>
	void trace(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		auto p = prefix();
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(p)) {
			l->trace(
				"{}: {}", p, fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void debug(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		auto p = prefix();
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(p)) {
			l->debug(
				"{}: {}", p, fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void info(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		auto p = prefix();
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(p)) {
			l->info(
				"{}: {}", p, fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void warn(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		auto p = prefix();
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(p)) {
			l->warn(
				"{}: {}", p, fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void error(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		auto p = prefix();
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(p)) {
			l->error(
				"{}: {}", p, fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}

	template <typename... Args>
	void critical(fmt::format_string<Args...> fmtstr, Args &&...args) const {
		auto p = prefix();
		if (auto l = global_logger();
			l && LogFilter::instance().is_enabled(p)) {
			l->critical(
				"{}: {}", p, fmt::format(fmtstr, std::forward<Args>(args)...));
		}
	}
};

template <typename ContextFn>
ContextLogger(std::string, ContextFn) -> ContextLogger<ContextFn>;

}  // namespace ekizu

#endif	// EKIZU_LOGGER_HPP
