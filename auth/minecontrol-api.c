/* minecontrol-api.c */
#define _GNU_SOURCE
#include "minecontrol-api.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

const char* const MESSAGE_KINDS[] = {
    MESSAGE_PLAYER_CHAT, MESSAGE_SERVER_CHAT, MESSAGE_SERVER_START,
    MESSAGE_SERVER_BIND, MESSAGE_SERVER_SHUTDOWN, MESSAGE_PLAYER_LOGIN,
    MESSAGE_PLAYER_ID, MESSAGE_PLAYER_JOIN, MESSAGE_PLAYER_LOGOUT_CONNECTION,
    MESSAGE_PLAYER_LOST_CONNECTION, MESSAGE_PLAYER_LEAVE,
    MESSAGE_PLAYER_ACHIEVEMENT, MESSAGE_UNKNOWN
};

const char* const COMMAND_FORMATS[] = {
    COMMAND_GENERIC, COMMAND_SAY, COMMAND_SETBLOCK, COMMAND_SETBLOCK_EX,
    COMMAND_SETBLOCK_COORD, COMMAND_SETBLOCK_COORD_EX, COMMAND_SUMMON,
    COMMAND_SUMMON_F, COMMAND_SUMMON_COORD, COMMAND_SUMMON_COORD_F, 
    COMMAND_TELEPORT, COMMAND_TELEPORT_F, COMMAND_TELEPORT_COORD,
    COMMAND_TELEPORT_COORD_F
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
void token_assign_float(token* tok,double f)
{
    tok->tok_float = f;
}
void token_assign_str(token* tok,const char* str)
{
    tok->tok_str = str;
}
const char* token_to_str(const token* tok,int kind,char* rbuf,int size)
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
    case t_coord:
        sprintf(rbuf,"%d %d %d",tok->tok_coord.coord_x,tok->tok_coord.coord_y,tok->tok_coord.coord_z);
        break;
    case t_coordf:
        sprintf(rbuf,"%.2f %.2f %.2f",tok->tok_coordf.coord_x,tok->tok_coordf.coord_y,tok->tok_coordf.coord_z);
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
    while (1) {
        if (tok[length]==0 || isspace(tok[length])) {
            if (length > 0) {
                if (index == 0) {
                    int flag, i, j;
                    for (flag = _m_first;flag <= _m_last;++flag)
                        if (length==strlen(MESSAGE_KINDS[flag]) && strncmp(tok,MESSAGE_KINDS[flag],length)==0)
                            break;
                    /* write rest-of-line to msg->msg_buffer */
                    i = length+1;
                    j = 0;
                    while (tok[i] && j<MAX_BUFFER)
                        msg->msg_buffer[j++] = tok[i++];
                    msg->msg_buffer[j] = 0;
                    /* assign flag kind */
                    if (flag > m_unknown) {
                        msg->msg_kind = m_invalid;
                        return -1; /* message is not understood */
                    }
                    else
                        msg->msg_kind = flag;
                }
                else {
                    /* add token */
                    int i;
                    i = msg->tb_top;
                    if (i+length > MAX_BUFFER)
                        length = MAX_BUFFER - i;
                    if (length>0 && msg->ta_top<MAX_MESSAGE_TOKENS) {
                        strncpy(msg->msg_tokbuffer+msg->tb_top,tok,length);
                        msg->tb_top += length;
                        msg->msg_tokbuffer[msg->tb_top++] = 0;
                        msg->msg_tokens[msg->ta_top++] = msg->msg_tokbuffer+i;
                    }
                }
                if (tok[length]) {
                    tok += length;
                    length = 0;
                    ++index;
                }
            }
            if (*tok == 0)
                break;
            ++tok;
        }
        else
            ++length;
    }
    /* add terminating NULL ptr to tokens list */
    msg->msg_tokens[msg->ta_top++] = NULL;
    return 0;
}

