/* journal.c - minecontrol authority program for location journaling */
#define _GNU_SOURCE
#include "minecontrol-api.h"
#include "tree.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

/* constants */
static const char* PROGRAM_NAME;
static const char* const AUTHORIZED_NAMESPACES[] = {
    "*public*",
    "*publicdomain*",
    NULL
};

/* signal handlers and alarm functionality */
static int setup_signal(int sig,void (*handler)(int));
static int setup_alarm(unsigned int seconds);
static void termination_handler(int);
static void alarm_handler(int);

/* journaling options: these may be changed from the defaults by command-line arguments */
static short journal_allow_tp = 0; /* teleport players upon request to journaled locations */
static short journal_allow_tell = 1; /* whisper journaled location coordinates */
static short journal_allow_private = 1; /* allow private namespaces for journaled locations */
static short journal_allow_public = 0; /* allow a public namespace for journaled locations */

/* journal location database: map user name to location tree which maps location name to location coordinates */
static struct search_tree database; /* key -> struct search_tree : key -> coord* */
static struct search_tree telerecord; /* key -> coord* */
static short database_updated = 0; /* becomes 1 if the database is modified in any way */
static short do_database_update = 0; /* becomes 1 if the database should be checked for updates and possibly saved */

/* provide a destructor for the payload of each key in 'database' */
static void user_database_destructor(void* payload);

/* command-line argument processing */
static void process_command_line_options(int argc,const char* argv[]);

/* file IO functions and constants */
static const char* const JOURNAL_FILE = "journal";
static void write_journal_file();
static void read_journal_file();

/* helpers */
static coord* add_default_entry(const char* user,const char* label);
static int update_entry(const char* user,const char* label,coord* location);

static coord* lookup_entry(const char* user,const char* label);
static int remove_entry(const char* user,const char* label);

/* callback functions for input tracking */
static int track_main(int kind,const char* message);
static int track_help(int kind,const char* message,const callback_parameter* params);
static int track_tell(int kind,const char* message,const callback_parameter* params);
static int track_add(int kind,const char* message,const callback_parameter* params);
static int track_up(int kind,const char* message,const callback_parameter* params);
static int track_rm(int kind,const char* message,const callback_parameter* params);
static int track_tp(int kind,const char* message,const callback_parameter* params);
static int track_portmsg(int kind,const char* message,const callback_parameter* params);

/* main: load/unload database; process messages from minecontrol server */
int main(int argc,const char* argv[])
{
    fprintf(stderr,"%s: authority program start\n",argv[0]);
    /* handle command-line arguments */
    PROGRAM_NAME = argv[0];
    process_command_line_options(argc-1,argv+1);
    /* setup signal handler for termination events */
    if (setup_signal(SIGTERM,&termination_handler)==-1 || setup_signal(SIGINT,&termination_handler)==-1
        || setup_signal(SIGPIPE,&termination_handler)==-1) {
        fprintf(stderr,"%s: could not setup signal handlers\n",argv[0]);
        return 1;
    }
    /* setup alarm handler */
    if (setup_alarm(3600) == -1) {
        fprintf(stderr,"%s: could not setup alarm signal handler\n",argv[0]);
        return 1;
    }
    /* load database */
    init_minecontrol_api();
    tree_init(&database);
    tree_init(&telerecord);
    read_journal_file();
    /* register callbacks for input events */
    hook_tracking_function(&track_help,m_chat,"%s journal help");
    if (journal_allow_tell)
        hook_tracking_function(&track_tell,m_chat,"%s journal tell %s");
    hook_tracking_function(&track_add,m_chat,"%s journal add %s");
    hook_tracking_function(&track_up,m_chat,"%s journal up %s");
    hook_tracking_function(&track_rm,m_chat,"%s journal rm %s");
    if (journal_allow_tp)
        hook_tracking_function(&track_tp,m_chat,"%s journal tp %s");
    hook_tracking_function(&track_portmsg,m_playerteleported,"%s %n %n %n");
    /* begin handling messages */
    if (begin_input_tracking(&track_main) == -1)
        fprintf(stderr,"%s: fail begin_input_tracking()\n",argv[0]);
    /* write database to file and cleanup */
    if (database_updated) {
        database_updated = 0;
        write_journal_file();
    }
    close_minecontrol_api();
    tree_destroy_nopayload(&telerecord);
    tree_destroy_ex(&database,&user_database_destructor);
    fprintf(stderr,"%s: authority program exit\n",argv[0]);
    return 0;
}

