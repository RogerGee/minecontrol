// minecraft-server-properties.h
#ifndef MINECRAFT_SERVER_PROPERTIES_H
#define MINECRAFT_SERVER_PROPERTIES_H
#include "rlibrary/rstream.h"
#include "rlibrary/rdynarray.h"

namespace minecraft_controller
{
    // generic property base type
    template<typename T>
    struct minecraft_server_property
    {
        minecraft_server_property(); // field is null
        minecraft_server_property(const T& defaultValue); // field is default value
        virtual ~minecraft_server_property();

        void put(rtypes::rstream&) const;

        const char* get_key() const
        { return _getKeyName(); }
        const T& get_value() const
        { return value; }

        void set_value(const T&);
    private:
        T _value;
        bool _isNull; // if true, property field becomes: Key=

        virtual void _putValue(rtypes::rstream&) const;
        virtual const char* _getKeyName() const = 0;
    };
    //

    // base property types
    typedef minecraft_server_property<bool> boolean_prop;
    typedef minecraft_server_property<int> numeric_prop; // or otherwise enumerable in some way
    typedef minecraft_server_property<rtypes::str> string_prop;
    //

    // individual property types
    struct mcraft_allow_flight : boolean_prop
    {
        mcraft_allow_flight(): boolean_prop(false) {}

    private:
        virtual const char* _getKeyName() const
        { return "allow-flight"; }
    };

    struct mcraft_allow_nether : boolean_prop
    {
        mcraft_allow_nether(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "allow-nether"; }
    };

    struct mcraft_announce_player_achievements : boolean_prop
    {
        mcraft_announce_player_achievements(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "announce-player-achievements"; }
    };

    struct mcraft_difficulty : numeric_prop
    {
        enum {
            mcraft_difficulty_peaceful,
            mcraft_difficulty_easy,
            mcraft_difficulty_normal,
            mcraft_difficulty_hard
        };

        mcraft_difficulty(): numeric_prop(mcraft_difficulty_easy) {}
    private:
        virtual const char* _getKeyName() const
        { return "difficulty"; }
    };

    struct mcraft_enable_query : boolean_prop
    {
        mcraft_enable_query(): boolean_prop(false) {}

    private:
        virtual const char* _getKeyName() const
        { return "enable-query"; }
    };

    struct mcraft_enable_rcon : boolean_prop
    {
        mcraft_enable_rcon(): boolean_prop(false) {}

    private:
        virtual const char* _getKeyName() const
        { return "enable-rcon"; }
    };

    struct mcraft_enable_command_block : boolean_prop(false) {}
    {
        mcraft_enable_command_block(): boolean_prop(false) {}

    private:
        virtual const char* _getKeyName() const
        { return "enable-command-block"; }
    };

    struct mcraft_force_gamemode : boolean_prop
    {
        mcraft_force_gamemode(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "force-gamemode"; }
    };

    struct mcraft_gamemode : numeric_prop
    {
        enum {
            mcraft_gamemode_survival = 0,
            mcraft_gamemode_creative = 1,
            mcraft_gamemode_adventure = 2
        };

        mcraft_gamemode(): numeric_prop(mcraft_gamemode_survival) {}
    private:
        virtual const char* _getKeyName() const
        { return "gamemode"; }
    };

    struct mcraft_generate_structures : boolean_prop
    {
        mcraft_generate_structures(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "generate-structures"; }
    };

    struct mcraft_generator_settings : string_prop
    {
        // default null
    private:
        virtual const char* _getKeyName() const
        { return "generator_settings"; }
    };

    struct mcraft_hardcore : boolean_prop
    {
        mcraft_hardcore(): boolean_prop(false) {}

    private:
        virtual const char* _getKeyName() const
        { return "hardcore"; }
    };

    struct mcraft_level_name : string_prop
    {
        mcraft_level_name(): string_prop("world") {}

    private:
        virtual const char* _getKeyName() const
        { return "level-name"; }
    };

    struct mcraft_level_seed : string_prop
    {
    private:
        virtual const char* _getKeyName() const
        { return "level-seed"; }
    };

    struct mcraft_level_type : numeric_prop
    {
        enum {
            mcraft_level_default,
            mcraft_level_flat,
            mcraft_level_largebiomes,
            mcraft_level_amplified
        };

        mcraft_level_type(): numeric_prop(mcraft_level_default) {}
    private:
        virtual void _putValue(rtypes::rstream&) const;
        virtual const char* _getKeyName() const
        { return "level-type"; }
    };

    struct mcraft_max_build_height : numeric_prop
    {
        mcraft_max_build_height(): numeric_prop(256) {}

    private:
        virtual const char* _getKeyName() const
        { return "max-build-height"; }
    };

