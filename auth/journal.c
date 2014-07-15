/* journal.c - minecontrol authority program for location journaling */
#include "minecontrol-api.h"
#include "tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* globals */
static const char* PROGRAM_NAME;

/* journaling options: these may be changed from the defaults by command-line arguments */
static short journal_allow_tp = 0; /* teleport players upon request to journaled locations */
static short journal_allow_tell = 1; /* whisper journaled location coordinates */
static short journal_allow_dir = 1; /* whisper directions to journaled location */
static short journal_allow_private = 1; /* allow private namespaces for journaled locations */
static short journal_allow_public = 0; /* allow a public namespace for journaled locations */
static short journal_allow_invite = 1; /* allow invite to private namespace locations */

/* journal location database: map user name to location tree which maps location name to location coordinates */
static struct search_tree database; /* key -> struct search_tree */
static struct search_tree telerecord; /* key -> struct tree_key */

/* provide a destructor for the payload of each key in 'database' */
static void user_database_destructor(void* payload);

/* file IO functions and constants */
static const char* const JOURNAL_FILE = "journal";
static void write_journal_file();
static void read_journal_file();

/* helpers */
static int add_default_entry(const char* user,const char* label);
static int update_entry(const char* user,const char* label,coord* location);
static coord* lookup_entry(const char* user,const char* label);
static int remove_entry(const char* user,const char* label);

/* callback functions for input tracking */
static int track_main(int kind,const char* message);
static int track_tell(int kind,const char* message,const callback_parameter* params);
static int track_dir(int kind,const char* message,const callback_parameter* params);
static int track_add(int kind,const char* message,const callback_parameter* params);
static int track_up(int kind,const char* message,const callback_parameter* params);
static int track_rm(int kind,const char* message,const callback_parameter* params);
static int track_tp(int kind,const char* message,const callback_parameter* params);
static int track_portmsg(int kind,const char* message,const callback_parameter* params);

/* main: load/unload database; process messages from minecontrol server */
int main(int argc,const char* argv[])
{
    fprintf(stderr,"%s: authority program start\n",argv[0]);
    PROGRAM_NAME = argv[0];
    /* load database */
    init_minecontrol_api();
    tree_init(&database);
    read_journal_file();
    /* register callbacks for input events */
    hook_tracking_function(&track_tell,m_chat,"%s journal tell %s");
    hook_tracking_function(&track_dir,m_chat,"%s journal dir %s");
    hook_tracking_function(&track_add,m_chat,"%s journal add %s");
    hook_tracking_function(&track_up,m_chat,"%s journal up %s");
    hook_tracking_function(&track_rm,m_chat,"%s journal rm %s");
    hook_tracking_function(&track_tp,m_chat,"%s journal tp %s");
    hook_tracking_function(&track_portmsg,m_playerteleported,"%s %n %n %n");
    /* begin handling messages */
    if (begin_input_tracking(&track_main) == -1)
        fprintf(stderr,"%s: fail begin_input_tracking()\n",argv[0]);
    /* write database to file and cleanup */
    write_journal_file();
    close_minecontrol_api();
    tree_destroy_ex(&database,&user_database_destructor);
    fprintf(stderr,"%s: authority program end\n",argv[0]);
    return 0;
}

/* function definitions */
void user_database_destructor(void* payload)
{
    /* payloads in 'database' are search trees; this
       function will be called to destroy them */
    tree_destroy((struct search_tree*)payload);
}

