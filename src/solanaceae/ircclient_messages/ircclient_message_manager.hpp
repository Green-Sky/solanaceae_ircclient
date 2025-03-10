#pragma once

#include <solanaceae/ircclient/ircclient.hpp>
#include <solanaceae/ircclient_contacts/ircclient_contact_model.hpp>
#include <solanaceae/message3/registry_message_model.hpp>

class IRCClientMessageManager : public IRCClientEventI, public RegistryMessageModelEventI {
	protected:
		RegistryMessageModelI& _rmm;
		RegistryMessageModelI::SubscriptionReference _rmm_sr;
		ContactStore4I& _cs;
		ConfigModelI& _conf;
		IRCClient1& _ircc;
		IRCClient1::SubscriptionReference _ircc_sr;
		IRCClientContactModel& _ircccm;

	public:
		IRCClientMessageManager(
			RegistryMessageModelI& rmm,
			ContactStore4I& cs,
			ConfigModelI& conf,
			IRCClient1& ircc,
			IRCClientContactModel& ircccm
		);

		virtual ~IRCClientMessageManager(void);

		// bring event overloads into scope
		using IRCClientEventI::onEvent;
		using RegistryMessageModelEventI::onEvent;
	private:
		bool processMessage(ContactHandle4 from, ContactHandle4 to, std::string_view message_text, bool action);

	private: // mm3
		bool sendText(const Contact4 c, std::string_view message, bool action = false) override;

	private: // ircclient
		bool onEvent(const IRCClient::Events::Channel& e) override;
		bool onEvent(const IRCClient::Events::PrivMSG& e) override;
		bool onEvent(const IRCClient::Events::Notice& e) override;
		bool onEvent(const IRCClient::Events::ChannelNotice& e) override;
		bool onEvent(const IRCClient::Events::CTCP_Action& e) override;
};
