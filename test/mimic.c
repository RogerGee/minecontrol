// mimic.c - mimic IO qualities of minecraft server
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4096

void add_time(char* buffer,int* top)
{
    int i;
    time_t t;
    time(&t);
    i = *top;
    strftime(buffer+*top,BUFFER_SIZE-*top,"[%H:%M:%S] ",localtime(&t));
    *top += strlen(buffer+i);
}

void add_type(char* buffer,int* top)
{
    int i = *top;
    sprintf(buffer+*top,"[Server thread/INFO]: ");
    *top += strlen(buffer+i);
}

void generic_message(const char* message)
{
    int top;
    char buffer[BUFFER_SIZE];
    top = 0;
    add_time(buffer,&top);
    add_type(buffer,&top);
    sprintf(buffer+top,"%s",message);
    printf("%s\n",buffer);
}

int main()
{
    int top;
    char output[BUFFER_SIZE];
    char input[BUFFER_SIZE];

    generic_message("Starting minecraft server version 1.7.9");

    while ( gets(input) ) {
        top = 0;
        add_time(output,&top);
        add_type(output,&top);

        if (strcmp(input,"stop") == 0)
            break;
        else if (strlen(input)>4 && strncmp(input,"say",3) == 0) {
            sprintf(output+top,"[Server] %s\n",input+4);
            printf("%s\n",output);
        }
    }

    generic_message("Stopping the server");
}