static char seek_whitespace(FILE* pfile)
{
    char c;
    while (isspace(c = fgetc(pfile)))
        ;
    return c;
}
static char seek_until(FILE* pfile,const char* until)
{
    char c;
    while (1) {
        int i;
        c = fgetc(pfile);
        i = 0;
        if (c == EOF)
            break;
        while (until[i]) {
            if (c == until[i])
                return c;
            ++i;
        }
    }
    return c;
}
static char assign_until(FILE* pfile,char** buffer,int* cap,int* top,char until)
{
    /* seek through characters until whitespace or 'until'; add valid
       characters to the dynamic buffer */
    char c;
    while (1) {
        c = fgetc(pfile);
        if (c==EOF || isspace(c) || c==until)
            break;
        if (*top >= *cap)
            *buffer = grow_dynamic_array(*buffer,cap,sizeof(char));
        (*buffer)[(*top)++] = c;
    }
    return c;
}
static int read_location_entry(FILE* journal,char** buffer,int* cap,struct tree_key** result)
{
    char c;
    int top;
    coord* entry;
    c = seek_whitespace(journal);
    if (c == EOF) {
        fprintf(stderr,"%s: database error: expected '}' (end-user-entry) or '[' (begin-location-entry) but found end of file\n",PROGRAM_NAME);
        return -1;
    }
    else if (c == '}')
        /* end of namespace entry */
        return -1;
    else if (c != '[') {
        fprintf(stderr,"%s: database error: expected '[' (begin-location-entry) but found '%c'\n",PROGRAM_NAME,c);
        return seek_until(journal,",}")==',' ? 1 : -1;
    }
    /* found open square bracket; read off the location name */
    c = seek_whitespace(journal);
    top = 0;
    while (c!=':' && c!='}' && c!=EOF) {
        if (top >= *cap)
            *buffer = grow_dynamic_array(*buffer,cap,sizeof(char));
        (*buffer)[top++] = c;
        c = fgetc(journal);
    }
    if (c == '}') {
        fprintf(stderr,"%s: database error: expected ':' (location-entry-kv-sep) but found '}' (end-location-entry)\n",PROGRAM_NAME);
        return 1;
    }
    if (c == EOF) {
        fprintf(stderr,"%s: database error: expected ':' (location-entry-kv-sep) but found end of file\n",PROGRAM_NAME);
        return -1;
    }
    /* null terminate 'buffer' */
    if (top >= *cap)
        *buffer = grow_dynamic_array(*buffer,cap,sizeof(char));
    (*buffer)[top++] = 0;
    /* read off the location coordinates; store them in binary format in memory */
    entry = malloc(sizeof(coord));
    if (fscanf(journal,"%d,%d,%d",&entry->coord_x,&entry->coord_y,&entry->coord_z) < 3) {
        fprintf(stderr,"%s: database error: failed parsing location coordinates for entry '%s'\n",PROGRAM_NAME,*buffer);
        free(entry);
        return seek_until(journal,",}")==',' ? 1 : -1;
    }
    /* create key */
    *result = malloc(sizeof(struct tree_key));
    tree_key_init(*result,*buffer,entry);
    /* read off ']' character */
    c = fgetc(journal);
    if (c != ']')
        ungetc(c,journal);
    return 0;
}
static int read_usernamespace_entry(FILE* journal,char** buffer,int* cap,struct tree_key** result)
{
    /* read a user-namespace entry; this consists of a user name and a series of location key-value pairs 
       in the journal file; return condition 1 to indicate a handled error; return condition -1 to indicate
       a fatal error and condition 0 to indicate success */
    char c;
    int top;
    int keycap;
    struct tree_key** keys;
    struct search_tree* entry;
    /* read file contents; use 'buffer' to store keys as they are read */
    top = 0;
    if ((c = seek_whitespace(journal)) == EOF)
        /* no entry */
        return -1;
    ungetc(c,journal);
    if ((c = assign_until(journal,buffer,cap,&top,'{')) == EOF) {
        fprintf(stderr,"%s: database error; expected '{' (begin-user-entry) but found end of file\n",PROGRAM_NAME);
        return -1;
    }
    /* if 'c' is not the open curly bracket then it must be space */
    if (c != '{') {
        if ((c = seek_whitespace(journal)) != '{') {
            if (c == EOF) {
                fprintf(stderr,"%s: database error: expected '{' (begin-user-entry) but found end of file\n",PROGRAM_NAME);
                return -1;
            }
            fprintf(stderr,"%s: database error: expected '{' (begin-user-entry) but found '%c'\n",PROGRAM_NAME,c);
            /* attempt to handle this problem by skipping past the next '}' character */
            return seek_until(journal,"}") == '}' ? 1 : -1;
        }
    }
    else if (top == 0) {
        fprintf(stderr,"%s: database error: expected user name before '{' (begin-user-entry)\n",PROGRAM_NAME);
        return seek_until(journal,"}") == '}' ? 1 : -1;
    }
    /* null terminate the user name */
    if (top >= *cap)
        *buffer = grow_dynamic_array(*buffer,cap,sizeof(char));
    (*buffer)[top++] = 0;
    /* 'c' is the open curly bracket for the namespace entry; create a new entry and create dynamic
       array 'keys' to store location entries */
    entry = malloc(sizeof(struct search_tree));
    tree_init(entry);
    *result = malloc(sizeof(struct tree_key));
    tree_key_init(*result,*buffer,entry);
    keys = init_dynamic_array(&keycap,sizeof(struct tree_key*),32);
    top = 0;
    /* read location entries */
    while (1) {
        int cond;
        if (top >= keycap)
            keys = grow_dynamic_array(keys,&keycap,sizeof(struct tree_key*));
        /* attempt to read the location entry; if the result was 1 then
           a handled error occurred so try again */
        do {
            cond = read_location_entry(journal,buffer,cap,keys+top);
        } while (cond == 1);
        if (cond != 0)
            break;
        ++top;
    }
    /* construct 'entry' using 'keys'; return success condition */
    tree_construct(entry,keys,top);
    free(keys);
    return 0;
}
void read_journal_file()
{
    int top;
    int bufcap;
    int keycap;
    char* buffer;
    FILE* journal;
    struct tree_key** keys;
    if ((journal = fopen(JOURNAL_FILE,"r")) == NULL) {
        if (errno == ENOENT)
            return;
        fprintf(stderr,"%s: couldn't open database file for reading: %s\n",PROGRAM_NAME,strerror(errno));
        return;
    }
    buffer = init_dynamic_array(&bufcap,sizeof(char),16);
    keys = init_dynamic_array(&keycap,sizeof(struct tree_key*),32);
    top = 0;
    while (1) {
        int cond;
        if (top >= keycap)
            keys = grow_dynamic_array(keys,&keycap,sizeof(struct tree_key*));
        /* attempt to read user-namespace entry; if a handled error occurs try again */
        do {
            cond = read_usernamespace_entry(journal,&buffer,&bufcap,keys+top);
        } while (cond == 1);
        if (cond != 0)
            break;
        ++top;
    }
    /* build the database */
    tree_construct(&database,keys,top);
    free(keys);
    free(buffer);
    fclose(journal);
}
static void write_location_entry(FILE* journal,int* width,const struct tree_node* node)
{
    int i;
    i = 0;
    while (node->keys[i] != NULL) {
        coord* loc;
        if (*width+strlen(node->keys[i]->key) > 115) {
            fputs("\n\t",journal);
            *width = 0;
        }
        loc = (coord*)node->keys[i]->payload;
        fprintf(journal,"[%s:%d,%d,%d] ",node->keys[i]->key,loc->coord_x,loc->coord_y,loc->coord_z);
        ++i;
    }
    i = 0;
    while (node->children[i] != NULL)
        write_location_entry(journal,width,node->children[i]);
}
static void write_usernamespace_entry(FILE* journal,const struct tree_node* node)
{
    int i;
    i = 0;
    while (node->keys[i] != NULL) {
        static int width;
        fprintf(journal,"%s{\n\t",node->keys[i]->key);
        width = 0;
        write_location_entry(journal,&width,((const struct search_tree*)node->keys[i]->payload)->root);
        fputs("\n}",journal);
        ++i;
    }
    i = 0;
    while (node->children[i] != NULL)
        write_usernamespace_entry(journal,node->children[i]);
}
void write_journal_file()
{
    FILE* journal;
    if ((journal = fopen(JOURNAL_FILE,"w+")) == NULL) {
        fprintf(stderr,"%s: couldn't open database file for writing: %s\n",PROGRAM_NAME,strerror(errno));
        return;
    }
    if (database.root != NULL) {
        write_usernamespace_entry(journal,database.root);
        fputc('\n',journal);
    }
    fclose(journal);
}

