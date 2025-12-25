#include <boost/url/encode.hpp>
#include <boost/url/rfc/unreserved_chars.hpp>
#include <ekizu/request/delete_user_reaction.hpp>

namespace ekizu {
DeleteUserReaction::DeleteUserReaction(
	RequestSender sender, Snowflake channel_id, Snowflake message_id,
	RequestReaction emoji, Snowflake user_id)
	: m_channel_id{channel_id},
	  m_emoji{std::move(emoji)},
	  m_message_id{message_id},
	  m_user_id{user_id},
	  m_sender{sender} {}

DeleteUserReaction::operator net::HttpRequest() const {
	auto url = std::visit(
		[this](auto &&emoji) {
			using T = std::decay_t<decltype(emoji)>;

			if constexpr (std::is_same_v<T, CustomEmoji>) {
				return fmt::format(
					"/channels/{}/messages/{}/reactions/{}/{}", m_channel_id,
					m_message_id,
					boost::urls::encode(
						fmt::format("{}:{}", emoji.name, emoji.id),
						boost::urls::unreserved_chars),
					m_user_id);
			} else if constexpr (std::is_same_v<T, std::string>) {
				return fmt::format(
					"/channels/{}/messages/{}/reactions/{}/{}", m_channel_id,
					m_message_id,
					boost::urls::encode(emoji, boost::urls::unreserved_chars),
					m_user_id);
			} else {
				static_assert(sizeof(T) == 0);
			}
		},
		m_emoji);

	return {net::HttpMethod::delete_, url, 11};
}
}  // namespace ekizu