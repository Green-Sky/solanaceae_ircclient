#pragma once

#include "./components.hpp"

#include <entt/core/type_info.hpp>

// TODO: move more central
#define DEFINE_COMP_ID(x) \
template<> \
constexpr entt::id_type entt::type_hash<x>::value() noexcept { \
    using namespace entt::literals; \
    return #x##_hs; \
}

// cross compiler stable ids

DEFINE_COMP_ID(Contact::Components::IRC::ServerName)
DEFINE_COMP_ID(Contact::Components::IRC::ChannelName)
DEFINE_COMP_ID(Contact::Components::IRC::UserName)

#undef DEFINE_COMP_ID

