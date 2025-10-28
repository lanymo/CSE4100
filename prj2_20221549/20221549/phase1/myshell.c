#include "csapp.h"
#include <errno.h>
#define MAXARGS   128

/*prototype*/
void eval(char *Cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char ** argv);



int main(){
    char cmdline[MAXLINE];
    

    while(1){
        printf("CSE4100-SP-P2> ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin)){
            exit(0); 
        }

        eval(cmdline);
    }
}

void eval(char* cmdline){
     
    char *argv[MAXARGS]; //argument list
    char buf[MAXLINE]; //modified command line
    int bg; //background job
    pid_t pid; //Process id


    strcpy(buf, cmdline);
    bg = parseline(buf, argv); //child에 parsing


    if(argv[0] == NULL){
        return; /*emptyline 무시*/
    }

    if(!builtin_command(argv)){ //quit -> exit(0), & -> ignore, other -> run
       
        if((pid = fork()) == 0){ //child run

            if(execvp(argv[0], argv) < 0){ //execve를 수행하면서 run
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
    

        else {
            if(!bg){
                int status;
                if (Waitpid(pid, &status, 0) < 0)
                    unix_error("waitfg: waitpid error");
            }
            else{
                printf("%d %s", pid, cmdline);
            }
        }
    }

    return;
}

int builtin_command(char **argv){
    if(!strcmp(argv[0], "quit")){ /*quit command*/
        exit(0);
    }
    if(!strcmp(argv[0], "&")){ /*Ignore singletom*/
        return 1;
    }
    if(!strcmp(argv[0], "exit")){ /*exit command*/
        exit(0);
    }

    /*User*/
    if(!strcmp(argv[0], "cd")){   
        
        char* path = argv[1] ? argv[1] : getenv("HOME");
        if (path == NULL){
            path = ".";
        }
        if(chdir(path) != 0){
            perror("Error : ");
        }

       return 1;
    }
    return 0; /*Not a builtin command*/
}

int parseline(char *buf, char **argv){

    char *delim;  /*points to first spcae delimiter*/
    int argc; /*number of args*/
    int bg; /*backgrount job*/


    buf[strlen(buf)-1] = ' '; /*replace '\n' with space*/
    while(*buf && (*buf == ' ')){ /*Ignore leading spaces*/
        buf++;
    }

    /*Build the argv list*/
    argc = 0;

    while((delim = strchr(buf, ' '))){

        if(buf[0] == '"'){
            buf++;
            delim = strchr(buf, '"');
            if(!delim) break;
        }

        if(buf[0] == '\''){
            buf++;
            delim = strchr(buf, '\'');
            if(!delim) break;
        }


        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;

        while(*buf && (*buf == ' ')){ /*Ignore spaces*/
            buf++;
        }
    }

    argv[argc] = NULL;

    if(argc == 0){ /*Ignore blank line*/
        return 1;
    }

    /*Should the job run in the background?*/
    if ((bg = (*argv[argc-1] == '&')) != 0){
        argv[--argc] = NULL;
    }

    return bg;
}
