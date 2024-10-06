#include "./ircclient_message_manager.hpp"

#include <solanaceae/ircclient_contacts/components.hpp>

#include <solanaceae/contact/components.hpp>
#include <solanaceae/message3/components.hpp>
#include <solanaceae/message3/registry_message_model.hpp>

#include <libirc_rfcnumeric.h>
#include <libircclient.h>

#include "./string_view_split.hpp"

#include <cstdint>
#include <string_view>
#include <vector>
#include <iostream>

IRCClientMessageManager::IRCClientMessageManager(
	RegistryMessageModelI& rmm,
	Contact3Registry& cr,
	ConfigModelI& conf,
	IRCClient1& ircc,
	IRCClientContactModel& ircccm
) : _rmm(rmm), _cr(cr), _conf(conf), _ircc(ircc), _ircccm(ircccm) {
	_ircc.subscribe(this, IRCClient_Event::CHANNEL);
	_ircc.subscribe(this, IRCClient_Event::PRIVMSG);
	_ircc.subscribe(this, IRCClient_Event::NOTICE);
	_ircc.subscribe(this, IRCClient_Event::CHANNELNOTICE);
	_ircc.subscribe(this, IRCClient_Event::CTCP_ACTION);

	_rmm.subscribe(this, RegistryMessageModel_Event::send_text);
}

IRCClientMessageManager::~IRCClientMessageManager(void) {
}

bool IRCClientMessageManager::processMessage(Contact3Handle from, Contact3Handle to, std::string_view message_text, bool action) {
	const uint64_t ts = Message::getTimeMS();

	Message3Registry* reg_ptr = nullptr;
	if (to.all_of<Contact::Components::TagSelfStrong>()) {
		reg_ptr = _rmm.get(from);
	} else {
		reg_ptr = _rmm.get(to);
	}
	if (reg_ptr == nullptr) {
		std::cerr << "IRCCMM error: cant find reg\n";
		return false;
	}

	// TODO: check for existence, hs or other syncing mechanics might have sent it already (or like, it arrived 2x or whatever)
	auto new_msg = Message3Handle{*reg_ptr, reg_ptr->create()};

	{ // contact
		// from
		new_msg.emplace<Message::Components::ContactFrom>(from);

		// to
		new_msg.emplace<Message::Components::ContactTo>(to);
	}

	// no message id :(

	new_msg.emplace<Message::Components::MessageText>(message_text);

	if (action) {
		new_msg.emplace<Message::Components::TagMessageIsAction>();
	}

	new_msg.emplace<Message::Components::TimestampProcessed>(ts);
	new_msg.emplace<Message::Components::Timestamp>(ts); // reactive?

	new_msg.emplace<Message::Components::TagUnread>();

	_rmm.throwEventConstruct(new_msg);
	return false;
}

bool IRCClientMessageManager::sendText(const Contact3 c, std::string_view message, bool action) {
	if (!_cr.valid(c)) {
		return false;
	}

	if (message.empty()) {
		return false; // TODO: empty messages allowed?
	}

	const uint64_t ts = Message::getTimeMS();

	if (_cr.all_of<Contact::Components::TagSelfStrong>(c)) {
		return false; // message to self? not with irc
	}

	// test for contact irc specific components
	// TODO: what about commands and server messages?
	if (!_cr.any_of<Contact::Components::IRC::UserName, Contact::Components::IRC::ChannelName>(c)) {
		return false;
	}

	std::string to_str;
	if (_cr.all_of<Contact::Components::IRC::UserName>(c)) {
		to_str = _cr.get<Contact::Components::IRC::UserName>(c).name;
	} else {
		to_str = _cr.get<Contact::Components::IRC::ChannelName>(c).name;
	}

	auto* reg_ptr = _rmm.get(c);
	if (reg_ptr == nullptr) {
		return false; // nope
	}

	if (!_cr.all_of<Contact::Components::Self>(c)) {
		std::cerr << "IRCCMM error: cant get self\n";
		return false;
	}

	{ // actually send
		// split message by line
		for (const auto& inner_str : MM::std_utils::split(message, "\n")) {
			if (inner_str.empty()) {
				continue;
			}

			std::string tmp_message{inner_str}; // string_view might not be nul terminated
			if (action) {
				if (irc_cmd_me(_ircc.getSession(), to_str.c_str(), tmp_message.c_str()) != 0) {
					std::cerr << "IRCCMM error: failed to send action\n";

					// we dont have offline messaging in irc
					return false;
				}
			} else {
				if (irc_cmd_msg(_ircc.getSession(), to_str.c_str(), tmp_message.c_str()) != 0) {
					std::cerr << "IRCCMM error: failed to send message\n";

					// we dont have offline messaging in irc
					return false;
				}
			}
		}
	}

	const Contact3 c_self = _cr.get<Contact::Components::Self>(c).self;

	auto new_msg = Message3Handle{*reg_ptr, reg_ptr->create()};

	new_msg.emplace<Message::Components::ContactFrom>(c_self);
	new_msg.emplace<Message::Components::ContactTo>(c);

	new_msg.emplace<Message::Components::MessageText>(message);

	if (action) {
		new_msg.emplace<Message::Components::TagMessageIsAction>();
	}

	new_msg.emplace<Message::Components::TimestampWritten>(ts);
	new_msg.emplace<Message::Components::Timestamp>(ts); // reactive?
	new_msg.emplace<Message::Components::TimestampProcessed>(ts);

	// mark as read
	new_msg.emplace<Message::Components::Read>(ts); // reactive?

	_rmm.throwEventConstruct(new_msg);
	return true;
}

