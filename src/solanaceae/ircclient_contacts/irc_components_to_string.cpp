#include "./irc_components_to_string.hpp"

#include "./components.hpp"

#include <entt/entity/registry.hpp>
#include <entt/entity/handle.hpp>

namespace Contact {

void registerIRCComponents2Str(ContactStore4I& cs) {
	cs.registerComponentToString(
		entt::type_id<Contact::Components::IRC::ServerName>().hash(),
		+[](ContactHandle4 c, bool verbose) -> std::string {
			return c.get<Contact::Components::IRC::ServerName>().name;
		},
		"Irc",
		"ServerName",
		entt::type_id<Contact::Components::IRC::ServerName>().name(),
		true
	);

	cs.registerComponentToString(
		entt::type_id<Contact::Components::IRC::ChannelName>().hash(),
		+[](ContactHandle4 c, bool verbose) -> std::string {
			return c.get<Contact::Components::IRC::ChannelName>().name;
		},
		"Irc",
		"ChannelName",
		entt::type_id<Contact::Components::IRC::ChannelName>().name(),
		true
	);

	cs.registerComponentToString(
		entt::type_id<Contact::Components::IRC::UserName>().hash(),
		+[](ContactHandle4 c, bool verbose) -> std::string {
			return c.get<Contact::Components::IRC::UserName>().name;
		},
		"Irc",
		"UserName",
		entt::type_id<Contact::Components::IRC::UserName>().name(),
		true
	);
}

void unregisterIRCComponents2Str(ContactStore4I& cs) {
	cs.unregisterComponentToString(
		entt::type_id<Contact::Components::IRC::UserName>().hash()
	);
	cs.unregisterComponentToString(
		entt::type_id<Contact::Components::IRC::ChannelName>().hash()
	);
	cs.unregisterComponentToString(
		entt::type_id<Contact::Components::IRC::UserName>().hash()
	);
}

} // Contact
