#include "./ircclient_contact_model.hpp"

#include "./components.hpp"

#include <solanaceae/contact/contact_store_i.hpp>
#include <solanaceae/contact/components.hpp>
#include <solanaceae/util/utils.hpp>

#include <libirc_rfcnumeric.h>
#include <libircclient.h>

#include <entt/entity/registry.hpp>
#include <entt/entity/handle.hpp>

#include <sodium/crypto_hash_sha256.h>

#include <cstdint>
#include <string_view>
#include <vector>
#include <iostream>

IRCClientContactModel::IRCClientContactModel(
	ContactStore4I& cs,
	ConfigModelI& conf,
	IRCClient1& ircc
) : _cs(cs), _conf(conf), _ircc(ircc), _ircc_sr(_ircc.newSubRef(this)) {
	_ircc_sr
		.subscribe(IRCClient_Event::CONNECT)

		.subscribe(IRCClient_Event::NUMERIC)

		.subscribe(IRCClient_Event::JOIN)
		.subscribe(IRCClient_Event::PART)
		.subscribe(IRCClient_Event::TOPIC)
		.subscribe(IRCClient_Event::QUIT)

		.subscribe(IRCClient_Event::CTCP_REQ)

		.subscribe(IRCClient_Event::DISCONNECT)
	;

	// dont create server self etc until connect event comes

	for (const auto& [channel, should_join] : _conf.entries_bool("IRCClient", "autojoin")) {
		if (should_join) {
			std::cout << "IRCCCM: autojoining " << channel << "\n";
			join(channel);
		}
	}
}

IRCClientContactModel::~IRCClientContactModel(void) {
}

void IRCClientContactModel::join(const std::string& channel) {
	if (_connected) {
		irc_cmd_join(
			_ircc.getSession(),
			channel.c_str(),
			""
		);
		std::cout << "IRCCCM: connected joining channel...\n";
	} else {
		_join_queue.push(channel);
		std::cout << "IRCCCM: not connected yet, queued join...\n";
	}
}

bool IRCClientContactModel::addContact(Contact4 c) {
	return false;
}

bool IRCClientContactModel::acceptRequest(Contact4 c, std::string_view self_name, std::string_view password) {
	return false;
}

bool IRCClientContactModel::leave(Contact4 c, std::string_view reason) {
	return false;
}

std::vector<uint8_t> IRCClientContactModel::getHash(std::string_view value) {
	assert(!value.empty());

	std::vector<uint8_t> hash(crypto_hash_sha256_bytes(), 0x00);
	crypto_hash_sha256(hash.data(), reinterpret_cast<const uint8_t*>(value.data()), value.size());
	return hash;
}

std::vector<uint8_t> IRCClientContactModel::getHash(const std::vector<uint8_t>& value) {
	assert(!value.empty());

	std::vector<uint8_t> hash(crypto_hash_sha256_bytes(), 0x00);
	crypto_hash_sha256(hash.data(), value.data(), value.size());
	return hash;
}

std::vector<uint8_t> IRCClientContactModel::getIDHash(std::string_view name) {
	assert(!_server_hash.empty());
	assert(!name.empty());

	std::vector<uint8_t> data = _server_hash;
	data.insert(data.end(), name.begin(), name.end());
	return getHash(data);
}

ContactHandle4 IRCClientContactModel::getC(std::string_view channel) {
	const auto server_name = _ircc.getServerName();
	const auto& cr = _cs.registry();
	// TODO: this needs a better way
	for (const auto e : cr.view<Contact::Components::IRC::ServerName, Contact::Components::IRC::ChannelName>()) {
		if (cr.get<Contact::Components::IRC::ServerName>(e).name == server_name && cr.get<Contact::Components::IRC::ChannelName>(e).name == channel) {
			return _cs.contactHandle(e);
		}
	}

	return {};
}

ContactHandle4 IRCClientContactModel::getU(std::string_view nick) {
	const auto server_name = _ircc.getServerName();
	const auto& cr = _cs.registry();
	// TODO: this needs a better way
	for (const auto e : cr.view<Contact::Components::IRC::ServerName, Contact::Components::IRC::UserName>()) {
		if (cr.get<Contact::Components::IRC::ServerName>(e).name == server_name && cr.get<Contact::Components::IRC::UserName>(e).name == nick) {
			return _cs.contactHandle(e);
		}
	}

	return {};
}