bool IRCClientMessageManager::onEvent(const IRCClient::Events::Channel& e) {
	if (e.params.size() < 2) {
		std::cerr << "IRCCMM error: channel event too few params\n";
		return false;
	}


	// e.origin is sender
	auto sender =  _ircccm.getU(e.origin); // assuming its always a user // aka ContactFrom
	if (!sender.valid()) {
		std::cerr << "IRCCMM error: channel event unknown sender\n";
		return false;
	}

	// e.params.at(0) is channel
	auto channel =  _ircccm.getC(e.params.at(0)); // aka ContactTo
	if (!channel.valid()) {
		std::cerr << "IRCCMM error: channel event unknown channel\n";
		return false;
	}

	// e.params.at(1) is message
	const auto& message_text = e.params.at(1);

	return processMessage(sender, channel, message_text, false);
}

bool IRCClientMessageManager::onEvent(const IRCClient::Events::PrivMSG& e) {
	if (e.params.size() < 2) {
		std::cerr << "IRCCMM error: privmsg event too few params\n";
		return false;
	}

	// e.origin is sender
	auto from =  _ircccm.getU(e.origin); // assuming its always a user // aka ContactFrom
	if (!from.valid()) {
		std::cerr << "IRCCMM error: privmsg event unknown sender\n";
		return false;
	}

	// e.params.at(0) is receiver (us?)
	auto to =  _ircccm.getU(e.params.at(0)); // aka ContactTo
	if (!to.valid()) {
		std::cerr << "IRCCMM error: privmsg event unknown channel\n";
		return false;
	}

	// TODO: move this to contact
	// upgrade contact to big
	from.emplace_or_replace<Contact::Components::TagBig>(); // could be like an invite?
	from.emplace_or_replace<Contact::Components::TagPrivate>();

	return processMessage(from, to, e.params.at(1), false);
}

bool IRCClientMessageManager::onEvent(const IRCClient::Events::Notice& e) {
	if (e.params.size() < 2) {
		std::cerr << "IRCCMM error: notice event too few params\n";
		return false;
	}

	// server message type 1
		// e.origin is server host (not network name)
		// e.params.at(0) is '*'
	// server message type 2
		// e.origin is server host (not network name)
		// e.params.at(0) is user (us)
	// server message type 3
		// e.origin is "Global"
		// e.params.at(0) is user (us)
	// user message (private)
		// e.origin is sending user
		// e.params.at(0) is user (us)

	// e.params.at(1) is message

	return false;
}

bool IRCClientMessageManager::onEvent(const IRCClient::Events::ChannelNotice& e) {
	if (e.params.size() < 2) {
		std::cerr << "IRCCMM error: channel notice event too few params\n";
		return false;
	}

	// e.origin is sending user (probably)
	auto from = _ircccm.getU(e.origin);
	if (!from.valid()) {
		std::cerr << "IRCCMM error: channel notice event unknown sender\n";
		return false;
	}

	// e.params.at(0) is channel
	auto to = _ircccm.getC(e.params.at(0));
	if (!to.valid()) {
		std::cerr << "IRCCMM error: unknown receiver\n";
		return false;
	}

	// TODO: add notice tag

	// e.params.at(1) is message
	return processMessage(from, to, e.params.at(1), false);
}

bool IRCClientMessageManager::onEvent(const IRCClient::Events::CTCP_Action& e) {
	if (e.params.size() < 2) {
		std::cerr << "IRCCMM error: action event too few params\n";
		return false;
	}

	// e.origin is sender
	auto from = _ircccm.getU(e.origin); // assuming its always a user // aka ContactFrom
	if (!from.valid()) {
		std::cerr << "IRCCMM error: channel event unknown sender\n";
		return false;
	}

	// e.params.at(0) is receiver (self if pm or channel if channel)
	auto receiver = _ircccm.getCU(e.params.at(0));
	if (!receiver.valid()) {
		std::cerr << "IRCCMM error: unknown receiver\n";
		return false;
	}
	// e.params.at(1) is message

	// upgrade contact to big
	if (receiver.all_of<Contact::Components::IRC::UserName>()) {
		from.emplace_or_replace<Contact::Components::TagBig>(); // could be like an invite?
	}

	return processMessage(from, receiver, e.params.at(1), true);
}