/* callback_parameter functions */
static void _callback_parameter_free(int s_top,char** s_tokens)
{
    int i;
    for (i = 0;i < s_top;++i)
        free(s_tokens[i]);
}
int callback_parameter_init(callback_parameter* params,const char* format,char* message,int* i_tokens,char* c_tokens,double* f_tokens,char** s_tokens)
{
    params->i_top = 0;
    params->c_top = 0;
    params->f_top = 0;
    params->s_top = 0;
    params->i_tokens = NULL;
    params->c_tokens = NULL;
    params->f_tokens = NULL;
    params->s_tokens = NULL;
    if (format != NULL) {
        /* support the following message tokens for fundamental types:
            - %i (integer)
            - %c (character)
            - %f (floating-point number)
            - %s (string) */
        /* parse message; tokens are separated by the next character in the format string */
        while (*format && *message) {
            if (*format == '%') {
                char t_kind;
                t_kind = *(++format);
                if (t_kind) {
                    char t_sep;
                    t_sep = *(++format);
                    if (t_sep) {
                        int i;
                        i = 0;
                        while (message[i] && message[i]!=t_sep)
                            ++i;
                        /* substitue null separator temporarily */
                        message[i] = 0;
                        /* process string */
                        switch (t_kind) {
                        case 'i':
                            if (params->i_top<MAX_MESSAGE_TOKENS && sscanf(message,"%d",i_tokens+params->i_top)==1)
                                ++params->i_top;
                        case 'c':
                            if (params->c_top<MAX_MESSAGE_TOKENS && sscanf(message,"%c",c_tokens+params->c_top)==1)
                                ++params->c_top;
                        case 'f':
                            if (params->f_top<MAX_MESSAGE_TOKENS && sscanf(message,"%lf",f_tokens+params->f_top)==1)
                                ++params->f_top;
                        case 's':
                            if (params->s_top < MAX_MESSAGE_TOKENS) {
                                s_tokens[params->s_top] = malloc(i);
                                strcpy(s_tokens[params->s_top],message);
                                ++params->s_top;
                            }
                        default:
                            _callback_parameter_free(params->s_top,s_tokens);
                            return -1;
                        }
                        /* replace old separator and advance message */
                        message[i] = t_sep;
                        message += i;
                    }
                }
            }
            if (*format != *message) {
                _callback_parameter_free(params->s_top,s_tokens);
                return -1;
            }
            ++format;
            ++message;
        }
        if (*format != *message) {
            _callback_parameter_free(params->s_top,s_tokens);
            return -1;
        }
    }
    params->i_tokens = i_tokens;
    params->c_tokens = c_tokens;
    params->f_tokens = f_tokens;
    params->s_tokens = s_tokens;
    return 0;
}
void callback_parameter_destroy(callback_parameter* params)
{
    params->i_tokens = NULL;
    params->c_tokens = NULL;
    params->f_tokens = NULL;
    _callback_parameter_free(params->s_top,params->s_tokens);
    params->s_tokens = NULL;
}

