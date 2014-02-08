// minecraft-server-properties.cpp
#include "minecraft-server-properties.h"
using namespace rtypes;
using namespace minecraft_controller;

// minecraft_controller::minecraft_server_property derivations

bool mcraft_level_type::_readValue(rstream& stream)
{
    str id;
    stream >> id;
    for (size_type i = 0;i<id.length();i++)
        if (id[i]>='A' && id[i]<='Z')
            id[i] -= 'A', id[i] += 'a';
    if (id == "flat")
        _value = mcraft_level_flat;
    else if (id == "largebiomes")
        _value = mcraft_level_largebiomes;
    else if (id == "amplified")
        _value = mcraft_level_amplified;
    else if (id == "default")
        _value = mcraft_level_default;
    else
        return false;
    return true;
}
void mcraft_level_type::_putValue(rstream& stream) const
{
    switch ( get_value() )
    {
    case mcraft_level_flat:
        stream << "FLAT";
        break;
    case mcraft_level_largebiomes:
        stream << "LARGEBIOMES";
        break;
    case mcraft_level_amplified:
        stream << "AMPLIFIED";
        break;
    default:
        stream << "DEFAULT";
        break;
    }
}

// minecraft_controller::minecraft_server_generic_property_list derivations
minecraft_boolean_property_list::minecraft_boolean_property_list()
{
    _pushBack( new mcraft_allow_flight );
    _pushBack( new mcraft_allow_nether );
    _pushBack( new mcraft_announce_player_achievements );
    _pushBack( new mcraft_enable_query );
    _pushBack( new mcraft_enable_rcon );
    _pushBack( new mcraft_enable_command_block );
    _pushBack( new mcraft_force_gamemode );
    _pushBack( new mcraft_generate_structures );
    _pushBack( new mcraft_hardcore );
    _pushBack( new mcraft_online_mode );
    _pushBack( new mcraft_pvp );
    _pushBack( new mcraft_snooper_enabled );
    _pushBack( new mcraft_spawn_animals );
    _pushBack( new mcraft_spawn_monsters );
    _pushBack( new mcraft_spawn_npcs );
    _pushBack( new mcraft_white_list );
}
minecraft_numeric_property_list::minecraft_numeric_property_list()
{
    _pushBack( new mcraft_difficulty );
    _pushBack( new mcraft_gamemode );
    _pushBack( new mcraft_level_type );
    _pushBack( new mcraft_max_build_height );
    _pushBack( new mcraft_max_players );
    _pushBack( new mcraft_op_permission_level );
    _pushBack( new mcraft_player_idle_timeout );
    _pushBack( new mcraft_server_port );
    _pushBack( new mcraft_spawn_protection );
    _pushBack( new mcraft_view_distance );
}
minecraft_string_property_list::minecraft_string_property_list()
{
    _pushBack( new mcraft_generator_settings );
    _pushBack( new mcraft_level_seed );
    _pushBack( new mcraft_motd );
    _pushBack( new mcraft_resource_pack );
    _pushBack( new mcraft_server_ip );
    _pushBack( new mcraft_server_name );
}
