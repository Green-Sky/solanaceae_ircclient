#include <solanaceae/plugin/solana_plugin_v1.h>

#include <solanaceae/ircclient/ircclient.hpp>
#include <solanaceae/ircclient_contacts/ircclient_contact_model.hpp>
#include <solanaceae/ircclient_messages/ircclient_message_manager.hpp>

#include <entt/entt.hpp>
#include <entt/fwd.hpp>

#include <memory>
#include <iostream>

static std::unique_ptr<IRCClient1> g_ircc = nullptr;
static std::unique_ptr<IRCClientContactModel> g_ircccm = nullptr;
static std::unique_ptr<IRCClientMessageManager> g_irccmm = nullptr;

constexpr const char* plugin_name = "IRCClient";

extern "C" {

SOLANA_PLUGIN_EXPORT const char* solana_plugin_get_name(void) {
	return plugin_name;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_get_version(void) {
	return SOLANA_PLUGIN_VERSION;
}

SOLANA_PLUGIN_EXPORT uint32_t solana_plugin_start(struct SolanaAPI* solana_api) {
	std::cout << "PLUGIN " << plugin_name << " START()\n";

	if (solana_api == nullptr) {
		return 1;
	}

	try {
		auto* cr = PLUG_RESOLVE_INSTANCE_VERSIONED(Contact3Registry, "1");
		auto* rmm = PLUG_RESOLVE_INSTANCE(RegistryMessageModel);
		auto* conf = PLUG_RESOLVE_INSTANCE(ConfigModelI);

		// static store, could be anywhere tho
		// construct with fetched dependencies
		g_ircc = std::make_unique<IRCClient1>(*conf);
		g_ircccm = std::make_unique<IRCClientContactModel>(*cr, *conf, *g_ircc);
		g_irccmm = std::make_unique<IRCClientMessageManager>(*rmm, *cr, *conf, *g_ircc, *g_ircccm);

		// register types
		PLUG_PROVIDE_INSTANCE(IRCClient1, plugin_name, g_ircc.get());
		PLUG_PROVIDE_INSTANCE(IRCClientContactModel, plugin_name, g_ircccm.get());
		PLUG_PROVIDE_INSTANCE(IRCClientMessageManager, plugin_name, g_irccmm.get());
	} catch (const ResolveException& e) {
		std::cerr << "PLUGIN " << plugin_name << " " << e.what << "\n";
		return 2;
	}

	return 0;
}

SOLANA_PLUGIN_EXPORT void solana_plugin_stop(void) {
	std::cout << "PLUGIN " << plugin_name << " STOP()\n";

	g_ircc.reset();
}

SOLANA_PLUGIN_EXPORT float solana_plugin_tick(float delta) {
	return g_ircc->iterate(delta);
}

} // extern C