/* command functions */
void command_init(command* com)
{
    com->com_kind = c_invalid;
    com->com_toktop = 0;
}
int _command_assign(token* tokens,int* tokcnt,const char* fmt,va_list vargs)
{
    /* use the format string to mark the number of needed tokens */
    while (*fmt) {
        if (*fmt == '%') {
            /* look the token type and interpret the next argument */
            token* tok = tokens + (*tokcnt)++;
            int tkind = *(++fmt);
            if (tkind == t_invalid)
                break;
            if (tkind == t_string)
                tok->tok_str = va_arg(vargs,const char*);
            else if (tkind == t_float)
                tok->tok_float = va_arg(vargs,double);
            else if (tkind == t_coord)
                tok->tok_coord = va_arg(vargs,coord);
            else if (tkind == t_coordf)
                tok->tok_coordf = va_arg(vargs,coordf);
            else
                tok->tok_flag = va_arg(vargs,int);
        }
        ++fmt;
    }
    return 0;
}
int command_assign(command* com,int kind, ...)
{
    if (kind>=_c_first && kind<=_c_last) {
        int code;
        va_list vargs;
        const char* fmt;
        va_start(vargs,kind);
        fmt = COMMAND_FORMATS[kind];
        com->com_kind = kind;
        com->com_toktop = 0;
        code = _command_assign(com->com_tokens,&com->com_toktop,fmt,vargs);
        va_end(vargs);
        return code;
    }
    return -1;
}
token* command_create_token(command* com)
{
    if (com->com_toktop < MAX_COMMAND_TOKENS)
        return com->com_tokens + com->com_toktop++;
    return NULL;
}
void command_add_token(command* com,const char* str)
{
    if (com->com_toktop < MAX_COMMAND_TOKENS)
        com->com_tokens[com->com_toktop++].tok_str = str;
}
void command_add_token_byflag(command* com,int flag)
{
    if (com->com_toktop < MAX_COMMAND_TOKENS)
        com->com_tokens[com->com_toktop++].tok_flag = flag;
}
token* command_get_token(command* com,int position)
{
    return com->com_tokens+position;
}
void command_assign_token(command* com,int position,const char* str)
{
    com->com_tokens[position].tok_str = str;
}
void command_assign_token_byflag(command* com,int position,int flag)
{
    com->com_tokens[position].tok_flag = flag;
}
static int _command_compile(const char* fmt,const token* tokens,int tokcnt,char* buffer,int size)
{
    int i, top;
    i = 0;
    top = 0;
    /* save 2 bytes for the terminator sequence */
    size -= 2;
    if (size < 0)
        return -1;
    while (*fmt && i<size) {
        if (*fmt == '%') {
            ++fmt;
            if (*fmt == t_invalid)
                break;
            if (top < tokcnt) {
                const char* str;
                char tmpbuffer[MAX_BUFFER];
                str = token_to_str(tokens+top++,*fmt,tmpbuffer,MAX_BUFFER);
                /* copy string to output buffer */
                while (*str && i<size)
                    buffer[i++] = *str++;
            }
        }
        else
            /* place character in output buffer */
            buffer[i++] = *fmt;
        ++fmt;
    }
    /* add command terminator */
    strcpy(buffer+i,"\n");
    return ++i;
}
int command_compile(const command* com,char* buffer,int size)
{
    const char* fmt;
    fmt = COMMAND_FORMATS[com->com_kind];
    return _command_compile(fmt,com->com_tokens,com->com_toktop,buffer,size);
}

/* blockmap functions */
void blockmap_init(blockmap* bmap,int defaultAllocation)
{
    int i;
    if (defaultAllocation < 1)
        defaultAllocation = 16;
    bmap->bmap_data = malloc(block_kind_size * sizeof(command*));
    for (i = 0;i < block_kind_size;++i)
        bmap->bmap_data[i] = NULL;
    bmap->bmap_cmd_alloc = defaultAllocation;
    bmap->bmap_cmd_top = 0;
    bmap->bmap_cmd_data = malloc(defaultAllocation * sizeof(command));
}
void blockmap_destroy(blockmap* bmap)
{
    if (bmap->bmap_data != NULL) {
        free(bmap->bmap_data);
        bmap->bmap_data = NULL;
    }
    if (bmap->bmap_cmd_data != NULL) {
        free(bmap->bmap_cmd_data);
        bmap->bmap_cmd_data = NULL;
    }
    bmap->bmap_cmd_alloc = 0;
    bmap->bmap_cmd_top = 0;
}
static void _blockmap_reallocate(blockmap* bmap)
{
    /* allocate bigger amount of command structures */
    int i;
    command* pnew;
    bmap->bmap_cmd_alloc *= 2;
    pnew = malloc(sizeof(command) * bmap->bmap_cmd_alloc);
    for (i = 0;i < bmap->bmap_cmd_top;++i) {
        int j;
        pnew[i].com_kind = bmap->bmap_cmd_data[i].com_kind;
        pnew[i].com_toktop = bmap->bmap_cmd_data[i].com_toktop;
        for (j = 0;j < bmap->bmap_cmd_data[i].com_toktop;++j)
            pnew[i].com_tokens[j] = bmap->bmap_cmd_data[i].com_tokens[j];
    }
    free(bmap->bmap_cmd_data);
    bmap->bmap_cmd_data = pnew;
}
command* blockmap_insert(blockmap* bmap,int kind)
{
    if (kind>=0 && kind<block_kind_size && bmap->bmap_data[kind]==NULL) {
        command* com;
        if (bmap->bmap_cmd_top >= bmap->bmap_cmd_alloc)
            _blockmap_reallocate(bmap);
        com = bmap->bmap_cmd_data + bmap->bmap_cmd_top++;
        command_init(com);
        command_assign(com,kind,COMMAND_SETBLOCK,0,0,0,kind);
        bmap->bmap_data[kind] = com;
        return com;
    }
    return NULL;
}
void blockmap_insert_ex(blockmap* bmap,int* kinds,int count)
{
    int i;
    for (i = 0;i < count;++i, ++kinds)
        blockmap_insert(bmap,*kinds);
}
command* blockmap_lookup(blockmap* bmap,int kind,int x,int y,int z)
{
    command* com = NULL;
    if (kind>=0 && kind<block_kind_size && (com = bmap->bmap_data[kind]) != NULL) {
        command_assign_token_byflag(com,0,x);
        command_assign_token_byflag(com,1,y);
        command_assign_token_byflag(com,2,z);
    }
    return com;
}
command* blockmap_lookup_ex(blockmap* bmap,int kind,const coord* loc)
{
    command* com = NULL;
    if (kind>=0 && kind<block_kind_size && (com = bmap->bmap_data[kind]) != NULL) {
        command_assign_token_byflag(com,0,loc->coord_x);
        command_assign_token_byflag(com,1,loc->coord_y);
        command_assign_token_byflag(com,2,loc->coord_z);
    }
    return com;
}

