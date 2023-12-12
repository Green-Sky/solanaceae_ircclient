cmake_minimum_required(VERSION 3.14...3.24 FATAL_ERROR)

add_library(plugin_ircclient SHARED
	./plugin_ircclient.cpp
)

target_link_libraries(plugin_ircclient PUBLIC
	solanaceae_plugin
	solanaceae_ircclient_contacts
	solanaceae_ircclient_messages
)

