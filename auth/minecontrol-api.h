/* minecontrol-api.h - minecontrol authority program library core API */
#ifndef MINECONTROL_API_H
#define MINECONTROL_API_H

/* size constants */
#define MAX_BUFFER 1024
#define MAX_TOKENS 512
#define MAX_PATTERN 24

/* message constants */
#define MESSAGE_CHAT "chat"
#define MESSAGE_SERVER_CHAT "server-chat"

/* command format constants */
#define COMMAND_SAY "say %s"
#define COMMAND_SETBLOCK "setblock %d %d %d %d"
extern const char* const COMMAND_FORMATS[10];

/* message types */
typedef enum {
    m_eoi,
    m_chat,
    m_serverchat
} message_kind;
typedef struct {
    int kind; /* message_kind */
    char buffer[MAX_BUFFER+1]; /* stores rest-of-line */
    char tokbuffer[MAX_BUFFER+1]; /* stores token data pointed to by elements of 'tokens' */
    char* tokens[MAX_TOKENS+1]; /* stores individual token strings; terminated by NULL ptr */
} message;

/* command types */
typedef enum {
    cmd_say,
    cmd_setblock
} command_kind;
typedef struct {
    int kind; /* command_kind */
    char buffer[MAX_BUFFER+1]; /* stores command line */
    char tokbuffer[MAX_BUFFER+1]; /* stores token data pointed to by elements of 'tokens' */
    char* tokens[MAX_TOKENS+1]; /* stores individual token strings; terminated by NULL ptr */
} command;

/* block_kind - define block types */
typedef enum {
    block_air, block_stone, block_grass, block_dirt, block_cobblestone,
    block_planks, block_sapling, block_bedrock, block_flowing_water,
    block_water, block_flowing_lava, block_lava, block_sand, block_gravel,
    block_gold_ore, block_iron_ore, block_coal_ore, block_log, block_leaves,
    block_sponge, block_glass, block_lapis_ore, block_dispenser, block_sandstone,
    block_noteblock, block_bed, block_golden_rail, block_detector_rail, block_sticky_piston,
    block_web, block_tallgrass, block_deadbush, block_piston, block_piston_head, block_wool,
    block_piston_extension, block_yellow_flower, block_red_flower, block_brown_mushroom,
    block_red_mushroom, block_gold_block, block_iron_block, block_double_stone_slab,
    block_stone_slab, block_brick_block, block_tnt, block_bookshelf, block_mossy_cobblestone,
    block_obsidian, block_torch, block_fire, block_mob_spawner, block_oak_stairs, block_chest,
    block_redstone_wire, block_diamond_ore, block_diamond_block, block_crafting_table, block_wheat,
    block_farmland, block_furnace, block_lit_furnace, block_standing_sign, block_wooden_door,
    block_ladder, block_rail, block_stone_stairs, block_wall_sign, block_lever, block_stone_pressure_plate,
    block_iron_door, block_wooden_pressure_plate, block_redstone_ore, block_lit_redstone_ore,
    block_unlit_redstone_torch, block_redstone_torch, block_stone_button, block_snow_layer,
    block_ice, block_snow, block_cactus, block_clay, block_reeds, block_jukebox, block_fence,
    block_pumpkin, block_netherrack, block_soul_sand, block_glowstone, block_portal, block_lit_pumpkin,
    block_cake, block_unpowered_repeater, block_powered_repeater, block_stained_glass, block_trapdoor,
    block_monster_egg, block_stonebrick, block_brown_mushroom_block, block_red_mushroom_block, 
    block_iron_bars, block_glass_pane, block_melon_block, block_pumpkin_stem, block_melon_stem,
    block_vine, block_fence_gate, block_brick_stairs, block_stone_brick_stairs, block_mycelium,
    block_waterlily, block_nether_brick, block_nether_brick_fence, block_nether_brick_stairs,
    block_nether_wart, block_enchanting_table, block_brewing_stand, block_cauldron, block_end_portal,
    block_end_portal_frame, block_end_stone, block_dragon_egg, block_redstone_lamp, block_lit_redstone_lamp,
    block_double_wooden_slab, block_wooden_slab, block_cocoa, block_sandstone_stairs, block_emerald_ore,
    block_ender_chest, block_tripwire_hook, block_tripwire, block_emerald_block, block_spruce_stairs,
    block_birch_stairs, block_jungle_stairs, block_command_block, block_beacon, block_cobblestone_wall,
    block_flower_pot, block_carrots, block_potatoes, block_wooden_button, block_skull, block_anvil,
    block_trapped_chest, block_light_weighted_pressure_plate, block_heavy_weighted_pressure_plate, 
    block_unpowered_comparator, block_powered_comparator, block_daylight_detector, block_redstone_block,
    block_quartz_ore, block_hopper, block_quartz_block, block_quartz_stairs, block_activator_rail, block_dropper,
    block_stained_hardened_clay, block_stained_glass_pane, block_leaves2, block_log2, block_acacia_stairs,
    block_dark_oak_stairs, block_slime, block_barrier, block_iron_trapdoor, block_prismarine, block_sea_lantern,
    block_hay_block, block_carpet, block_hardened_clay, block_coal_block, block_packed_ice, block_double_plant
} block_kind;

/* coord - define location */
typedef struct {
    int x;
    int y;
    int z;
} coord;

/* rectangle - defines a 2D rectangle */
typedef struct {
    int width;
    int height;
} rectangle;

/* box - defines a 3D rectangular prism */
typedef struct {
    rectangle recth;
    rectangle rectv;
} box;

/* pattern - define an array (grid) of block kinds */
typedef struct {
    int kindData[MAX_PATTERN][MAX_PATTERN];
    int flags;
} pattern;

/* message functions */
int get_message(char* buffer,int size); /* return message_kind; place rest-of-line in buffer */
int get_message_ex(message* msg);
void issue_command(int kind,const char* tokens[],int size);


/* lower-level apply functions */
inline void apply_block(int kind,int x,int y,int z);
inline void apply_block_ex(int kind,const coord* loc);
void apply_rectangle(int kind,int x,int y,int z,int width,int height);
void apply_rectangle_ex(int kind,const coord* loc,const rectangle* rect);
void apply_rectangle_pat(const pattern* pat,const coord* loc,const rectangle* rect);
void apply_box(int kind,int x,int y,int z,int widthv,int heightv,int widthh,int heighth);
void apply_box_ex(int kind,const coord* loc,const box* bx);
void apply_box_pat(const pattern* pat,const coord* loc,const box* bx);

/* higher-level apply functions */

#endif