/* general operation functions - the C IO library should be thread-safe, but not re-entrant; this
   is okay seeing as the order that messages are sent is not important */
int get_message(char* buffer,int size)
{
    int kind;
    int length;
    const char* tok;
    /* read a line of input */
    if (fgets(buffer,size,stdin) == NULL) {
        buffer[0] = 0;
        return m_eoi;
    }
    /* get first token */
    length = 0;
    tok = buffer;
    while (*tok && !isspace(*tok)) {
        ++tok;
        ++length;
    }
    /* see if it matches one of the message kinds */
    for (kind = _m_first;kind <= _m_last;++kind)
        if (strlen(MESSAGE_KINDS[kind])==length && strncmp(tok,MESSAGE_KINDS[kind],length)==0)
            break;
    if (kind > m_unknown)
        return m_invalid;
    return kind;
}
int get_message_ex(message* msg)
{
    char buffer[MAX_BUFFER];
    if (fgets(buffer,MAX_BUFFER,stdin) == NULL)
        return m_eoi;
    message_assign(msg,buffer);
    return msg->msg_kind;
}
int issue_command(int kind, ...)
{
    if (kind>=_c_first && kind<=_c_last) {
        int code;
        command com;
        va_list vargs;
        const char* fmt;
        fmt = COMMAND_FORMATS[kind];
        va_start(vargs,kind);
        command_init(&com);
        com.com_kind = kind;
        code = _command_assign(com.com_tokens,&com.com_toktop,fmt,vargs);
        va_end(vargs);
        /* if the token assignment was successful, compile the command and send it to stdout */
        if (code != -1) {
            int sz;
            char buffer[MAX_BUFFER];
            if ((sz = command_compile(&com,buffer,MAX_BUFFER)) <= 0)
                return -1;
            fwrite(buffer,1,sz,stdout);
            fflush(stdout);
        }
        return 0;
    }
    return -1;
}
int issue_command_ex(const command* com)
{
    int sz;
    char buffer[MAX_BUFFER];
    if ((sz = command_compile(com,buffer,MAX_BUFFER)) > 0) {
        fwrite(buffer,1,sz,stdout);
        fflush(stdout);
        return 0;
    }
    return -1;
}
int issue_command_str(const char* fmt, ...)
{
    int sz;
    va_list vargs;
    int tokenCount;
    char buffer[MAX_BUFFER];
    token tokens[MAX_COMMAND_TOKENS];
    tokenCount = 0;
    va_start(vargs,fmt);
    _command_assign(tokens,&tokenCount,fmt,vargs);
    va_end(vargs);
    if ((sz = _command_compile(fmt,tokens,tokenCount,buffer,MAX_BUFFER)) <= 0)
        return -1;
    fwrite(buffer,1,sz,stdout);
    fflush(stdout);
    return 0;
}