int add_default_entry(const char* user,const char* label)
{
    coord* loc;
    struct tree_key* key;
    /* create a coordinate structure on the heap for the key entry */
    loc = malloc(sizeof(coord));
    loc->coord_x = 0;
    loc->coord_y = 0;
    loc->coord_z = 0;
    /* search for the specified user */
    key = tree_lookup(&database,user);
    if (key == NULL) {
        /* create new user entry*/
        struct search_tree* newentry;
        newentry = malloc(sizeof(struct search_tree*));
        tree_init(newentry);
        tree_insert(newentry,label,loc);
        return tree_insert(&database,user,newentry);
    }
    if (tree_insert((struct search_tree*)key->payload,label,loc) == 1) {
        free(loc);
        return 1;
    }
    return 0;
}
int update_entry(const char* user,const char* label,coord* location)
{
    struct tree_key* key;
    key = tree_lookup(&database,user);
    if (key != NULL) {
        key = tree_lookup((struct search_tree*)key->payload,label);
        if (key != NULL) {
            *(coord*)key->payload = *location;
            return 0;
        }
    }
    return 1;
}
coord* lookup_entry(const char* user,const char* label)
{
    struct tree_key* key;
    key = tree_lookup(&database,user);
    if (key != NULL) {
        key = tree_lookup((struct search_tree*)key->payload,label);
        return (coord*)key->payload;
    }
    return NULL;
}
int remove_entry(const char* user,const char* label)
{
    struct tree_key* key;
    key = tree_lookup(&database,user);
    if (key != NULL)
        return tree_remove((struct search_tree*)key->payload,label);
    return NULL;
}

int track_main(int kind,const char* message)
{
}
int track_tell(int kind,const char* message,const callback_parameter* params)
{
}
int track_dir(int kind,const char* message,const callback_parameter* params)
{
}
int track_add(int kind,const char* message,const callback_parameter* params)
{
    int i;
    const char* label, nspace;
    /* the user provided a label (params->s_tokens[1]); this label is of the 
       form '[namespace:]label'; the namespace is a qualifier that groups 
       location labels together; if no namespace is explicitly given, then
       the player's name is used */
    i = 0;
    while (params->s_tokens[1][i] && params->s_tokens[1][i]!=':')
        ++i;
    if (params->s_tokens[1][i] == ':') {
        params->s_tokens[1][i] = 0;
        label = params->s_tokens[1] + i+1;
        nspace = params->s_tokens[1];
    }
    else {
        label = params->s_tokens[1];
        nspace = params->s_tokens[0];
    }
    /* attempt to add a default entry using the label */

    /* create an entry in the teleport record to catch later teleport messages */

    /* finally, teleport the player to their current location so as to prompt a
       teleport message that indicates their current location */

}
int track_up(int kind,const char* message,const callback_parameter* params)
{
}
int track_rm(int kind,const char* message,const callback_parameter* params)
{
}
int track_tp(int kind,const char* message,const callback_parameter* params)
{
}
int track_portmsg(int kind,const char* message,const callback_parameter* params)
{   
}
