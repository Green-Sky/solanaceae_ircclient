cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

add_library(plugin_ircclient MODULE
	./plugin_ircclient.cpp
)

set_target_properties(plugin_ircclient PROPERTIES
	C_VISIBILITY_PRESET hidden
)
# probably not enough
target_compile_definitions(plugin_ircclient PUBLIC ENTT_API_IMPORT)

target_link_libraries(plugin_ircclient PUBLIC
	solanaceae_plugin
	solanaceae_ircclient_contacts
	solanaceae_ircclient_messages
)