ContactHandle4 IRCClientContactModel::getCU(std::string_view name) {
	if (name.empty()) {
		return {};
	}

	static constexpr std::string_view channel_prefixes{
		// rfc 1459 1.3
		"&" // local
		"#" // regular

		// rfc 2812 1.3
		"+"
		"!"
	};

	if (channel_prefixes.find(name.front()) != std::string_view::npos) {
		return getC(name);
	} else {
		return getU(name);
	}
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Connect& e) {
	_server_hash = getHash(_ircc.getServerName());
	_connected = true;

	auto& cr = _cs.registry();

	bool server_contact_created {false};
	{ // server
		if (!cr.valid(_server)) {
			// check for empty contact by id
			_server = _cs.getOneContactByID(ByteSpan{_server_hash});

			if (!cr.valid(_server)) {
				_server = cr.create();
				server_contact_created = true;
				cr.emplace_or_replace<Contact::Components::ID>(_server, _server_hash);
			}
		}

		cr.emplace_or_replace<Contact::Components::ContactModel>(_server, this);
		cr.emplace_or_replace<Contact::Components::IRC::ServerName>(_server, std::string{_ircc.getServerName()}); // really?
		cr.emplace_or_replace<Contact::Components::Name>(_server, std::string{_ircc.getServerName()}); // TODO: add special string?

		// does this make sense ?
		cr.emplace_or_replace<Contact::Components::ConnectionState>(_server, Contact::Components::ConnectionState::State::direct);

		cr.emplace_or_replace<Contact::Components::TagBig>(_server);
		// the server connection is also the root contact (ircccm only handles 1 server 1 user)
		cr.emplace_or_replace<Contact::Components::TagRoot>(_server);
		// TODO: should this be its own node instead? or should the server node be created on construction?
	}

	bool self_contact_created {false};
	{ // self
		if (!cr.valid(_self)) {
			// TODO: this can create self with peexisting id
			if (!e.params.empty()) {
				const auto self_hash = getIDHash(e.params.front());

				// check for empty contact by id
				_server = _cs.getOneContactByID(_server, ByteSpan{self_hash});
			}
			if (!cr.valid(_self)) {
				_self = cr.create();
				self_contact_created = true;
			}
		}
		cr.emplace_or_replace<Contact::Components::ContactModel>(_self, this);
		cr.emplace_or_replace<Contact::Components::Parent>(_self, _server);
		cr.emplace_or_replace<Contact::Components::TagSelfStrong>(_self);
		cr.emplace_or_replace<Contact::Components::IRC::ServerName>(_self, std::string{_ircc.getServerName()}); // really?
		if (!e.params.empty()) {
			cr.emplace_or_replace<Contact::Components::IRC::UserName>(_self, std::string{e.params.front()});
			cr.emplace_or_replace<Contact::Components::Name>(_self, std::string{e.params.front()});
			// make id hash(hash(ServerName)+UserName)
			// or irc name format, but those might cause collisions
			cr.emplace_or_replace<Contact::Components::ID>(_self, getIDHash(e.params.front()));

#if 0
			std::cout << "### created self with"
				<< " e:" << entt::to_integral(_self)
				<< " ircn:" << _cr.get<Contact::Components::IRC::UserName>(_self).name
				<< " ircsn:" << _cr.get<Contact::Components::IRC::ServerName>(_self).name
				<< " id:" << bin2hex(_cr.get<Contact::Components::ID>(_self).data)
				<< "\n";
#endif
		}

		cr.emplace_or_replace<Contact::Components::ConnectionState>(_self, Contact::Components::ConnectionState::State::direct);

		// add self to server
		cr.emplace_or_replace<Contact::Components::Self>(_server, _self);
	}

	// check for preexisting channels,
	// since this might be a reconnect
	// and reissue joins
	cr.view<Contact::Components::IRC::ServerName, Contact::Components::IRC::ChannelName>().each([this](const auto c, const auto& sn_c, const auto& cn_c) {
		// HACK: by name
		// should be by parent instead
		if (sn_c.name != _ircc.getServerName()) {
			return;
		}

		// TODO: implement join lol
		irc_cmd_join(
			_ircc.getSession(),
			cn_c.name.c_str(),
			""
		);
	});

	// join queued
	// TODO: merge with above
	while (!_join_queue.empty()) {
		// TODO: implement join lol
		irc_cmd_join(
			_ircc.getSession(),
			_join_queue.front().c_str(),
			""
		);
		_join_queue.pop();
	}

	if (server_contact_created) {
		_cs.throwEventConstruct(_server);
	} else {
		_cs.throwEventUpdate(_server);
	}

	if (self_contact_created) {
		_cs.throwEventConstruct(_self);
	} else {
		_cs.throwEventUpdate(_self);
	}

	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Numeric& e) {
	if (e.event == LIBIRC_RFC_RPL_NAMREPLY) {
		// user list
		// e.origin is the server
		// e.params.at(0) user (self)
		// e.params.at(1) =
		// e.params.at(2) channel
		// e.params.at(3) list of users space seperated with power prefixed
		if (e.params.size() != 4) {
			// error
			return false;
		}

		if (
			e.params.at(1) != "=" && // Public channel
			e.params.at(1) != "@" && // Secret channel
			e.params.at(1) != "*" // Private channel
		) {
			std::cerr << "IRCCCM error: name list for unknown channel type\n";
			return false;
		}

		const auto& channel_name = e.params.at(2);
		auto channel = getC(channel_name);
		if (!static_cast<bool>(channel)) {
			std::cerr << "IRCCCM error: name list for unknown channel\n";
			return false;
		}

		std::string_view user_list = e.params.at(3);

		std::string_view::size_type space_pos;
		do {
			space_pos = user_list.find_first_of(' ');
			auto user_str = user_list.substr(0, space_pos);
			{ // handle user
				// rfc 2812 5.1
				// The '@' and '+' characters next to the user name
				// indicate whether a client is a channel operator or
				// has been granted permission to speak on a moderated
				// channel.

				if (user_str.empty()) {
					std::cerr << "IRCCCM error: empty user\n";
					break;
				}

				// https://modern.ircdocs.horse/#channel-membership-prefixes
				static constexpr std::string_view membership_prefixes{
					"~" // founder
					"&" // protected
					"@" // operator
					"%" // half operator
					"+" // voice
				};
				if (membership_prefixes.find(user_str.front()) != std::string_view::npos) {
					switch (user_str.front()) {
						// TODO: use this info
						case '~': break;
						case '&': break;
						case '@': break;
						case '%': break;
						case '+': break;
					}
					user_str = user_str.substr(1);
				}

				if (user_str.empty()) {
					std::cerr << "IRCCCM error: empty user after removing membership prefix\n";
					break;
				}

				//std::cout << "u: " << user_str << "\n";

				auto user = getU(user_str);
				bool user_throw_event {false};
				bool user_created {false};
				if (!static_cast<bool>(user)) {
					const auto user_hash = getIDHash(user_str);
					// check for empty contact by id
					user = _cs.getOneContactByID(_server, ByteSpan{user_hash});
					if (!static_cast<bool>(user)) {
						user = _cs.contactHandle(_cs.registry().create());
						user_created = true;
						user.emplace_or_replace<Contact::Components::ID>(user_hash);
					}

					user.emplace_or_replace<Contact::Components::ContactModel>(this);
					user.emplace_or_replace<Contact::Components::IRC::ServerName>(std::string{_ircc.getServerName()});
					// channel list?
					// add to channel?
					user.emplace_or_replace<Contact::Components::IRC::UserName>(std::string{user_str});
					user.emplace_or_replace<Contact::Components::Name>(std::string{user_str});

					user_throw_event = true;
				}

				if (user.entity() != _self) {
					user.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::cloud);
					user.emplace_or_replace<Contact::Components::Self>(_self);

					user_throw_event = true;
				}

				{ // add user to channel
					auto& channel_user_list = channel.get_or_emplace<Contact::Components::ParentOf>().subs;
					if (std::find(channel_user_list.begin(), channel_user_list.end(), user) == channel_user_list.end()) {
						//std::cout << "!!!!!!!! new user in channel!\n";
						channel_user_list.push_back(user);
						user_throw_event = true;
					}
				}

				if (user_throw_event) {
					if (user_created) {
						_cs.throwEventConstruct(user);
					} else {
						_cs.throwEventUpdate(user);
					}
				}
			}

			if (space_pos == std::string_view::npos) {
				break;
			}

			// trim user
			user_list = user_list.substr(space_pos);
			const auto next_non_space = user_list.find_first_not_of(' ');
			if (next_non_space == std::string_view::npos) {
				break;
			}
			user_list = user_list.substr(next_non_space);
		} while (space_pos != std::string_view::npos);
	} else if (e.event == LIBIRC_RFC_RPL_TOPIC) {
		// origin is the server
		// params.at(0) is the user (self)
		// params.at(1) is the channel
		// params.at(2) is the topic
		if (e.params.size() != 3) {
			// error
			return false;
		}

		// TODO: check user (self)

		const auto channel_name = e.params.at(1);
		auto channel = getC(channel_name);
		if (!static_cast<bool>(channel)) {
			std::cerr << "IRCCCM error: topic for unknown channel\n";
			return false;
		}

		const auto topic = e.params.at(2);
		channel.emplace_or_replace<Contact::Components::StatusText>(std::string{topic}).fillFirstLineLength();
		_cs.throwEventUpdate(channel);
	}
	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Join& e) {
	if (e.params.empty()) {
		return false;
	}

	const auto& joined_channel_name = e.params.front();

	//std::cout << "JOIN!!!! " << e.origin << " in " << joined_channel_name << "\n";

	auto& cr = _cs.registry();

	bool channel_created {false};
	auto channel = getC(e.params.front());
	if (!static_cast<bool>(channel)) {
		const auto channel_hash = getIDHash(joined_channel_name);
		// check for empty contact by id
		channel = _cs.getOneContactByID(_server, ByteSpan{channel_hash});
		if (!static_cast<bool>(channel)) {
			//channel = {_cr, _cr.create()};
			channel = _cs.contactHandle(cr.create());
			channel_created = true;
			channel.emplace_or_replace<Contact::Components::ID>(channel_hash);
		}
		channel.emplace_or_replace<Contact::Components::ContactModel>(this);
		channel.emplace_or_replace<Contact::Components::Parent>(_server);
		cr.get_or_emplace<Contact::Components::ParentOf>(_server).subs.push_back(channel);
		channel.emplace_or_replace<Contact::Components::ParentOf>(); // start empty
		channel.emplace_or_replace<Contact::Components::IRC::ServerName>(std::string{_ircc.getServerName()});
		channel.emplace_or_replace<Contact::Components::IRC::ChannelName>(std::string{joined_channel_name});
		channel.emplace_or_replace<Contact::Components::Name>(std::string{joined_channel_name});

		std::cout << "IRCCCM: joined '" << joined_channel_name << "' id:" << bin2hex(channel.get<Contact::Components::ID>().data) << "\n";

		channel.emplace_or_replace<Contact::Components::TagBig>();
		channel.emplace_or_replace<Contact::Components::TagGroup>();

		// add self to channel
		channel.emplace_or_replace<Contact::Components::Self>(_self);
	}
	channel.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::cloud);

	if (channel_created) {
		_cs.throwEventConstruct(channel);
	} else {
		_cs.throwEventUpdate(channel);
	}

	auto user = getU(e.origin);
	bool user_throw_event {false};
	bool user_created {false};
	if (!static_cast<bool>(user)) {
		const auto user_hash = getIDHash(e.origin);
		// check for empty contact by id
		user = _cs.getOneContactByID(_server, ByteSpan{user_hash});
		if (!static_cast<bool>(user)) {
			user = _cs.contactHandle(cr.create());
			user_created = true;
			user.emplace_or_replace<Contact::Components::ID>(user_hash);
			std::cerr << "IRCCCM error: had to create joining user (self?)\n";
		}

		user.emplace_or_replace<Contact::Components::ContactModel>(this);
		user.emplace_or_replace<Contact::Components::Parent>(_server);
		user.emplace_or_replace<Contact::Components::IRC::ServerName>(std::string{_ircc.getServerName()});
		// channel list?
		// add to channel?
		user.emplace_or_replace<Contact::Components::IRC::UserName>(std::string{e.origin});
		user.emplace_or_replace<Contact::Components::Name>(std::string{e.origin});

		user_throw_event = true;

		// ???
		std::cout << "### created self(?) with"
			<< " ircn:" << cr.get<Contact::Components::IRC::UserName>(_self).name
			<< " ircsn:" << cr.get<Contact::Components::IRC::ServerName>(_self).name
			<< " id:" << bin2hex(cr.get<Contact::Components::ID>(_self).data)
			<< "\n";
	}

	if (user.entity() != _self) {
		user.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::cloud);
		user.emplace_or_replace<Contact::Components::Self>(_self);
		user_throw_event = true;
	}

	if (user_throw_event) {
		if (user_created) {
			_cs.throwEventConstruct(user);
		} else {
			_cs.throwEventUpdate(user);
		}
	}

	{ // add user to channel
		auto& channel_user_list = channel.get_or_emplace<Contact::Components::ParentOf>().subs;
		if (std::find(channel_user_list.begin(), channel_user_list.end(), user) == channel_user_list.end()) {
			//std::cout << "!!!!!!!! new user in channel!\n";
			channel_user_list.push_back(user);
			_cs.throwEventUpdate(channel);
		}
	}

	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Part& e) {
	if (e.params.size() < 1) {
		// error
		return false;
	}

	// e.origin // is the parting user
	auto user = getU(e.origin);
	if (!static_cast<bool>(user)) {
		// ignoring unknown users, might be caused by a bug
		std::cerr << "ignoring unknown users, might be caused by a bug\n";
		return false;
	}

	// e.params.front() is the channel
	auto channel = getC(e.params.front());
	if (!static_cast<bool>(channel)) {
		// ignoring unknown channel, might be caused by a bug
		std::cerr << "ignoring unknown channel, might be caused by a bug\n";
		return false;
	}

	{ // remove user from channel
		auto& channel_user_list = channel.get_or_emplace<Contact::Components::ParentOf>().subs;
		if (auto it = std::find(channel_user_list.begin(), channel_user_list.end(), user); it != channel_user_list.end()) {
			//std::cout << "!!!!!!!! removing user from channel!\n";
			channel_user_list.erase(it);
			_cs.throwEventUpdate(channel);
		} else {
			//std::cout << "!!!!!!!! unknown user leaving channel!\n";
		}
	}

	_cs.throwEventUpdate(user); // ??

	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Topic& e) {
	// sadly only fired on topic change

	// origin is user (setter)
	// params.at(0) is channel
	// params.at(1) is new topic
	if (e.params.size() != 2) {
		// error
		return false;
	}

	const auto channel_name = e.params.at(0);
	auto channel = getC(channel_name);
	if (!static_cast<bool>(channel)) {
		std::cerr << "IRCCCM error: new topic for unknown channel\n";
		return false;
	}

	const auto topic = e.params.at(1);
	channel.emplace_or_replace<Contact::Components::StatusText>(std::string{topic}).fillFirstLineLength();
	_cs.throwEventUpdate(channel);
	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Quit& e) {
	// e.origin // is the quitting user

	// e.params.front() is the quit reason

	auto user = getU(e.origin);
	if (!static_cast<bool>(user)) {
		// ignoring unknown users, might be caused by a bug
		return false;
	}

	if (user.entity() != _self) {
		user.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::disconnected);
	}

	// should we remove the user from the channel?

	_cs.throwEventUpdate(user);

	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::CTCP_Req& e) {
	if (e.params.size() < 1) {
		return false;
	}

	if (e.params.front() == "VERSION") {
		return false;
	}

	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Disconnect&) {
	_connected = false;
	auto& cr = _cs.registry();
	if (!cr.valid(_server)) {
		// skip if where already offline
		return false;
	}
	cr.get<Contact::Components::ConnectionState>(_server).state = Contact::Components::ConnectionState::disconnected;

	// HACK: by name
	ContactHandle4 server;
	cr.view<Contact::Components::IRC::ServerName, Contact::Components::ConnectionState>().each([this, &server](const auto c, const auto& sn_c, auto&) {
		// HACK: by name
		// should be by parent instead
		if (sn_c.name != _ircc.getServerName()) {
			return;
		}

		server = _cs.contactHandle(c);
	});

	if (!static_cast<bool>(server)) {
		return false;
	}

	server.get_or_emplace<Contact::Components::ConnectionState>().state = Contact::Components::ConnectionState::disconnected;

	_cs.throwEventUpdate(server);

	return false;
}

