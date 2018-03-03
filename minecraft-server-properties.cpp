// minecraft-server-properties.cpp
#include "minecraft-server-properties.h"
#include <rlibrary/rutility.h>
using namespace rtypes;
using namespace minecraft_controller;

// minecraft_controller::minecraft_server_input_property

minecraft_server_input_property::minecraft_server_input_property()
{
}
minecraft_server_input_property::minecraft_server_input_property(const char* pkey,const char* pvalue)
    : key(pkey), value(pvalue)
{
}

// minecraft_controller::minecraft_server_property derivations

bool mcraft_level_type::_readValue(rstream& stream)
{
    str id;
    stream >> id;
    rutil_to_lower_ref(id);
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

bool mcraft_motd::_readValue(rstream& stream)
{
    stream.getline(_value);
    return _value.length() > 0;
}

// minecraft_controller::minecraft_server_generic_property_list derivations
minecraft_boolean_property_list::minecraft_boolean_property_list()
{
    _pushBack( new mcraft_allow_flight );
    _pushBack( new mcraft_allow_nether );
    _pushBack( new mcraft_announce_player_achievements );
    _pushBack( new mcraft_broadcast_console_to_ops );
    _pushBack( new mcraft_enable_query );
    _pushBack( new mcraft_enable_rcon );
    _pushBack( new mcraft_enable_command_block );
    _pushBack( new mcraft_force_gamemode );
    _pushBack( new mcraft_generate_structures );
    _pushBack( new mcraft_hardcore );
    _pushBack( new mcraft_online_mode );
    _pushBack( new mcraft_prevent_proxy_connections );
    _pushBack( new mcraft_pvp );
    _pushBack( new mcraft_snooper_enabled );
    _pushBack( new mcraft_spawn_animals );
    _pushBack( new mcraft_spawn_monsters );
    _pushBack( new mcraft_spawn_npcs );
    _pushBack( new mcraft_use_native_transport );
    _pushBack( new mcraft_white_list );
}
minecraft_numeric_property_list::minecraft_numeric_property_list()
{
    _pushBack( new mcraft_difficulty );
    _pushBack( new mcraft_gamemode );
    _pushBack( new mcraft_level_type );
    _pushBack( new mcraft_max_build_height );
    _pushBack( new mcraft_max_players );
    _pushBack( new mcraft_max_tick_time );
    _pushBack( new mcraft_max_world_size );
    _pushBack( new mcraft_network_compression_threshold );
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
    _pushBack( new mcraft_resource_pack_sha1 );
    _pushBack( new mcraft_server_ip );
    _pushBack( new mcraft_server_name );
}

// minecraft_controller::minecraft_server_property_list
void minecraft_server_property_list::read(rstream& input)
{
    stringstream lineInput;
    lineInput.delimit_whitespace(false);
    lineInput.add_extra_delimiter("=\n");
    while ( input.has_input() ) {
        str key, value;
        lineInput.clear();
        input.getline( lineInput.get_device() );
        lineInput >> key >> value;
        if (key.length() > 0) {
            // check each type of property; if not found then strip
            // from the properties list
            minecraft_server_property<bool>* bprop;
            minecraft_server_property<int>* iprop;
            minecraft_server_property<str>* sprop;
            if ((bprop = booleanProps.lookup(key.c_str())) != NULL)
                bprop->set_value(value);
            else if ((iprop = numericProps.lookup(key.c_str())) != NULL)
                iprop->set_value(value);
            else if ((sprop = stringProps.lookup(key.c_str())) != NULL)
                sprop->set_value(value);
        }
    }
}
void minecraft_server_property_list::write(rstream& output) const
{
    for (size_type i = 0;i<booleanProps.size();i++)
        booleanProps[i]->put(output), output << newline;
    for (size_type i = 0;i<numericProps.size();i++)
        numericProps[i]->put(output), output << newline;
    for (size_type i = 0;i<stringProps.size();i++)
        stringProps[i]->put(output), output << newline;
}
