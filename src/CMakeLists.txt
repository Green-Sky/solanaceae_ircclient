cmake_minimum_required(VERSION 3.9...3.24 FATAL_ERROR)

project(solanaceae)

add_library(solanaceae_ircclient
	./solanaceae/ircclient/ircclient.hpp
	./solanaceae/ircclient/ircclient.cpp
)

target_include_directories(solanaceae_ircclient PUBLIC .)
target_compile_features(solanaceae_ircclient PRIVATE cxx_std_20)
target_compile_features(solanaceae_ircclient INTERFACE cxx_std_17)
target_link_libraries(solanaceae_ircclient PUBLIC
	solanaceae_util
	libircclient
	libsodium
)

########################################

add_library(solanaceae_ircclient_contacts
	./solanaceae/ircclient_contacts/components.hpp
	./solanaceae/ircclient_contacts/components_id.inl

	./solanaceae/ircclient_contacts/ircclient_contact_model.hpp
	./solanaceae/ircclient_contacts/ircclient_contact_model.cpp
)

target_include_directories(solanaceae_ircclient_contacts PUBLIC .)
target_compile_features(solanaceae_ircclient_contacts PRIVATE cxx_std_20)
target_compile_features(solanaceae_ircclient_contacts INTERFACE cxx_std_17)
target_link_libraries(solanaceae_ircclient_contacts PUBLIC
	solanaceae_util
	solanaceae_contact
	solanaceae_ircclient
)

########################################

add_library(solanaceae_ircclient_messages
	./solanaceae/ircclient_messages/ircclient_message_manager.hpp
	./solanaceae/ircclient_messages/ircclient_message_manager.cpp
)

target_include_directories(solanaceae_ircclient_messages PUBLIC .)
target_compile_features(solanaceae_ircclient_messages PRIVATE cxx_std_20)
target_compile_features(solanaceae_ircclient_messages INTERFACE cxx_std_17)
target_link_libraries(solanaceae_ircclient_messages PUBLIC
	solanaceae_ircclient_contacts
	solanaceae_message3
)

########################################

add_executable(test2
	test2.cpp
)

target_link_libraries(test2 PUBLIC
	solanaceae_ircclient
	solanaceae_ircclient_contacts
	solanaceae_ircclient_messages
)
