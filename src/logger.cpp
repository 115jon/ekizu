#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>
#include <cstring>
#include <ekizu/logger.hpp>

namespace ekizu {

namespace {
std::shared_ptr<spdlog::logger> s_global_logger;
}  // namespace

std::shared_ptr<spdlog::logger> global_logger() { return s_global_logger; }

void init_logger(std::string_view log_level_override) {
	// Skip if already initialized
	if (s_global_logger) { return; }

	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");

	auto file_sink =
		std::make_shared<spdlog::sinks::basic_file_sink_mt>("ekizu.log");
	file_sink->set_level(spdlog::level::trace);

#ifdef _DEBUG
	auto level = spdlog::level::debug;
#else
	auto level = spdlog::level::info;
#endif

	// Check override parameter first, then env var
	std::string_view log_level = log_level_override;
	if (log_level.empty()) {
		if (const auto *env = std::getenv("EKIZU_LOG_LEVEL")) {
			log_level = env;
		}
	}

	if (!log_level.empty()) {
		if (log_level == "info") {
			level = spdlog::level::info;
		} else if (log_level == "warn") {
			level = spdlog::level::warn;
		} else if (log_level == "error") {
			level = spdlog::level::err;
		} else if (log_level == "debug") {
			level = spdlog::level::debug;
		} else if (log_level == "trace") {
			level = spdlog::level::trace;
		} else if (log_level == "critical") {
			level = spdlog::level::critical;
		} else if (log_level == "off") {
			level = spdlog::level::off;
		}
	}

	console_sink->set_level(level);

	s_global_logger = std::make_shared<spdlog::logger>(
		"ekizu", spdlog::sinks_init_list{console_sink, file_sink});
	s_global_logger->set_level(level);

	// Initialize log filter from environment
	init_log_filter_from_env();
}

// LogFilter implementation
LogFilter &LogFilter::instance() {
	static LogFilter s_instance;
	return s_instance;
}

void LogFilter::disable_component(std::string_view component) {
	std::lock_guard lk(m_mutex);
	m_whitelist_mode = false;
	m_disabled.insert(std::string(component));
}

void LogFilter::enable_component(std::string_view component) {
	std::lock_guard lk(m_mutex);
	if (m_whitelist_mode) {
		m_whitelist.insert(std::string(component));
	} else {
		m_disabled.erase(std::string(component));
	}
}

bool LogFilter::is_enabled(std::string_view prefix) const {
	std::lock_guard lk(m_mutex);

	// Extract component name from prefix (e.g., "shard[0, 1]" -> "shard")
	auto bracket_pos = prefix.find_first_of("[{");
	std::string_view component =
		bracket_pos != std::string_view::npos
			? prefix.substr(0, bracket_pos)
			: prefix;

	if (m_whitelist_mode) {
		// In whitelist mode, only enabled components are logged
		return std::any_of(
			m_whitelist.begin(), m_whitelist.end(),
			[component](const auto &w) { return component == w; });
	}

	// In blacklist mode, check if component is disabled
	return !std::any_of(m_disabled.begin(), m_disabled.end(),
						[component](const auto &d) { return component == d; });
}

void LogFilter::clear() {
	std::lock_guard lk(m_mutex);
	m_disabled.clear();
	m_whitelist.clear();
	m_whitelist_mode = false;
}

void LogFilter::set_whitelist(
	std::initializer_list<std::string_view> components) {
	std::lock_guard lk(m_mutex);
	m_whitelist_mode = true;
	m_whitelist.clear();
	m_disabled.clear();
	for (auto c : components) { m_whitelist.insert(std::string(c)); }
}

void init_log_filter_from_env() {
	const auto *env = std::getenv("EKIZU_LOG_FILTER");
	if ((env == nullptr) || std::strlen(env) == 0) { return; }

	std::string filter_str(env);
	bool whitelist_mode{};

	// Check if we're in whitelist mode (all entries start with +)
	if (!filter_str.empty() && filter_str[0] == '+') { whitelist_mode = true; }

	// Parse comma-separated entries
	size_t pos{};
	while (pos < filter_str.size()) {
		auto comma = filter_str.find(',', pos);
		if (comma == std::string::npos) { comma = filter_str.size(); }

		auto entry = filter_str.substr(pos, comma - pos);
		// Trim whitespace
		while (!entry.empty() &&
			   (entry.front() == ' ' || entry.front() == '\t')) {
			entry = entry.substr(1);
		}
		while (!entry.empty() &&
			   (entry.back() == ' ' || entry.back() == '\t')) {
			entry.pop_back();
		}

		if (!entry.empty()) {
			if (entry[0] == '-') {
				// Disable component
				LogFilter::instance().disable_component(entry.substr(1));
			} else if (entry[0] == '+') {
				// Whitelist mode
				if (!whitelist_mode) {
					LogFilter::instance().clear();
					whitelist_mode = true;
				}
				LogFilter::instance().enable_component(entry.substr(1));
			} else {
				// No prefix - treat as disable
				LogFilter::instance().disable_component(entry);
			}
		}

		pos = comma + 1;
	}

	// If whitelist mode, set it
	if (whitelist_mode) {
		// Already handled via enable_component calls
	}
}

}  // namespace ekizu