/* function definitions */
int setup_signal(int sig,void (*handler)(int))
{
    struct sigaction action;
    memset(&action,0,sizeof(struct sigaction));
    action.sa_handler = handler;
    return sigaction(sig,&action,NULL);
}
int setup_alarm(unsigned int seconds)
{
    struct itimerval timer;
    memset(&timer,0,sizeof(struct itimerval));
    timer.it_interval.tv_sec = seconds;
    timer.it_value.tv_sec = seconds;
    if (setup_signal(SIGALRM,&alarm_handler) == -1)
        return -1;
    if (setitimer(ITIMER_REAL,&timer,NULL) == -1)
        return -1;
    return 0;
}
void termination_handler(int signal)
{
    /* this modifies a global variable */
    end_input_tracking();
}
void alarm_handler(int signal)
{
    /* mark that an alarm event occured; an m_intr message
       will be handled later that will possible write the
       database file to disk if need be */
    do_database_update = 1;
}

void user_database_destructor(void* payload)
{
    /* payloads in 'database' are search trees; this
       function will be called to destroy them */
    tree_destroy((struct search_tree*)payload);
}

void process_command_line_options(int argc,const char* argv[])
{
    int i;
    for (i = 0;i < argc;++i) {
        if (strlen(argv[i])>=3 && argv[i][0]=='-') {
            int state;
            const char* op;
            switch (argv[i][1]) {
            case '-':
                state = 0;
                break;
            case '+':
                state = 1;
                break;
            default:
                fprintf(stderr,"%s: bad option set state character; expected '-' or '+', found '%c'\n",PROGRAM_NAME,argv[i][1]);
                continue;
            }
            op = argv[i] + 2;
            if (strcmp(op,"tp") == 0)
                journal_allow_tp = state;
            else if (strcmp(op,"tell") == 0)
                journal_allow_tell = state;
            else if (strcmp(op,"private") == 0)
                journal_allow_private = state;
            else if (strcmp(op,"public") == 0)
                journal_allow_public = state;
            else
                fprintf(stderr,"%s: ignoring unrecognized option '%s'\n",PROGRAM_NAME,op);
        }
    }
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
        /* do an in-order traversal so check the child first */
        if (node->children[i] != NULL)
            write_location_entry(journal,width,node->children[i]);
        /* write the location entry to file */
        *width += strlen(node->keys[i]->key);
        *width += 15;
        if (*width > 80) {
            fputs("\n\t",journal);
            *width = 0;
        }
        loc = (coord*)node->keys[i]->payload;
        fprintf(journal,"[%s:%d,%d,%d] ",node->keys[i]->key,loc->coord_x,loc->coord_y,loc->coord_z);
        ++i;
    }
    if (node->children[i] != NULL)
        write_location_entry(journal,width,node->children[i]);
}
static void write_usernamespace_entry(FILE* journal,const struct tree_node* node)
{
    int i;
    i = 0;
    while (node->keys[i] != NULL) {
        static int width;
        /* do an in-order traversal so check the child first */
        if (node->children[i] != NULL)
            write_usernamespace_entry(journal,node->children[i]);
        /* write the namespace entry to file */
        fprintf(journal,"%s{\n\t",node->keys[i]->key);
        width = 0;
        if (((const struct search_tree*)node->keys[i]->payload)->root != NULL)
            write_location_entry(journal,&width,((const struct search_tree*)node->keys[i]->payload)->root);
        fputs("\n}",journal);
        ++i;
    }
    if (node->children[i] != NULL)
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
    fprintf(stderr,"%s: wrote database to disk\n",PROGRAM_NAME);
    fclose(journal);
}