/* types used to implement higher-level interface */
struct sync_info
{
    callback_sync cback;
    int kind;
    const char* format;
};
static void sync_info_default(struct sync_info* sinfo)
{
    /* put the object in a default invalid state */
    sinfo->cback = NULL;
    sinfo->kind = m_invalid;
    sinfo->format = NULL;
}
static void sync_info_init(struct sync_info* sinfo,callback_sync cback,int kind,const char* format)
{
    /* put the object in a usable state */
    sinfo->cback = cback;
    sinfo->kind = kind;
    sinfo->format = format;
}
static int sync_info_invoke(struct sync_info* sinfo,char* message)
{
    /* invoke the callback with the specified message contents;
       callback parameter data is stored here on the stack */
    callback_parameter params;
    int i_tokens[MAX_MESSAGE_TOKENS];
    char c_tokens[MAX_MESSAGE_TOKENS];
    double f_tokens[MAX_MESSAGE_TOKENS];
    char* s_tokens[MAX_MESSAGE_TOKENS];
    if (callback_parameter_init(&params,sinfo->format,message,i_tokens,c_tokens,f_tokens,s_tokens) == 0) {
        (*sinfo->cback)(sinfo->kind,message,&params);
        callback_parameter_destroy(&params);
        return 0;    
    }
    return 1;
}
static void sync_info_destroy(struct sync_info* sinfo)
{
    /* put the object in an unusable state */
    sinfo->cback = NULL;
    sinfo->kind = m_invalid;
    sinfo->format = NULL;
}

struct async_info
{
    int kind;
    const char* format;
    callback_async cback;
    volatile short status;
    unsigned char tids_in_use[MAX_ASYNC_INSTANCES];
    pthread_t tids[MAX_ASYNC_INSTANCES];
};
struct async_param_info
{
    struct async_info* ainfo;
    char* message;
};
static void async_info_default(struct async_info* ainfo)
{
    /* put the object in a default invalid state */
    ainfo->kind = m_invalid;
    ainfo->format = NULL;
    ainfo->cback = NULL;
    ainfo->status = 0;
}
static void async_info_init(struct async_info* ainfo,callback_async cback,int kind,const char* format)
{
    /* put the object in a usable state */
    int i;
    ainfo->cback = cback;
    ainfo->kind = kind;
    ainfo->format = format;
    ainfo->status = 1;
    for (i = 0;i < MAX_ASYNC_INSTANCES;++i)
        ainfo->tids_in_use[i] = 0;
}
static void* async_info_thread_start(void* pparam)
{
    callback_parameter params;
    struct async_param_info* apinfo = (struct async_param_info*)pparam;
    int i_tokens[MAX_MESSAGE_TOKENS];
    char c_tokens[MAX_MESSAGE_TOKENS];
    double f_tokens[MAX_MESSAGE_TOKENS];
    char* s_tokens[MAX_MESSAGE_TOKENS];
    if (callback_parameter_init(&params,apinfo->ainfo->format,apinfo->message,i_tokens,c_tokens,f_tokens,s_tokens) == 0) {
        (*apinfo->ainfo->cback)(&apinfo->ainfo->status,apinfo->ainfo->kind,apinfo->message,&params);
        callback_parameter_destroy(&params);
        free(apinfo->message);
        free(apinfo);
    }
    pthread_exit(NULL);
}
static int async_info_invoke(struct async_info* ainfo,char* message)
{
    int i;
    i = 0;
    while (i < MAX_ASYNC_INSTANCES)
        if (!ainfo->tids_in_use[i])
            break;
    if (i < MAX_ASYNC_INSTANCES) {
        /* prepare thread info parameter on heap */
        struct async_param_info* apinfo;
        apinfo = malloc(sizeof(struct async_param_info));
        apinfo->message = malloc(strlen(message));
        strcpy(apinfo->message,message);
        ainfo->tids_in_use[i] = 1;
        /* start thread */
        if (pthread_create(ainfo->tids + i,NULL,&async_info_thread_start,apinfo) == 0)
            return 0;
    }
    return -1;
}
static int async_info_check(struct async_info* ainfo)
{
    int i;
    void* pv;
    int found = -1;
    for (i = 0;i < MAX_ASYNC_INSTANCES;++i) {
        if (ainfo->tids_in_use[i] && pthread_tryjoin_np(ainfo->tids[i],&pv)==0) {
            ainfo->tids_in_use[i] = 0;
            found = 0;
        }
    }
    return found;
}
static void async_info_destroy(struct async_info* ainfo)
{
    int i;
    for (i = 0;i < MAX_ASYNC_INSTANCES;++i) {
        if (ainfo->tids_in_use[i])
            /* join back up with the thread; block until it quits */
            pthread_join(ainfo->tids[i],NULL);
    }
    async_info_default(ainfo);
}

