#ifndef EKIZU_REQUEST_CREATE_INVITE_HPP
#define EKIZU_REQUEST_CREATE_INVITE_HPP

#include <ekizu/http.hpp>
#include <ekizu/invite.hpp>
#include <ekizu/request/request_sender.hpp>

namespace ekizu {
struct CreateInviteFields {
	std::optional<uint32_t> max_age;
	std::optional<uint32_t> max_uses;
	std::optional<bool> temporary;
	std::optional<bool> unique;
	std::optional<TargetType> target_type;
	std::optional<Snowflake> target_user_id;
	std::optional<Snowflake> target_application_id;
};

EKIZU_EXPORT void to_json(nlohmann::json &j, const CreateInviteFields &f);
EKIZU_EXPORT void from_json(const nlohmann::json &j, CreateInviteFields &f);

struct CreateInvite {
	CreateInvite(RequestSender sender, Snowflake channel_id);

	EKIZU_EXPORT operator net::HttpRequest() const;

	// TODO: Add validation
	CreateInvite &max_age(uint32_t max_age) {
		m_fields.max_age = max_age;
		return *this;
	}

	CreateInvite &max_uses(uint32_t max_uses) {
		m_fields.max_uses = max_uses;
		return *this;
	}

	CreateInvite &temporary(bool temporary) {
		m_fields.temporary = temporary;
		return *this;
	}

	CreateInvite &unique(bool unique) {
		m_fields.unique = unique;
		return *this;
	}

	CreateInvite &target_type(TargetType target_type) {
		m_fields.target_type = target_type;
		return *this;
	}

	CreateInvite &target_user_id(Snowflake target_user_id) {
		m_fields.target_user_id = target_user_id;
		return *this;
	}

	CreateInvite &target_application_id(Snowflake target_application_id) {
		m_fields.target_application_id = target_application_id;
		return *this;
	}

	template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(Result<Invite>))
				  CompletionToken>
	auto send(CompletionToken &&token) const {
		return asio::async_initiate<CompletionToken, void(Result<Invite>)>(
			[this](auto &&handler) {
				m_sender.send<Invite>(
					*this, std::forward<decltype(handler)>(handler));
			},
			token);
	}

   private:
	Snowflake m_channel_id;
	CreateInviteFields m_fields;
	RequestSender m_sender;
};
}  // namespace ekizu

#endif	// EKIZU_REQUEST_CREATE_INVITE_HPP
