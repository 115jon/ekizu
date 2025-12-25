#include <ekizu/attachment.hpp>
#include <ekizu/json_util.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const Attachment &a) {
	serialize(j, "id", a.id);
	serialize(j, "filename", a.filename);
	serialize(j, "description", a.description);
	serialize(j, "content_type", a.content_type);
	serialize(j, "size", a.size);
	serialize(j, "url", a.url);
	serialize(j, "proxy_url", a.proxy_url);
	serialize(j, "height", a.height);
	serialize(j, "width", a.width);
	serialize(j, "ephemeral", a.ephemeral);
}

void from_json(const nlohmann::json &j, Attachment &a) {
	deserialize(j, "id", a.id);
	deserialize(j, "filename", a.filename);
	deserialize(j, "description", a.description);
	deserialize(j, "content_type", a.content_type);
	deserialize(j, "size", a.size);
	deserialize(j, "url", a.url);
	deserialize(j, "proxy_url", a.proxy_url);
	deserialize(j, "height", a.height);
	deserialize(j, "width", a.width);
	deserialize(j, "ephemeral", a.ephemeral);
}

void to_json(nlohmann::json &j, const PartialAttachment &a) {
	serialize(j, "id", a.id);
	serialize(j, "filename", a.filename);
	serialize(j, "description", a.description);
}

void from_json(const nlohmann::json &j, PartialAttachment &attachment) {
	deserialize(j, "id", attachment.id);
	deserialize(j, "filename", attachment.filename);
	deserialize(j, "description", attachment.description);
}
}  // namespace ekizu