/* global data used by higher-level interface */
static volatile short _minecontrol_api_itrack_state = 0;
static blockmap _minecontrol_api_bmap;
static pthread_mutex_t _minecontrol_api_protect_bmap = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _minecontrol_api_protect_hooks = PTHREAD_MUTEX_INITIALIZER;
static struct sync_info _minecontrol_api_sync_hooks[_m_last+1];
static struct async_info _minecontrol_api_async_hooks[_m_last+1];

/* higher-level init/destroy functions */
void init_minecontrol_api()
{
    int i;
    blockmap_init(&_minecontrol_api_bmap,32);
    for (i = 0;i <= _m_last;++i) {
        sync_info_default(_minecontrol_api_sync_hooks + i);
        async_info_default(_minecontrol_api_async_hooks + i);
    }
}
int hook_tracking_function(callback_sync func,int kind,const char* format)
{
    if (kind>=0 && kind<=_m_last) {
        sync_info_init(_minecontrol_api_sync_hooks + kind,func,kind,format);
        return 0;
    }
    return -1;
}
int hook_tracking_function_async(callback_async func,int kind,const char* format)
{
    if (kind>=0 && kind<=_m_last) {
        async_info_init(_minecontrol_api_async_hooks + kind,func,kind,format);
        return 0;
    }
    return -1;
}
int begin_input_tracking(callback hookMain)
{
    if (!_minecontrol_api_itrack_state) {
        _minecontrol_api_itrack_state = 1;
        message msg;
        while (_minecontrol_api_itrack_state) {
            int kind;
            /* reset msg object */
            message_init(&msg);
            /* read message kind and message body; this call blocks */
            kind = get_message_ex(&msg);
            if (kind == m_invalid)
                continue;
            if (kind == m_eoi) /* end of input */
                break;
            if (_minecontrol_api_sync_hooks[kind].cback != NULL)
                sync_info_invoke(_minecontrol_api_sync_hooks + kind,msg.msg_buffer);
            if (_minecontrol_api_async_hooks[kind].cback != NULL)
                async_info_invoke(_minecontrol_api_sync_hooks + kind,msg.msg_buffer);
        }
        /* signal tracking is closing down */

        return 0;
    }
    return -1;
}
int end_input_tracking()
{
    _minecontrol_api_itrack_state = 0;
    return 0;
}
void close_minecontrol_api()
{
    int i;
    blockmap_destroy(&_minecontrol_api_bmap);
    _minecontrol_api_itrack_state = 0;
    for (i = 0;i <= _m_last;++i) {
        sync_info_destroy(_minecontrol_api_sync_hooks + i);
        async_info_destroy(_minecontrol_api_async_hooks + i);
    }
}

/* higher-level geometry functions */
void apply_block(int kind,int x,int y,int z)
{
    command* com;
    char buffer[MAX_BUFFER];
    /* get the command that we need from the blockmap */
    pthread_mutex_lock(&_minecontrol_api_protect_bmap);
    com = blockmap_lookup(&_minecontrol_api_bmap,kind,x,y,z);
    if (com == NULL)
        com = blockmap_insert(&_minecontrol_api_bmap,kind);
    if (command_compile(com,buffer,MAX_BUFFER) != -1)
        issue_command_ex(com);
    pthread_mutex_unlock(&_minecontrol_api_protect_bmap);
}
void apply_block_ex(int kind,const coord* loc)
{
    apply_block(kind,loc->coord_x,loc->coord_y,loc->coord_z);
}

/*test*/
int main()
{
    
    return 0;
}
