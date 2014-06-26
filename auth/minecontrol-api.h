/* minecontrol-api.h - minecontrol authority program library core API */
#ifndef MINECONTROL_API_H
#define MINECONTROL_API_H

/* size constants */
#define MAX_BUFFER 1024
#define MAX_MESSAGE_TOKENS 512
#define MAX_COMMAND_TOKENS 64
#define MAX_PATTERN 24

/*************************************************************
 * CONSTANTS *************************************************
 *************************************************************
 *************************************************************
 */

/* token constants */
typedef enum {
    t_invalid, /* used by implementation */
    t_string = 's',
    t_integer = 'i',
    t_float = 'f',
    t_message = 'm',
    t_command = 'c',
    t_block = 'b',
    t_datavalue = 'd',
    t_blockhandling = 'h'

} token_kind;

/* message constants */
typedef enum {
    m_invalid = -1, /* used by implementation */
    m_eoi = -2, /* used by implementation */
    m_chat = 0,
    m_serverchat,
    m_serverstart,
    m_serverbind,
    m_servershutdown,
    m_playerlogin,
    m_playerid,
    m_playerjoin,
    m_playerlogout,
    m_playerdropped,
    m_playerleave,
    m_playerachieve,
    m_unknown
} message_kind;
#define MESSAGE_PLAYER_CHAT "chat"
#define MESSAGE_SERVER_CHAT "server-chat"
#define MESSAGE_SERVER_START "start"
#define MESSAGE_SERVER_BIND "bind"
#define MESSAGE_SERVER_SHUTDOWN "shutdown"
#define MESSAGE_PLAYER_LOGIN "login"
#define MESSAGE_PLAYER_ID "player-id"
#define MESSAGE_PLAYER_JOIN "join"
#define MESSAGE_PLAYER_LOGOUT_CONNECTION "logout-connection"
#define MESSAGE_PLAYER_LOST_CONNECTION "lost-connection"
#define MESSAGE_PLAYER_LEAVE "leave"
#define MESSAGE_PLAYER_ACHIEVEMENT "achievement"
#define MESSAGE_UNKNOWN "type-unknown"
extern const char* const MESSAGE_KINDS[];

/* command format constants */
typedef enum {
    c_invalid = -1, /* used by implementation */
    c_say,
    c_setblock,
    c_summon,
    c_teleport
} command_kind;
#define COMMAND_GENERIC "%c"
#define COMMAND_SAY "say %s"
#define COMMAND_SETBLOCK "setblock %i %i %i %b [%p] [%h]"
#define COMMAND_SUMMON "summon %s %i %i %i"
#define COMMAND_TELEPORT "tp %s %f %f %f"
extern const char* const COMMAND_FORMATS[];

/* block_kind - define block types */
typedef enum {
    b_air, b_stone, b_grass, b_dirt, b_cobblestone,
    b_planks, b_sapling, b_bedrock, b_flowing_water,
    b_water, b_flowing_lava, b_lava, b_sand, b_gravel,
    b_gold_ore, b_iron_ore, b_coal_ore, b_log, b_leaves,
    b_sponge, b_glass, b_lapis_ore, b_dispenser, b_sandstone,
    b_noteblock, b_bed, b_golden_rail, b_detector_rail, b_sticky_piston,
    b_web, b_tallgrass, b_deadbush, b_piston, b_piston_head, b_wool,
    b_piston_extension, b_yellow_flower, b_red_flower, b_brown_mushroom,
    b_red_mushroom, b_gold_block, b_iron_block, b_double_stone_slab,
    b_stone_slab, b_brick_block, b_tnt, b_bookshelf, b_mossy_cobblestone,
    b_obsidian, b_torch, b_fire, b_mob_spawner, b_oak_stairs, b_chest,
    b_redstone_wire, b_diamond_ore, b_diamond_block, b_crafting_table, b_wheat,
    b_farmland, b_furnace, b_lit_furnace, b_standing_sign, b_wooden_door,
    b_ladder, b_rail, b_stone_stairs, b_wall_sign, b_lever, b_stone_pressure_plate,
    b_iron_door, b_wooden_pressure_plate, b_redstone_ore, b_lit_redstone_ore,
    b_unlit_redstone_torch, b_redstone_torch, b_stone_button, b_snow_layer,
    b_ice, b_snow, b_cactus, b_clay, b_reeds, b_jukebox, b_fence,
    b_pumpkin, b_netherrack, b_soul_sand, b_glowstone, b_portal, b_lit_pumpkin,
    b_cake, b_unpowered_repeater, b_powered_repeater, b_stained_glass, b_trapdoor,
    b_monster_egg, b_stonebrick, b_brown_mushroom_block, b_red_mushroom_block, 
    b_iron_bars, b_glass_pane, b_melon_block, b_pumpkin_stem, b_melon_stem,
    b_vine, b_fence_gate, b_brick_stairs, b_stone_brick_stairs, b_mycelium,
    b_waterlily, b_nether_brick, b_nether_brick_fence, b_nether_brick_stairs,
    b_nether_wart, b_enchanting_table, b_brewing_stand, b_cauldron, b_end_portal,
    b_end_portal_frame, b_end_stone, b_dragon_egg, b_redstone_lamp, b_lit_redstone_lamp,
    b_double_wooden_slab, b_wooden_slab, b_cocoa, b_sandstone_stairs, b_emerald_ore,
    b_ender_chest, b_tripwire_hook, b_tripwire, b_emerald_block, b_spruce_stairs,
    b_birch_stairs, b_jungle_stairs, b_command_block, b_beacon, b_cobblestone_wall,
    b_flower_pot, b_carrots, b_potatoes, b_wooden_button, b_skull, b_anvil,
    b_trapped_chest, b_light_weighted_pressure_plate, b_heavy_weighted_pressure_plate, 
    b_unpowered_comparator, b_powered_comparator, b_daylight_detector, b_redstone_block,
    b_quartz_ore, b_hopper, b_quartz_block, b_quartz_stairs, b_activator_rail, b_dropper,
    b_stained_hardened_clay, b_stained_glass_pane, b_leaves2, b_log2, b_acacia_stairs,
    b_dark_oak_stairs, b_slime, b_barrier, b_iron_trapdoor, b_prismarine, b_sea_lantern,
    b_hay_block, b_carpet, b_hardened_clay, b_coal_block, b_packed_ice, b_double_plant
} block_kind;
extern const char* const BLOCK_IDS[];

