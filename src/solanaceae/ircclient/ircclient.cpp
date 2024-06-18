#include "./ircclient.hpp"

#include <libircclient.h>
#include <libirc_rfcnumeric.h>

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#include <string_view>

void IRCClient1::on_event_numeric(irc_session_t* session, unsigned int event, const char* origin, const char** params, unsigned int count) {
	std::vector<std::string_view> params_view;
	for (size_t i = 0; i < count; i++) {
		params_view.push_back(params[i]);
	}

#if 0
	std::cout << "IRC: event_numeric " << event << " " << origin << "\n";

	for (const auto it : params_view) {
		std::cout << "  " << it << "\n";
	}
#endif

	auto ircc = static_cast<IRCClient1*>(irc_get_ctx(session));
	ircc->dispatch(IRCClient_Event::NUMERIC, IRCClient::Events::Numeric{event, origin, params_view});
	ircc->_event_fired = true;
}

IRCClient1::IRCClient1(
	ConfigModelI& conf
) : _conf(conf) {

	static irc_callbacks_t cb{};

	cb.event_numeric = on_event_numeric;

#define IRC_CB_G(x0, x1, x2) cb.x0 = on_event_generic_new<IRCClient_Event::x1, IRCClient::Events::x2>;

	//cb.event_connect = on_event_generic_new<IRCClientContactModel_Event::CONNECT, IRCClient::Events::Connect>;

	IRC_CB_G(event_connect, CONNECT, Connect);
	IRC_CB_G(event_nick, NICK, Nick);
	IRC_CB_G(event_quit, QUIT, Quit);
	IRC_CB_G(event_join, JOIN, Join);
	IRC_CB_G(event_part, PART, Part);
	IRC_CB_G(event_mode, MODE, Mode);
	IRC_CB_G(event_umode, UMODE, UMode);
	IRC_CB_G(event_topic, TOPIC, Topic);
	IRC_CB_G(event_kick, KICK, Kick);

	IRC_CB_G(event_channel, CHANNEL, Channel);

	IRC_CB_G(event_privmsg, PRIVMSG, PrivMSG);
	IRC_CB_G(event_notice, NOTICE, Notice);
	IRC_CB_G(event_channel_notice, CHANNELNOTICE, ChannelNotice);
	IRC_CB_G(event_invite, INVITE, Invite);

	IRC_CB_G(event_ctcp_req, CTCP_REQ, CTCP_Req);
	IRC_CB_G(event_ctcp_rep, CTCP_REP, CTCP_Rep);
	IRC_CB_G(event_ctcp_action, CTCP_ACTION, CTCP_Action);

	IRC_CB_G(event_unknown, UNKNOWN, Unknown);

#undef IRC_CB_G

	// TODO: dcc
	//irc_event_dcc_chat_t		event_dcc_chat_req;
	//irc_event_dcc_send_t		event_dcc_send_req;


	_irc_session = irc_create_session(&cb);
	irc_set_ctx(_irc_session, this);

	irc_option_set(_irc_session, LIBIRC_OPTION_DEBUG);
	irc_option_set(_irc_session, LIBIRC_OPTION_STRIPNICKS);
	irc_option_set(_irc_session, LIBIRC_OPTION_SSL_NO_VERIFY); // why

	connectSession();
}

IRCClient1::~IRCClient1(void) {
	irc_destroy_session(_irc_session);
}

// tmp
void IRCClient1::run(void) {
	if (irc_run(_irc_session) != 0) {
		std::cerr << "error failed to run: " << irc_strerror(irc_errno(_irc_session)) << "\n";
	}
}

float IRCClient1::iterate(float delta) {
	//if ( session->state != LIBIRC_STATE_CONNECTING )
	//{
		//session->lasterror = LIBIRC_ERR_STATE;
		//return 1;
	//}

	if (!irc_is_connected(_irc_session)) {
		if (_try_connecting_state) {
			// try to connect, every 20sec
			_try_connecting_cooldown -= delta;
			if (_try_connecting_cooldown <= 0.f) {
				std::cerr << "IRCC: trying to connect\n";
				connectSession();
			}
		} else {
			std::cerr << "IRCC error: not connected, trying to reconnect\n";

			dispatch(IRCClient_Event::DISCONNECT, IRCClient::Events::Disconnect{});
			connectSession(); // potentially enters trying phase
		}
		return 0.5f;
	}

	_event_fired = false;

	struct timeval tv;
	fd_set in_set, out_set;
	int maxfd = 0;

	//tv.tv_usec = 20000; // 20ms
	tv.tv_usec = 1000; // 1ms
	tv.tv_sec = 0;

	// Init sets
	FD_ZERO(&in_set);
	FD_ZERO(&out_set);

	if (irc_add_select_descriptors(_irc_session, &in_set, &out_set, &maxfd) != 0) {
		std::cerr << "IRCC error: adding select descriptors\n";
	}

	if (select(maxfd + 1, &in_set, &out_set, 0, &tv) < 0) {
		std::cerr << "IRCC error: select returned error\n";
#if 0
		if (socket_error() == EINTR) {
			//continue;
			return;
		}
#endif

		//session->lasterror = LIBIRC_ERR_TERMINATED;
		//return 1;
		return 0.1f;
	}

	if (irc_process_select_descriptors(_irc_session, &in_set, &out_set) != 0) {
		std::cerr << "IRCC error: processing socket select\n";
		//return 1;
	}

	// TODO: handle dcc
	if (_event_fired) {
		return 0.1f;
	} else {
		return 1.f;
	}
}

irc_session_t* IRCClient1::getSession(void) {
	return _irc_session;
}

const std::string_view IRCClient1::getServerName(void) const {
	return _server_name;
}

void IRCClient1::join(std::string_view channel) {
	assert(false && "implement me");
}

void IRCClient1::connectSession(void) {
	_try_connecting_state = true;
	_try_connecting_cooldown = 20.f;

	// reset connection
	// only closes potentially open sockets and sets state to init
	// nothing else is touched
	irc_disconnect(_irc_session);

	// TODO: do we need to set this every time?
	if (!_conf.has_string("IRCClient", "server")) {
		std::cerr << "IRCC error: no irc server in config!!\n";
		throw std::runtime_error("missing server in config");
	}

	// if server is prefixed with '#', its ssl
	std::string server = _conf.get_string("IRCClient", "server").value();
	_server_name = server; // TODO: find a better solution
	int64_t port = _conf.get_int("IRCClient", "port").value_or(6660);
	// TODO: password

	std::string nick;
	if (_conf.has_string("IRCClient", "nick")) {
		nick = _conf.get_string("IRCClient", "nick").value();
	} else {
		nick = "solanaceae_guest_" + std::to_string(std::random_device{}() % 10'000);
	}

	std::string username;
	if (_conf.has_string("IRCClient", "username")) {
		username = _conf.get_string("IRCClient", "username").value();
	} else {
		username = nick + "_";
	}

	std::string realname;
	if (_conf.has_string("IRCClient", "realname")) {
		realname = _conf.get_string("IRCClient", "realname").value();
	} else {
		realname = username + "_";
	}

	if (irc_connect(_irc_session, server.c_str(), port, nullptr, nick.c_str(), username.c_str(), realname.c_str()) != 0) {
		std::cerr << "IRCC error: failed to connect: (" << irc_errno(_irc_session) << ") " << irc_strerror(irc_errno(_irc_session)) << "\n";

		irc_disconnect(_irc_session);

		//throw std::runtime_error("failed to connect to irc");
		return;
	}

	_try_connecting_state = false;
}

