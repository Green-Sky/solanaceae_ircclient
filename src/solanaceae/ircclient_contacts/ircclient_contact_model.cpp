#include "./ircclient_contact_model.hpp"

#include "./components.hpp"

#include <solanaceae/contact/components.hpp>
#include <solanaceae/util/utils.hpp>

#include <libirc_rfcnumeric.h>
#include <libircclient.h>

#include <sodium/crypto_hash_sha256.h>

#include <cstdint>
#include <string_view>
#include <vector>
#include <iostream>

IRCClientContactModel::IRCClientContactModel(
	Contact3Registry& cr,
	ConfigModelI& conf,
	IRCClient1& ircc
) : _cr(cr), _conf(conf), _ircc(ircc) {
	_ircc.subscribe(this, IRCClient_Event::CONNECT);

	_ircc.subscribe(this, IRCClient_Event::NUMERIC);

	_ircc.subscribe(this, IRCClient_Event::JOIN);
	_ircc.subscribe(this, IRCClient_Event::PART);
	_ircc.subscribe(this, IRCClient_Event::TOPIC);
	_ircc.subscribe(this, IRCClient_Event::QUIT);

	_ircc.subscribe(this, IRCClient_Event::CTCP_REQ);

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

Contact3Handle IRCClientContactModel::getC(std::string_view channel) {
	const auto server_name = _ircc.getServerName();
	// TODO: this needs a better way
	for (const auto e : _cr.view<Contact::Components::IRC::ServerName, Contact::Components::IRC::ChannelName>()) {
		if (_cr.get<Contact::Components::IRC::ServerName>(e).name == server_name && _cr.get<Contact::Components::IRC::ChannelName>(e).name == channel) {
			return {_cr, e};
		}
	}

	return {_cr, entt::null};
}

Contact3Handle IRCClientContactModel::getU(std::string_view nick) {
	const auto server_name = _ircc.getServerName();
	// TODO: this needs a better way
	for (const auto e : _cr.view<Contact::Components::IRC::ServerName, Contact::Components::IRC::UserName>()) {
		if (_cr.get<Contact::Components::IRC::ServerName>(e).name == server_name && _cr.get<Contact::Components::IRC::UserName>(e).name == nick) {
			return {_cr, e};
		}
	}

	return {_cr, entt::null};
}

Contact3Handle IRCClientContactModel::getCU(std::string_view name) {
	if (name.empty()) {
		return {_cr, entt::null};
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

	{ // server
		if (!_cr.valid(_server)) {
			// check for empty contact by id
			for (const auto e : _cr.view<Contact::Components::ID>()) {
				if (_cr.get<Contact::Components::ID>(e).data == _server_hash) {
					_server = e;
					break;
				}
			}

			if (!_cr.valid(_server)) {
				_server = _cr.create();
				_cr.emplace_or_replace<Contact::Components::ID>(_server, _server_hash);
			}
		}

		_cr.emplace_or_replace<Contact::Components::ContactModel>(_server, this);
		_cr.emplace_or_replace<Contact::Components::IRC::ServerName>(_server, std::string{_ircc.getServerName()}); // really?
		_cr.emplace_or_replace<Contact::Components::Name>(_server, std::string{_ircc.getServerName()}); // TODO: add special string?

		// does this make sense ?
		_cr.emplace_or_replace<Contact::Components::ConnectionState>(_server, Contact::Components::ConnectionState::State::direct);

		_cr.emplace_or_replace<Contact::Components::TagBig>(_server);
		// the server connection is also the root contact (ircccm only handles 1 server 1 user)
		_cr.emplace_or_replace<Contact::Components::TagRoot>(_server);
		// TODO: should this be its own node instead? or should the server node be created on construction?
	}

	{ // self
		if (!_cr.valid(_self)) {
			// TODO: this can create self with peexisting id
			if (!e.params.empty()) {
				const auto self_hash = getIDHash(e.params.front());

				// check for empty contact by id
				for (const auto e : _cr.view<Contact::Components::ID>()) {
					if (_cr.get<Contact::Components::ID>(e).data == self_hash) {
						_self = e;
						break;
					}
				}
			}
			if (!_cr.valid(_self)) {
				_self = _cr.create();
			}
		}
		_cr.emplace_or_replace<Contact::Components::ContactModel>(_self, this);
		_cr.emplace_or_replace<Contact::Components::Parent>(_self, _server);
		_cr.emplace_or_replace<Contact::Components::TagSelfStrong>(_self);
		_cr.emplace_or_replace<Contact::Components::IRC::ServerName>(_self, std::string{_ircc.getServerName()}); // really?
		if (!e.params.empty()) {
			_cr.emplace_or_replace<Contact::Components::IRC::UserName>(_self, std::string{e.params.front()});
			_cr.emplace_or_replace<Contact::Components::Name>(_self, std::string{e.params.front()});
			// make id hash(hash(ServerName)+UserName)
			// or irc name format, but those might cause collisions
			_cr.emplace_or_replace<Contact::Components::ID>(_self, getIDHash(e.params.front()));

#if 0
			std::cout << "### created self with"
				<< " e:" << entt::to_integral(_self)
				<< " ircn:" << _cr.get<Contact::Components::IRC::UserName>(_self).name
				<< " ircsn:" << _cr.get<Contact::Components::IRC::ServerName>(_self).name
				<< " id:" << bin2hex(_cr.get<Contact::Components::ID>(_self).data)
				<< "\n";
#endif
		}

		_cr.emplace_or_replace<Contact::Components::ConnectionState>(_self, Contact::Components::ConnectionState::State::direct);

		// add self to server
		_cr.emplace_or_replace<Contact::Components::Self>(_server, _self);
	}

	// join queued
	while (!_join_queue.empty()) {
		irc_cmd_join(
			_ircc.getSession(),
			_join_queue.front().c_str(),
			""
		);
		_join_queue.pop();
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
		if (!channel.valid()) {
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
				if (!user.valid()) {
					const auto user_hash = getIDHash(user_str);
					// check for empty contact by id
					for (const auto e : _cr.view<Contact::Components::ID>()) {
						if (_cr.get<Contact::Components::ID>(e).data == user_hash) {
							user = {_cr, e};
							break;
						}
					}
					if (!user.valid()) {
						user = {_cr, _cr.create()};
						user.emplace_or_replace<Contact::Components::ID>(user_hash);
					}

					user.emplace_or_replace<Contact::Components::ContactModel>(this);
					user.emplace_or_replace<Contact::Components::IRC::ServerName>(std::string{_ircc.getServerName()});
					// channel list?
					// add to channel?
					user.emplace_or_replace<Contact::Components::IRC::UserName>(std::string{user_str});
					user.emplace_or_replace<Contact::Components::Name>(std::string{user_str});
				}

				if (user.entity() != _self) {
					user.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::cloud);
					user.emplace_or_replace<Contact::Components::Self>(_self);
				}

				{ // add user to channel
					auto& channel_user_list = channel.get_or_emplace<Contact::Components::ParentOf>().subs;
					if (std::find(channel_user_list.begin(), channel_user_list.end(), user) == channel_user_list.end()) {
						//std::cout << "!!!!!!!! new user in channel!\n";
						channel_user_list.push_back(user);
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
		if (!channel.valid()) {
			std::cerr << "IRCCCM error: topic for unknown channel\n";
			return false;
		}

		const auto topic = e.params.at(2);
		channel.emplace_or_replace<Contact::Components::StatusText>(std::string{topic}).fillFirstLineLength();
	}
	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Join& e) {
	if (e.params.empty()) {
		return false;
	}

	const auto& joined_channel_name = e.params.front();

	//std::cout << "JOIN!!!! " << e.origin << " in " << joined_channel_name << "\n";

	auto channel = getC(e.params.front());
	if (!channel.valid()) {
		const auto channel_hash = getIDHash(joined_channel_name);
		// check for empty contact by id
		for (const auto e : _cr.view<Contact::Components::ID>()) {
			if (_cr.get<Contact::Components::ID>(e).data == channel_hash) {
				channel = {_cr, e};
				break;
			}
		}
		if (!channel.valid()) {
			channel = {_cr, _cr.create()};
			channel.emplace_or_replace<Contact::Components::ID>(channel_hash);
		}
		channel.emplace_or_replace<Contact::Components::ContactModel>(this);
		channel.emplace_or_replace<Contact::Components::Parent>(_server);
		channel.emplace_or_replace<Contact::Components::ParentOf>(); // start empty
		channel.emplace_or_replace<Contact::Components::IRC::ServerName>(std::string{_ircc.getServerName()});
		channel.emplace_or_replace<Contact::Components::IRC::ChannelName>(std::string{joined_channel_name});
		channel.emplace_or_replace<Contact::Components::Name>(std::string{joined_channel_name});

		std::cout << "IRCCCM: joined '" << joined_channel_name << "' id:" << bin2hex(channel.get<Contact::Components::ID>().data) << "\n";

		channel.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::cloud);

		channel.emplace_or_replace<Contact::Components::TagBig>();
		channel.emplace_or_replace<Contact::Components::TagGroup>();

		// add self to channel
		channel.emplace_or_replace<Contact::Components::Self>(_self);
	}

	auto user = getU(e.origin);
	if (!user.valid()) {
		const auto user_hash = getIDHash(e.origin);
		// check for empty contact by id
		for (const auto e : _cr.view<Contact::Components::ID>()) {
			if (_cr.get<Contact::Components::ID>(e).data == user_hash) {
				user = {_cr, e};
				break;
			}
		}
		if (!user.valid()) {
			user = {_cr, _cr.create()};
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

		std::cout << "### created self(?) with"
			<< " ircn:" << _cr.get<Contact::Components::IRC::UserName>(_self).name
			<< " ircsn:" << _cr.get<Contact::Components::IRC::ServerName>(_self).name
			<< " id:" << bin2hex(_cr.get<Contact::Components::ID>(_self).data)
			<< "\n";
	}

	if (user.entity() != _self) {
		user.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::cloud);
		user.emplace_or_replace<Contact::Components::Self>(_self);
	}

	{ // add user to channel
		auto& channel_user_list = channel.get_or_emplace<Contact::Components::ParentOf>().subs;
		if (std::find(channel_user_list.begin(), channel_user_list.end(), user) == channel_user_list.end()) {
			//std::cout << "!!!!!!!! new user in channel!\n";
			channel_user_list.push_back(user);
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
	if (!user.valid()) {
		// ignoring unknown users, might be caused by a bug
		std::cerr << "ignoring unknown users, might be caused by a bug\n";
		return false;
	}

	// e.params.front() is the channel
	auto channel = getC(e.params.front());
	if (!channel.valid()) {
		// ignoring unknown channel, might be caused by a bug
		std::cerr << "ignoring unknown channel, might be caused by a bug\n";
		return false;
	}

	{ // remove user from channel
		auto& channel_user_list = channel.get_or_emplace<Contact::Components::ParentOf>().subs;
		if (auto it = std::find(channel_user_list.begin(), channel_user_list.end(), user); it != channel_user_list.end()) {
			//std::cout << "!!!!!!!! removing user from channel!\n";
			channel_user_list.erase(it);
		} else {
			//std::cout << "!!!!!!!! unknown user leaving channel!\n";
		}
	}

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
	if (!channel.valid()) {
		std::cerr << "IRCCCM error: new topic for unknown channel\n";
		return false;
	}

	const auto topic = e.params.at(1);
	channel.emplace_or_replace<Contact::Components::StatusText>(std::string{topic}).fillFirstLineLength();
	return false;
}

bool IRCClientContactModel::onEvent(const IRCClient::Events::Quit& e) {
	// e.origin // is the quitting user

	// e.params.front() is the quit reason

	auto user = getU(e.origin);
	if (!user.valid()) {
		// ignoring unknown users, might be caused by a bug
		return false;
	}

	if (user.entity() != _self) {
		user.emplace_or_replace<Contact::Components::ConnectionState>(Contact::Components::ConnectionState::State::disconnected);
	}

	// should we remove the user from the channel?

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