/* data_value - block data values */
typedef enum {
    /* wood planks */
    d_oak_wood_planks = 0, d_spruce_wood_planks, d_birch_wood_planks, d_jungle_wood_planks,
    d_acacia_wood_planks, d_dark_oak_wood_planks,

    /* stone */
    d_stone = 0, d_granite, d_polished_granite, d_diorite, d_polished_diorite, d_andesite,
    d_polished_andesite,

    /* dirt */
    d_dirt = 0, d_coarse_dirt, d_podzol,

    /* saplings */
    d_oak_sapling = 0, d_spruce_sapling, d_birch_sapling, d_jungle_sapling, d_acacia_sapling,
    d_dark_oak_sapling,

    /* sand */
    d_sand = 0, d_red_sand,

    /* wood (b_log) */
    d_oak_updown = 0, d_spruce_updown, d_birch_updown, d_jungle_updown, d_oak_eastwest, d_spruce_eastwest,
    d_birch_eastwest, d_jungle_eastwest, d_oak_northsouth, d_spruce_northsouth, d_birch_northsouth,
    d_jungle_northsouth, d_oak_onlybark, d_spruce_onlybark, d_birch_onlybark, d_jungle_onlybark,

    /* wood (b_log2) */
    d_acacia_updown = 0, d_darkoak_updown = 1, d_acacia_eastwest = 4, d_darkoak_eastwest = 5,
    d_acacia_northsouth = 8, d_darkoak_northsouth = 9, d_acacia_onlybark = 12, d_darkoak_onlybark = 13,

    /* leaves (b_leaves) */
    d_oak_leaves = 0, d_spruce_leaves, d_birch_leaves, d_jungle_leaves, d_oak_leaves_nodecay,
    d_spruce_leaves_nodecay, d_birch_leaves_nodecay, d_jungle_leaves_nodecay, d_oak_leaves_check,
    d_spruce_leaves_check, d_birch_leaves_check, d_jungle_leaves_check, d_oak_leaves_nodecaycheck,
    d_spruce_leaves_nodecaycheck, d_birch_leaves_nodecaycheck, d_jungle_leaves_nodecaycheck,

    /* leaves (b_leaves2) */
    d_acacia_leaves = 0, d_darkoak_leaves = 1, d_acacia_leaves_nodecay = 4, d_darkoak_leaves_nodecay = 5,
    d_acacia_leaves_check = 8, d_darkoak_leaves_check = 9, d_acacia_leaves_nodecaycheck = 12,
    d_darkoak_leaves_nodecaycheck = 13,

    /* colored (b_wool, b_stained_glassb_stained_hardened_clay, b_stained_glass_pane, and b_carpet */
    d_regular_white = 0, d_orange, d_magenta, d_light_blue, d_yellow, d_lime, d_pink, d_gray,
    d_light_gray, d_cyan, d_purple, d_blue, d_brown, d_green, d_red, d_black,

    /* torch position (b_torch, b_unlit_redstone_torch, and b_redstone_torch) */
    d_torch_east = 1, d_torch_west, d_torch_south, d_torch_north, d_torch_floor,

    /* stone slabs (b_stone_slab, b_double_stone_slab) */
    d_stone_slab = 0, d_sandstone_slab, d_wooden_slab, d_cobblestone_slab, d_brick_slab, d_stone_brick_slab,
    d_nether_brick_slab, d_quartz_slab, d_smooth_stone_slab, d_smooth_sandstone_slab, d_tile_quartz_slab,

    /* wooden slabs (b_wooden_slab, b_double_wooden_slab) */
    d_oak_slab = 0, d_spruce_slab, d_birch_slab, d_jungle_slab, d_acacia_slab, d_darkoak_slab,

    /* sandstone (b_sandstone) */
    d_sandstone = 0, d_chiseled_sandstone, d_smooth_sandstone,

    /* bed position (b_bed) */
    d_bed_south = 0, d_bed_west, d_bed_north, d_bed_east,

    /* grass (b_grass) */
    d_shrub = 0, d_grass, d_fern, d_grass2,

    /* flowers (b_red_flower) */
    d_poppy = 0, d_blue_orchid, d_allium, d_azure_bluet, d_red_tulip, d_orange_tulip, d_white_tulip,
    d_pink_tulip, d_oxeye_daisy,

    /* flowers2 (b_double_plant) */
    d_sunflower = 0, d_lilac, d_double_tallgrass, d_large_fern, d_rose_bush, d_peony,

    /* stairs */
    d_stairs_east = 0, d_stairs_west, d_stairs_south, d_stairs_north,

    /* doors */
    d_door_west = 0, d_door_north, d_door_east, d_door_south
} data_value;

