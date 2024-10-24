#pragma once

#include <solanaceae/contact/contact_model3.hpp>
#include <solanaceae/util/config_model.hpp>

#include <solanaceae/ircclient/ircclient.hpp>

#include <vector>
#include <string>
#include <queue>
#include <cstdint>

#include <iostream> // tmp

class IRCClientContactModel : public IRCClientEventI, public ContactModel3I {
	Contact3Registry& _cr;
	ConfigModelI& _conf;
	IRCClient1& _ircc;
	IRCClient1::SubscriptionReference _ircc_sr;

	// cm needs the connect event to happen
	bool _connected {false};

	std::vector<uint8_t> _server_hash; // cached for id gen
	Contact3 _server = entt::null;
	Contact3 _self = entt::null;

	// used if not connected
	std::queue<std::string> _join_queue;

	public:
		IRCClientContactModel(
			Contact3Registry& cr,
			ConfigModelI& conf,
			IRCClient1& ircc
		);

		virtual ~IRCClientContactModel(void);

		void join(const std::string& channel);

	private:
		// just the hash algo
		std::vector<uint8_t> getHash(std::string_view value);
		std::vector<uint8_t> getHash(const std::vector<uint8_t>& value);

	public:
		// the actually ID is a chain containing the server+channel or server+name
		// eg: hash(hash(ServerName)+ChannelName)
		std::vector<uint8_t> getIDHash(std::string_view name);

		Contact3Handle getC(std::string_view channel);
		Contact3Handle getU(std::string_view nick);
		// user or channel using channel prefix
		Contact3Handle getCU(std::string_view name);

	private: // ircclient
		bool onEvent(const IRCClient::Events::Connect& e) override;
		bool onEvent(const IRCClient::Events::Numeric& e) override;
		bool onEvent(const IRCClient::Events::Join& e) override;
		bool onEvent(const IRCClient::Events::Part& e) override;
		bool onEvent(const IRCClient::Events::Topic& e) override;
		bool onEvent(const IRCClient::Events::Quit& e) override;
		bool onEvent(const IRCClient::Events::CTCP_Req&) override;
		bool onEvent(const IRCClient::Events::Disconnect&) override;
};
