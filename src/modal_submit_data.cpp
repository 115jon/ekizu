#include <ekizu/json_util.hpp>
#include <ekizu/modal_submit_data.hpp>

namespace ekizu {
using json_util::deserialize;
using json_util::serialize;

void to_json(nlohmann::json &j, const ModalSubmitData &d) {
	serialize(j, "custom_id", d.custom_id);
	serialize(j, "components", d.components);
	serialize(j, "resolved", d.resolved);
}

void from_json(const nlohmann::json &j, ModalSubmitData &d) {
	deserialize(j, "custom_id", d.custom_id);
	deserialize(j, "components", d.components);
	deserialize(j, "resolved", d.resolved);
}
}  // namespace ekizu