/* block_handling - flags for setblock command */
typedef enum {
    h_keep,
    h_replace,
    h_destroy
} block_handling;

/*************************************************************
 * STRUCTURES ************************************************
 *************************************************************
 *************************************************************
 */

/* token - stores a token */
typedef union {
    int tok_flag;
    float tok_float;
    const char* tok_str;
} token;
void token_init(token* tok);
void token_assign_flag(token* tok,int flag);
void token_assign_int(token* tok,int i);
void token_assign_float(token* toke,float f);
void token_assign_str(token* tok,const char* str);
const char* token_to_str(token* tok,int kind); /* interpret the token as a string based on token_kind  (not thread-safe) */
const char* token_to_str_tsafe(token* tok,int kind,char* buffer/*MAX_BUFFER+1 length*/); /* same as above but may use provided buffer for thread safety */

/* message - store message from minecontrol server */
typedef struct {
    int ta_top; /* number of tokens in 'msg_tokens' */
    int tb_top;
    int msg_kind; /* message_kind */
    char msg_buffer[MAX_BUFFER+1]; /* complete message (minus kind) */
    char* msg_tokens[MAX_MESSAGE_TOKENS+1]; /* whitespace separated tokens terminated by NULL ptr */
    char msg_tokbuffer[MAX_BUFFER+1];
} message;
void message_init(message* msg);
int message_assign(message* msg,const char* source); /* parse source message */

/* command - store command to send to Minecraft server */
typedef struct {
    int com_kind; /* command_kind */
    token com_tokens[MAX_COMMAND_TOKENS+1];
    int com_toktop;
} command;
void command_init(command* com);
void command_assign(command* com,int kind);
void command_assign_ex(command* com,int kind,const char* source);
void command_addtoken(command* com,const char* token);
void command_addtoken_byflag(command* com,int flag);
int command_replacetoken(command* com,int position,const char* token);
int command_replacetoken_byflag(command* com,int position,int flag);
int command_compile(command* com,char* buffer,int size);

/* coord - define location */
typedef struct {
    int coord_x;
    int coord_y;
    int coord_z;
} coord;

/* rectangle - defines a 2D rectangle */
typedef struct {
    int r_width;
    int r_height;
} rectangle;

/* box - defines a 3D rectangular prism */
typedef struct {
    rectangle bx_recth;
    rectangle bx_rectv;
} box;

/* pattern - define an array (grid) of block kinds */
typedef struct {
    int pat_data[MAX_PATTERN][MAX_PATTERN];
    int pat_flags;
} pattern;



/* general operation functions */
int get_message(char* buffer,int size); /* return message_kind; place rest-of-line in buffer */
int get_message_ex(message* msg);
void issue_command(int kind, ...);
void issue_command_ex(const command* com);

/* apply functions */
void apply_block(int kind,int x,int y,int z);
void apply_block_ex(int kind,const coord* loc);
void apply_rectangle(int kind,int x,int y,int z,int width,int height);
void apply_rectangle_ex(int kind,const coord* loc,const rectangle* rect);
void apply_rectangle_pat(const pattern* pat,const coord* loc,const rectangle* rect);
void apply_box(int kind,int x,int y,int z,int widthv,int heightv,int widthh,int heighth);
void apply_box_ex(int kind,const coord* loc,const box* bx);
void apply_box_pat(const pattern* pat,const coord* loc,const box* bx);

#endif