    struct mcraft_max_players : numeric_prop
    {
        mcraft_max_players(): numeric_prop(20) {}

    private:
        virtual const char* _getKeyName() const
        { return "max-players"; }
    };

    struct mcraft_motd : string_prop
    {
        mcraft_motd(): string_prop("A Minecraft Server") {}

    private:
        virtual const char* _getKeyName() const
        { return "motd"; }
    };

    struct mcraft_online_mode : boolean_prop
    {
        mcraft_online_mode(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "online-mode"; }
    };

    struct mcraft_op_permission_level : numeric_prop
    {
        enum {
            mcraft_op_level1 = 1,
            mcraft_op_level2 = 2,
            mcraft_op_level3 = 3,
            mcraft_op_level4 = 4
        };

        mcraft_op_permission_level(): numeric_prop(mcraft_op_level3) {}
    private:
        virtual const char* _getKeyName() const
        { return "op-permission-level"; }
    };

    struct mcraft_player_idle_timeout : numeric_prop
    {
        mcraft_player_idle_timeout(): numeric_prop(20) {}

    private:
        virtual const char* _getKeyName() const
        { return "player-idle-timeout"; }
    };

    struct mcraft_pvp : boolean_prop
    {
        mcraft_pvp(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "pvp"; }
    };

    struct mcraft_resource_pack : string_prop
    {
    private:
        virtual const char* _getKeyName() const
        { return "resource-pack"; }
    };

    struct mcraft_server_ip : string_prop
    {
    private:
        virtual const char* _getKeyName() const
        { return "server-ip"; }
    };

    struct mcraft_server_name : string_prop
    {
    private:
        virtual const char* _getKeyName() const
        { return "server-name"; }
    };

    struct mcraft_server_port : numeric_prop
    {
        mcraft_server_port(): numeric_prop(25565) {}

    private:
        virtual const char* _getKeyName() const
        { return "server-port"; }
    };

    struct mcraft_snooper_enabled : boolean_prop
    {
        mcraft_snooper_enabled(): boolean_prop(false) {}

    private:
        virtual const char* _getKeyName() const
        { return "snooper-enabled"; }
    };

    struct mcraft_spawn_animals : boolean_prop
    {
        mcraft_spawn_animals(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "spawn-animals"; }
    };

    struct mcraft_spawn_monsters : boolean_prop
    {
        mcraft_spawn_monsters(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "spawn-monsters"; }
    };

    struct mcraft_spawn_npcs : boolean_prop
    {
        mcraft_spawn_npcs(): boolean_prop(true) {}

    private:
        virtual const char* _getKeyName() const
        { return "spawn-npcs"; }
    };

    struct mcraft_spawn_protection : numeric_prop
    {
        mcraft_spawn_protection(): numeric_prop(16) {}

    private:
        virtual const char* _getKeyName() const
        { return "spawn-protection"; }
    };

    struct mcraft_view_distance : numeric_prop
    {
        mcraft_view_distance(): numeric_prop(10) {}

    private:
        virtual const char* _getKeyName() const
        { return "view-distance"; }
    };

    struct mcraft_white_list : boolean_prop
    {
        mcraft_white_list(): boolean_prop(false) {}

    private:
        virtual const char* _getKeyName() const
        { return "white-list"; }
    };
    //

    // property list type
    template<typename T>
    class minecraft_property_list
    {
        minecraft_property_list();
        // perform template specializations for
        // each of the property types: int, rtypes::str, and bool

        ~minecraft_property_list();

        minecraft_server_property<T>* operator [](const char*);
        const minecraft_server_property<T>* operator [](const char*) const;
    private:
        rtypes::dynamic_array< minecraft_server_property<T> > _list;
    };
    //
}

// minecraft_server_property out-of-line implementation
template<typename T>
minecraft_controller::minecraft_server_property<T>::minecraft_server_property()
{
    _isNull = true;
}
template<typename T>
minecraft_controller::minecraft_server_property<T>::minecraft_server_property(const T& defaultValue)
    : _value(defaultValue)
{
    _isNull = false;
}
template<typename T>
minecraft_controller::minecraft_server_property<T>::~minecraft_server_property()
{
}
template<typename T>
void minecraft_controller::minecraft_server_property<T>::put(rtypes::rstream& stream) const
{
    stream << _getKeyName() << '=';
    if (!_isNull)
        _putValue(stream);
}
template<typename T>
void minecraft_controller::minecraft_server_property<T>::set_value(const T& value)
{
    // since the user intentionally is setting the value,
    // then it shouldn't be left null
    _isNull = false;
    _value = value;
}
template<typename T>
void minecraft_controller::minecraft_server_property<T>::_putValue(rtypes::rstream& stream) const
{
    stream << value;
}

#endif
