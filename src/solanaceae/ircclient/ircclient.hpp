#pragma once

#include <solanaceae/util/config_model.hpp>
#include <solanaceae/util/event_provider.hpp>

#include <iostream> // tmp

// fwd
struct irc_session_s;
using irc_session_t = irc_session_s;
extern "C" void* irc_get_ctx(irc_session_t* session);

namespace IRCClient::Events {

	// TODO: proper param separation

	struct Numeric {
		unsigned int event;
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Connect {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Nick {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Quit {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Join {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Part {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Mode {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct UMode {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Topic {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Kick {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Channel {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct PrivMSG {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Notice {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct ChannelNotice {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Invite {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct CTCP_Req {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct CTCP_Rep {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct CTCP_Action {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

	struct Unknown {
		std::string_view origin;
		std::vector<std::string_view> params;
	};

} // Events

enum class IRCClient_Event : uint32_t {
	NUMERIC,
	CONNECT,
	NICK,
	QUIT,
	JOIN,
	PART,
	MODE,
	UMODE,
	TOPIC,
	KICK,
	CHANNEL,
	PRIVMSG,
	NOTICE,
	CHANNELNOTICE,
	INVITE,

	CTCP_REQ,
	CTCP_REP,
	CTCP_ACTION,

	UNKNOWN,

	MAX
};

struct IRCClientEventI {
	using enumType = IRCClient_Event;

	virtual ~IRCClientEventI(void) {}

	virtual bool onEvent(const IRCClient::Events::Numeric&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Connect&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Nick&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Quit&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Join&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Part&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Mode&) { return false; }
	virtual bool onEvent(const IRCClient::Events::UMode&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Topic&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Kick&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Channel&) { return false; }
	virtual bool onEvent(const IRCClient::Events::PrivMSG&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Notice&) { return false; }
	virtual bool onEvent(const IRCClient::Events::ChannelNotice&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Invite&) { return false; }
	virtual bool onEvent(const IRCClient::Events::CTCP_Req&) { return false; }
	virtual bool onEvent(const IRCClient::Events::CTCP_Rep&) { return false; }
	virtual bool onEvent(const IRCClient::Events::CTCP_Action&) { return false; }
	virtual bool onEvent(const IRCClient::Events::Unknown&) { return false; }
};

using IRCClientEventProviderI = EventProviderI<IRCClientEventI>;

// one network per instance only
class IRCClient1 : public IRCClientEventProviderI {
	ConfigModelI& _conf;

	irc_session_t* _irc_session {nullptr};
	bool _try_connecting_state {false};
	float _try_connecting_cooldown {0.f};

	bool _event_fired {false};

	std::string _server_name; // name of the irc network this iirc is connected to

	public:
		IRCClient1(
			ConfigModelI& conf
		);

		~IRCClient1(void);


		// tmp
		void run(void);
		float iterate(float delta);

		// raw access
		irc_session_t* getSession(void);

		const std::string_view getServerName(void) const;

		// join
		void join(std::string_view channel);

	private:
		// connects an already existing session
		void connectSession(void);

	private: // callbacks for libircclient
		static void on_event_numeric(irc_session_t* session, unsigned int event, const char* origin, const char** params, unsigned int count);

		template<IRCClient_Event event_type_enum, typename EventType>
		static void on_event_generic_new(irc_session_t* session, const char* event, const char* origin, const char** params, unsigned int count) {
			std::vector<std::string_view> params_view;
			for (size_t i = 0; i < count; i++) {
				params_view.push_back(params[i]);
			}

			std::cout << "IRC: event " << event << " " << origin << "\n";

#if 0
			if (std::string_view{event} == "ACTION") {
				std::cout << " -action is " << typeid(EventType).name() << "\n";
				std::cout << " -enum is " << (int)event_type_enum << "\n";
			}
#endif

			for (const auto it : params_view) {
				std::cout << "  " << it << "\n";
			}

			auto* ircc = static_cast<IRCClient1*>(irc_get_ctx(session));
			assert(ircc != nullptr);

			ircc->dispatch(event_type_enum, EventType{origin, params_view});
			ircc->_event_fired = true;
		}
};

