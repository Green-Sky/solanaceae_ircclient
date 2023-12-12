#pragma once

#include <string>

namespace Contact::Components::IRC {

	struct ServerName {
		std::string name;
	};

	struct ChannelName {
		std::string name;
	};

	struct UserName {
		std::string name;
	};

	// TODO:
	// - membership level in channels
	// - dcc stuff
	// - tags for server channel user?

} // Contact::Components::IRC

#include "./components_id.inl"

