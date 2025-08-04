#pragma once

#include <solanaceae/contact/contact_model4.hpp>
#include <solanaceae/util/config_model.hpp>

#include <solanaceae/ircclient/ircclient.hpp>

#include <entt/entity/entity.hpp>

#include <vector>
#include <string>
#include <queue>
#include <cstdint>

class IRCClientContactModel : public IRCClientEventI, public ContactModel4I {
	ContactStore4I& _cs;
	ConfigModelI& _conf;
	IRCClient1& _ircc;
	IRCClient1::SubscriptionReference _ircc_sr;

	// cm needs the connect event to happen
	bool _connected {false};

	std::vector<uint8_t> _server_hash; // cached for id gen
	Contact4 _server {entt::null};
	Contact4 _self {entt::null};

	// used if not connected
	std::queue<std::string> _join_queue;

	public:
		IRCClientContactModel(
			ContactStore4I& cs,
			ConfigModelI& conf,
			IRCClient1& ircc
		);

		virtual ~IRCClientContactModel(void);

		void join(const std::string& channel);

	protected: // interface
		bool addContact(Contact4 c) override;
		bool acceptRequest(Contact4 c, std::string_view self_name, std::string_view password) override;
		bool leave(Contact4 c, std::string_view reason) override;
		bool invite(Contact4 c, Contact4 to) override;
		bool canInvite(Contact4 c, Contact4 to) override;

	private:
		// just the hash algo
		std::vector<uint8_t> getHash(std::string_view value);
		std::vector<uint8_t> getHash(const std::vector<uint8_t>& value);

	public:
		// the actually ID is a chain containing the server+channel or server+name
		// eg: hash(hash(ServerName)+ChannelName)
		std::vector<uint8_t> getIDHash(std::string_view name);

		ContactHandle4 getC(std::string_view channel);
		ContactHandle4 getU(std::string_view nick);
		// user or channel using channel prefix
		ContactHandle4 getCU(std::string_view name);

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
