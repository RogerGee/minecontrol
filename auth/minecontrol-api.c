/* minecontrol-api.c */
#include "minecontrol-api.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char* const MESSAGE_KINDS[] = {
    MESSAGE_PLAYER_CHAT, MESSAGE_SERVER_CHAT, MESSAGE_SERVER_START,
    MESSAGE_SERVER_BIND, MESSAGE_SERVER_SHUTDOWN, MESSAGE_PLAYER_LOGIN,
    MESSAGE_PLAYER_ID, MESSAGE_PLAYER_JOIN, MESSAGE_PLAYER_LOGOUT_CONNECTION,
    MESSAGE_PLAYER_LOST_CONNECTION, MESSAGE_PLAYER_LEAVE,
    MESSAGE_PLAYER_ACHIEVEMENT, MESSAGE_UNKNOWN
};

const char* const COMMAND_FORMATS[] = {
    COMMAND_SAY, COMMAND_SETBLOCK, COMMAND_SUMMON, COMMAND_TELEPORT
};

const char* const BLOCK_IDS[] = {
   "minecraft:air", "minecraft:stone", "minecraft:grass", "minecraft:dirt", "minecraft:cobblestone", 
   "minecraft:planks", "minecraft:sapling", "minecraft:bedrock", "minecraft:flowing_water", 
   "minecraft:water", "minecraft:flowing_lava", "minecraft:lava", "minecraft:sand", "minecraft:gravel", 
   "minecraft:gold_ore", "minecraft:iron_ore", "minecraft:coal_ore", "minecraft:log", "minecraft:leaves", 
   "minecraft:sponge", "minecraft:glass", "minecraft:lapis_ore", "minecraft:dispenser", "minecraft:sandstone", 
   "minecraft:noteblock", "minecraft:bed", "minecraft:golden_rail", "minecraft:detector_rail", "minecraft:sticky_piston", 
   "minecraft:web", "minecraft:tallgrass", "minecraft:deadbush", "minecraft:piston", "minecraft:piston_head", "minecraft:wool", 
   "minecraft:piston_extension", "minecraft:yellow_flower", "minecraft:red_flower", "minecraft:brown_mushroom", 
   "minecraft:red_mushroom", "minecraft:gold_block", "minecraft:iron_block", "minecraft:double_stone_slab", 
   "minecraft:stone_slab", "minecraft:brick_block", "minecraft:tnt", "minecraft:bookshelf", "minecraft:mossy_cobblestone", 
   "minecraft:obsidian", "minecraft:torch", "minecraft:fire", "minecraft:mob_spawner", "minecraft:oak_stairs", "minecraft:chest", 
   "minecraft:redstone_wire", "minecraft:diamond_ore", "minecraft:diamond_block", "minecraft:crafting_table", "minecraft:wheat", 
   "minecraft:farmland", "minecraft:furnace", "minecraft:lit_furnace", "minecraft:standing_sign", "minecraft:wooden_door", 
   "minecraft:ladder", "minecraft:rail", "minecraft:stone_stairs", "minecraft:wall_sign", "minecraft:lever", "minecraft:stone_pressure_plate", 
   "minecraft:iron_door", "minecraft:wooden_pressure_plate", "minecraft:redstone_ore", "minecraft:lit_redstone_ore", 
   "minecraft:unlit_redstone_torch", "minecraft:redstone_torch", "minecraft:stone_button", "minecraft:snow_layer", 
   "minecraft:ice", "minecraft:snow", "minecraft:cactus", "minecraft:clay", "minecraft:reeds", "minecraft:jukebox", "minecraft:fence", 
   "minecraft:pumpkin", "minecraft:netherrack", "minecraft:soul_sand", "minecraft:glowstone", "minecraft:portal", "minecraft:lit_pumpkin", 
   "minecraft:cake", "minecraft:unpowered_repeater", "minecraft:powered_repeater", "minecraft:stained_glass", "minecraft:trapdoor", 
   "minecraft:monster_egg", "minecraft:stonebrick", "minecraft:brown_mushroom_block", "minecraft:red_mushroom_block",  
   "minecraft:iron_bars", "minecraft:glass_pane", "minecraft:melon_block", "minecraft:pumpkin_stem", "minecraft:melon_stem", 
   "minecraft:vine", "minecraft:fence_gate", "minecraft:brick_stairs", "minecraft:stone_brick_stairs", "minecraft:mycelium", 
   "minecraft:waterlily", "minecraft:nether_brick", "minecraft:nether_brick_fence", "minecraft:nether_brick_stairs", 
   "minecraft:nether_wart", "minecraft:enchanting_table", "minecraft:brewing_stand", "minecraft:cauldron", "minecraft:end_portal", 
   "minecraft:end_portal_frame", "minecraft:end_stone", "minecraft:dragon_egg", "minecraft:redstone_lamp", "minecraft:lit_redstone_lamp", 
   "minecraft:double_wooden_slab", "minecraft:wooden_slab", "minecraft:cocoa", "minecraft:sandstone_stairs", "minecraft:emerald_ore", 
   "minecraft:ender_chest", "minecraft:tripwire_hook", "minecraft:tripwire", "minecraft:emerald_block", "minecraft:spruce_stairs", 
   "minecraft:birch_stairs", "minecraft:jungle_stairs", "minecraft:command_block", "minecraft:beacon", "minecraft:cobblestone_wall", 
   "minecraft:flower_pot", "minecraft:carrots", "minecraft:potatoes", "minecraft:wooden_button", "minecraft:skull", "minecraft:anvil", 
   "minecraft:trapped_chest", "minecraft:light_weighted_pressure_plate", "minecraft:heavy_weighted_pressure_plate",  
   "minecraft:unpowered_comparator", "minecraft:powered_comparator", "minecraft:daylight_detector", "minecraft:redstone_block", 
   "minecraft:quartz_ore", "minecraft:hopper", "minecraft:quartz_block", "minecraft:quartz_stairs", "minecraft:activator_rail", "minecraft:dropper", 
   "minecraft:stained_hardened_clay", "minecraft:stained_glass_pane", "minecraft:leaves2", "minecraft:log2", "minecraft:acacia_stairs", 
   "minecraft:dark_oak_stairs", "minecraft:slime", "minecraft:barrier", "minecraft:iron_trapdoor", "minecraft:prismarine", "minecraft:sea_lantern", 
   "minecraft:hay_block", "minecraft:carpet", "minecraft:hardened_clay", "minecraft:coal_block", "minecraft:packed_ice", "minecraft:double_plant"
};