coord* add_default_entry(const char* user,const char* label)
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
        newentry = malloc(sizeof(struct search_tree));
        tree_init(newentry);
        tree_insert(newentry,label,loc);
        if (tree_insert(&database,user,newentry) == 1) {
            /* this frees 'loc' */
            tree_destroy(newentry);
            free(newentry);
            return NULL;
        }
    }
    else if (tree_insert((struct search_tree*)key->payload,label,loc) == 1) {
        free(loc);
        return NULL;
    }
    return loc;
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
        if (key != NULL)
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
    return 1;
}

static int parse_label(const char* result[],char* player,char* label)
{
    /* the user provided a label; this label is of the form '[namespace:]label';
       the namespace is a qualifier that groups location labels together; if no
       namespace is explicitly given, then the player's name is used */
    int i;
    i = 0;
    while (label[i] && label[i]!=':')
        ++i;
    if (label[i] == ':') {
        const char* const* auth;
        label[i] = 0;
        result[0] = label;
        result[1] = label + i+1;
        /* check to see if the player has access to the namespace; this means that
           the namespace is either one of the AUTHORIZED_NAMESPACES or is the player's
           namespace */
        if (journal_allow_public) {
            auth = AUTHORIZED_NAMESPACES;
            while (*auth != NULL) {
                if (tree_key_compare_raw(*auth,label) == 0)
                    break;
                ++auth;
            }
        }
        else
            auth = NULL;
        if ((auth==NULL || *auth==NULL) && (!journal_allow_private || tree_key_compare_raw(player,label)!=0)) {
            issue_command_str("tellraw %s {text:\"Access to namespace %s is not authorized\",color:\"red\"}",player,label);
            return 1;
        }
    }
    else {
        if (!journal_allow_private) {
            issue_command_str("tellraw %s {text:\"Access to private namespace is not allowed\",color:\"red\"}",player);
            return 1;
        }
        result[0] = player;
        result[1] = label;
    }
    return 0;
}
static coord* preop_task(const char* label[],char* player,char* inputlabel)
{
    /* parse label and lookup record */
    coord* record;
    if (parse_label(label,player,inputlabel) == 1)
        return NULL;
    record = lookup_entry(label[0],label[1]);
    if (record == NULL)
        issue_command_str("tellraw %s {text:\"Location does not exist in database\",color:\"red\"}",player);
    return record;
}
int track_main(int kind,const char* message)
{
    /* handle database file update events; these occur when a m_intr message kind
       was received (meaning a signal was sent to the process that we already handled) */
    if (kind == m_intr) {
        /* if 'do_database_update' then the timer alarm has elapsed; we
           should now check the database for changes */
        if (do_database_update) {
            do_database_update = 0;
            /* if the database has changed then write it to disk */
            if (database_updated) {
                database_updated = 0;
                write_journal_file();
            }
        }
    }
    return 0;
}
int track_help(int kind,const char* message,const callback_parameter* params)
{
    issue_command_str("tellraw %s {text:\"journal \",color:\"white\",\
extra:[{text:\"add\",color:\"gray\"},{text:\" | \",color:\"white\"},\
{text:\"tell\",color:\"gray\"},{text:\" | \",color:\"white\"},\
{text:\"up\",color:\"gray\"},{text:\" | \",color:\"white\"},\
{text:\"rm\",color:\"gray\"},{text:\" | \",color:\"white\"},\
{text:\"tp \",color:\"gray\"},{text:\"location-label\",color:\"blue\",underlined:\"true\"}\
]}",params->s_tokens[0]);
    return 0;
}
int track_tell(int kind,const char* message,const callback_parameter* params)
{
    coord* record;
    const char* label[2];
    record = preop_task(label,params->s_tokens[0],params->s_tokens[1]);
    if (record != NULL) {
        issue_command_str("tellraw %s {text:\"%s:%s -> {%c}\",color:\"blue\"}",params->s_tokens[0],label[0],label[1],*record);
        return 0;
    }
    return 1;
}
int track_add(int kind,const char* message,const callback_parameter* params)
{
    coord* record;
    struct tree_key* key;
    const char* label[2];
    if (parse_label(label,params->s_tokens[0],params->s_tokens[1]) != 1) {
        /* attempt to add a default entry using the user:label pair */
        record = add_default_entry(label[0],label[1]);
        if (record != NULL) {
            /* create or update an entry in the teleport record to catch later teleport messages */
            key = tree_lookup(&telerecord,params->s_tokens[0]);
            if (key == NULL)
                tree_insert(&telerecord,params->s_tokens[0],record);
            else
                key->payload = record;
            /* finally, teleport the player to their current location so as to prompt a
               teleport message that indicates their current location */
            issue_command_str("tp %s ~ ~ ~",params->s_tokens[0]);
            issue_command_str("tellraw %s {text:\"Added location to database\",color:\"blue\"}",params->s_tokens[0]);
            database_updated = 1;
            return 0;
        }
        else
            issue_command_str("tellraw %s {text:\"The label %s:%s is already in use\",color:\"red\"}",params->s_tokens[0],label[0],label[1]);
    }
    return 1;
}
int track_up(int kind,const char* message,const callback_parameter* params)
{
    coord* record;
    const char* label[2];
    record = preop_task(label,params->s_tokens[0],params->s_tokens[1]);
    if (record != NULL) {
        struct tree_key* key;
        /* update or create an entry in the teleport record to catch later teleport messages */
        key = tree_lookup(&telerecord,params->s_tokens[0]);
        if (key == NULL)
            tree_insert(&telerecord,params->s_tokens[0],record);
        else
            key->payload = record;
        /* issue the teleport command so when can get the player's location */
        issue_command_str("tp %s ~ ~ ~",params->s_tokens[0]);
        issue_command_str("tellraw %s {text:\"Location updated\",color:\"blue\"}",params->s_tokens[0]);
        database_updated = 1;
        return 0;
    }
    return 1;
}
int track_rm(int kind,const char* message,const callback_parameter* params)
{
    const char* label[2];
    if (parse_label(label,params->s_tokens[0],params->s_tokens[1]) != 1) {
        if (remove_entry(label[0],label[1]) != 1) {
            issue_command_str("tellraw %s {text:\"Location removed from database\",color:\"blue\"}",params->s_tokens[0]);
            database_updated = 1;
            return 0;
        }
        else
            issue_command_str("tellraw %s {text:\"Location does not exist in database\",color:\"red\"}",params->s_tokens[0]);
    }
    return 1;
}
int track_tp(int kind,const char* message,const callback_parameter* params)
{
    coord* record;
    const char* label[2];
    record = preop_task(label,params->s_tokens[0],params->s_tokens[1]);
    if (record != NULL) {
        issue_command_str("tellraw %s {text:\"Teleported to location %s:%s\",color:\"blue\"}",params->s_tokens[0],label[0],label[1]);
        issue_command_str("tp %s %c",params->s_tokens[0],*record);
        return 0;
    }
    return 1;
}
int track_portmsg(int kind,const char* message,const callback_parameter* params)
{
    /* see if there is a record for the player awaiting assignment */
    struct tree_key* key;
    key = tree_lookup(&telerecord,params->s_tokens[0]);
    if (key!=NULL && key->payload!=NULL) {
        /* assign coordinates received from tp result; truncate from float to int */
        coord* record = (coord*)key->payload;
        record->coord_x = (int)params->f_tokens[0];
        record->coord_y = (int)params->f_tokens[1];
        record->coord_z = (int)params->f_tokens[2];
        database_updated = 1;
        /* mark the record as being nil */
        key->payload = NULL;
        return 0;
    }
    return 1;
}
