#include <solanaceae/plugin/solana_plugin_v1.h>

#include <solanaceae/ircclient/ircclient.hpp>
#include <solanaceae/ircclient_contacts/ircclient_contact_model.hpp>
#include <solanaceae/ircclient_messages/ircclient_message_manager.hpp>

#include <memory>
#include <iostream>

#define RESOLVE_INSTANCE(x) static_cast<x*>(solana_api->resolveInstance(#x))
#define PROVIDE_INSTANCE(x, p, v) solana_api->provideInstance(#x, p, static_cast<x*>(v))

static std::unique_ptr<IRCClient1> g_ircc = nullptr;
static std::unique_ptr<IRCClientContactModel> g_ircccm = nullptr;
static std::unique_ptr<IRCClientMessageManager> g_irccmm = nullptr;

extern "C" {

SOLANA_PLUGIN_EXPORT const char* solana_plugin_get_name(void) {
	return "IRCClient";
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_get_version(void) {
	return SOLANA_PLUGIN_VERSION;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_start(struct SolanaAPI* solana_api) {
	std::cout << "PLUGIN IRCC START()\n";

	if (solana_api == nullptr) {
		return 1;
	}

	Contact3Registry* cr;
	RegistryMessageModel* rmm = nullptr;
	ConfigModelI* conf = nullptr;

	{ // make sure required types are loaded
		cr = RESOLVE_INSTANCE(Contact3Registry);
		rmm = RESOLVE_INSTANCE(RegistryMessageModel);
		conf = RESOLVE_INSTANCE(ConfigModelI);

		if (cr == nullptr) {
			std::cerr << "PLUGIN IRCC missing Contact3Registry\n";
			return 2;
		}

		if (rmm == nullptr) {
			std::cerr << "PLUGIN IRCC missing RegistryMessageModel\n";
			return 2;
		}

		if (conf == nullptr) {
			std::cerr << "PLUGIN IRCC missing ConfigModelI\n";
			return 2;
		}
	}

	// static store, could be anywhere tho
	// construct with fetched dependencies
	g_ircc = std::make_unique<IRCClient1>(*conf);

	// register types
	PROVIDE_INSTANCE(IRCClient1, "IRCClient", g_ircc.get());

	g_ircccm = std::make_unique<IRCClientContactModel>(*cr, *conf, *g_ircc);

	// register types
	PROVIDE_INSTANCE(IRCClientContactModel, "IRCClient", g_ircccm.get());

	g_irccmm = std::make_unique<IRCClientMessageManager>(*rmm, *cr, *conf, *g_ircc, *g_ircccm);

	// register types
	PROVIDE_INSTANCE(IRCClientMessageManager, "IRCClient", g_irccmm.get());

	return 0;
}

SOLANA_PLUGIN_EXPORT void solana_plugin_stop(void) {
	std::cout << "PLUGIN IRCC STOP()\n";

	g_ircc.reset();
}

SOLANA_PLUGIN_EXPORT float solana_plugin_tick(float delta) {
	(void)delta;
	//std::cout << "PLUGIN IRCC TICK()\n";
	g_ircc->iterate(); // TODO: return interval, respect dcc etc

	return 1.f; // expect atleast once per sec
}

} // extern C