/* token functions */
void token_init(token* tok)
{
    memset(tok,0,sizeof(token));
}
void token_assign_flag(token* tok,int flag)
{
    tok->tok_flag = flag;
}
void token_assign_int(token* tok,int i)
{
    tok->tok_flag = i;
}
void token_assign_float(token* tok,float f)
{
    tok->tok_float = f;
}
void token_assign_str(token* tok,const char* str)
{
    tok->tok_str = str;
}
const char* token_to_str(token* tok,int kind)
{
    static char rbuf[MAX_BUFFER+1];
    return token_to_str_ex(tok,kind,rbuf);
}
const char* token_to_str_ex(token* tok,int kind,char* rbuf)
{
    *rbuf = 0;
    switch (kind) {
    case t_string:
        return tok->tok_str;
    case t_integer:
    case t_datavalue:
        sprintf(rbuf,"%d",tok->tok_flag);
        break;
    case t_float:
        sprintf(rbuf,"%.2f",tok->tok_float);
        break;
    case t_message:
        return MESSAGE_KINDS[tok->tok_flag];
    case t_command:
        return COMMAND_FORMATS[tok->tok_flag];
    case t_block:
        return BLOCK_IDS[tok->tok_flag];
    case t_blockhandling:
        if (tok->tok_flag == h_keep)
            return "keep";
        else if (tok->tok_flag == h_destroy)
            return "destroy";
        else if (tok->tok_flag == h_replace)
            return "replace";
        else
            break;
    default:
        break;
    }
    return rbuf;
}

/* message functions */
void message_init(message* msg)
{
    msg->ta_top = 0;
    msg->tb_top = 0;
    msg->msg_kind = m_invalid;
    msg->msg_buffer[0] = 0;
    msg->msg_tokens[0] = NULL;
    msg->msg_tokbuffer[0] = 0;
}
int message_assign(message* msg,const char* source)
{
    const char* tok;
    int length, index;
    tok = source;
    length = 0;
    index = 0;
    message_init(msg); /* clear previous state */
    while (tok[length]) {
        if (tok[length]==' ' && length>0) {
            if (index == 0) {
                int i, j;
                for (i = m_eoi;i <= m_unknown;++i)
                    if (strncmp(tok,MESSAGE_KINDS[i],length) == 0)
                        break;
                if (i > m_unknown)
                    return 1; /* message is not understood */
                /* assign message kind and write rest-of-line to msg->msg_buffer */
                msg->msg_kind = i;
                i = length+1;
                j = 0;
                while (tok[i] && j<MAX_BUFFER)
                    msg->msg_buffer[j++] = tok[i++];
                msg->msg_buffer[j] = 0;
            }
            else {
                /* add token */
                int i;
                i = msg->tb_top;
                if (i+length > MAX_BUFFER)
                    length = MAX_BUFFER - i;
                if (length > 0) {
                    strncpy(msg->msg_tokbuffer+msg->tb_top,tok,length);
                    msg->tb_top += length;
                    msg->msg_tokbuffer[msg->tb_top++] = 0;
                    msg->msg_tokens[msg->ta_top++] = msg->msg_tokbuffer+i;
                }
            }
            tok += length+1;
            length = 0;
        }
        else
            ++length;
    }
    return 0;
}

/*test*/
int main()
{
    printf("Testing: %s\n",BLOCK_IDS[b_reeds]);

    return 0;
}
