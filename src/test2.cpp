#include <solanaceae/util/simple_config_model.hpp>
#include <solanaceae/contact/contact_model3.hpp>
#include <solanaceae/message3/registry_message_model.hpp>
#include <solanaceae/ircclient/ircclient.hpp>
#include <solanaceae/ircclient_contacts/ircclient_contact_model.hpp>
#include <solanaceae/ircclient_messages/ircclient_message_manager.hpp>

#include <libircclient.h>

#include <iostream>
#include <string_view>

int main(void) {
	SimpleConfigModel conf;

	conf.set("IRCClient", "server", std::string_view{"#irc.rizon.net"});
	conf.set("IRCClient", "port", int64_t(6697));
	conf.set("IRCClient", "autojoin", "#HorribleSubs", true);
	conf.set("IRCClient", "autojoin", "#green_testing", true);

	Contact3Registry cr;
	RegistryMessageModel rmm{cr};

	IRCClient1 ircc{conf};

	IRCClientContactModel ircccm{cr, conf, ircc};
	IRCClientMessageManager irccmm{rmm, cr, conf, ircc, ircccm};

	//ircccm.join("#green_testing");

	while (irc_is_connected(ircc.getSession())) {
		ircc.iterate();
	}

	return 0;
}
