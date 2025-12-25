#ifndef EKIZU_REQUEST_MODIFY_GUILD_HPP
#define EKIZU_REQUEST_MODIFY_GUILD_HPP

#include <ekizu/guild.hpp>
#include <ekizu/http.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct ModifyGuildFields {
	std::optional<std::string> name;
	std::optional<std::string> region;
	std::optional<VerificationLevel> verification_level;
	std::optional<DefaultMessageNotificationLevel>
		default_message_notifications;
	std::optional<ExplicitContentFilter> explicit_content_filter;
	std::optional<Snowflake> afk_channel_id;
	std::optional<uint64_t> afk_timeout;
	std::optional<std::string> icon;
	std::optional<Snowflake> owner_id;
	std::optional<std::string> splash;
	std::optional<std::string> discovery_splash;
	std::optional<std::string> banner;
	std::optional<Snowflake> system_channel_id;
	std::optional<SystemChannelFlags> system_channel_flags;
	std::optional<Snowflake> rules_channel_id;
	std::optional<Snowflake> public_updates_channel_id;
	std::optional<std::string> preferred_locale;
	std::optional<std::vector<std::string>> features;
	std::optional<std::string> description;
	std::optional<bool> premium_progress_bar_enabled;
	std::optional<Snowflake> safety_alerts_channel_id;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const ModifyGuildFields &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, ModifyGuildFields &f);

struct ModifyGuild {
	ModifyGuild(RequestSender sender, Snowflake guild_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	ModifyGuild &afk_channel_id(Snowflake afk_channel_id) {
		m_fields.afk_channel_id = afk_channel_id;
		return *this;
	}

	ModifyGuild &afk_timeout(uint64_t afk_timeout) {
		m_fields.afk_timeout = afk_timeout;
		return *this;
	}

	ModifyGuild &banner(std::string banner) {
		m_fields.banner = std::move(banner);
		return *this;
	}

	ModifyGuild &default_message_notifications(
		DefaultMessageNotificationLevel default_message_notifications) {
		m_fields.default_message_notifications = default_message_notifications;
		return *this;
	}

	ModifyGuild &description(std::string description) {
		m_fields.description = std::move(description);
		return *this;
	}

	ModifyGuild &discovery_splash(std::string discovery_splash) {
		m_fields.discovery_splash = std::move(discovery_splash);
		return *this;
	}

	ModifyGuild &explicit_content_filter(
		ExplicitContentFilter explicit_content_filter) {
		m_fields.explicit_content_filter = explicit_content_filter;
		return *this;
	}

	ModifyGuild &features(const std::vector<std::string> &features) {
		m_fields.features = features;
		return *this;
	}

	ModifyGuild &icon(std::string icon) {
		m_fields.icon = std::move(icon);
		return *this;
	}

	ModifyGuild &name(std::string name) {
		m_fields.name = std::move(name);
		return *this;
	}

	ModifyGuild &owner_id(Snowflake owner_id) {
		m_fields.owner_id = owner_id;
		return *this;
	}

	ModifyGuild &preferred_locale(std::string preferred_locale) {
		m_fields.preferred_locale = std::move(preferred_locale);
		return *this;
	}

	ModifyGuild &premium_progress_bar_enabled(
		bool premium_progress_bar_enabled) {
		m_fields.premium_progress_bar_enabled = premium_progress_bar_enabled;
		return *this;
	}

	ModifyGuild &public_updates_channel_id(
		Snowflake public_updates_channel_id) {
		m_fields.public_updates_channel_id = public_updates_channel_id;
		return *this;
	}

	ModifyGuild &region(std::string region) {
		m_fields.region = std::move(region);
		return *this;
	}

	ModifyGuild &rules_channel_id(Snowflake rules_channel_id) {
		m_fields.rules_channel_id = rules_channel_id;
		return *this;
	}

	ModifyGuild &safety_alerts_channel_id(Snowflake safety_alerts_channel_id) {
		m_fields.safety_alerts_channel_id = safety_alerts_channel_id;
		return *this;
	}

	ModifyGuild &splash(std::string splash) {
		m_fields.splash = std::move(splash);
		return *this;
	}

	ModifyGuild &system_channel_flags(SystemChannelFlags system_channel_flags) {
		m_fields.system_channel_flags = system_channel_flags;
		return *this;
	}

	ModifyGuild &system_channel_id(Snowflake system_channel_id) {
		m_fields.system_channel_id = system_channel_id;
		return *this;
	}

	ModifyGuild &verification_level(VerificationLevel verification_level) {
		m_fields.verification_level = verification_level;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Guild>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Guild>)>(
			[this](auto &&handler) {
				m_sender.send<Guild>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_guild_id;
	ModifyGuildFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_MODIFY_GUILD_HPP
