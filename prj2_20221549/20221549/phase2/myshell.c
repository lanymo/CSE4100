#include "csapp.h"
#include<errno.h>
#define MAXARGS   128

/*prototype*/
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char ** argv);

/*user*/
int pipe_flag = 0;
int pipe_cnt = 0;


void run_pipeline(char **argv) {
    int num_cmds = pipe_cnt + 1;
    int fds[pipe_cnt][2]; //fds[cmd_num][0] -> read, fds[cmd_num][1] -> write

    for(int i=0; i < pipe_cnt; i++){
        if(pipe(fds[i]) < 0){
            perror("pipe");
            exit(1);
        }
    }

 
    //명령어를 순서에 따라 분배
    char*** cmd = (char***)malloc(sizeof(char**) * (num_cmds));

    for(int i=0; i< num_cmds; i++){
        cmd[i] = (char**)malloc(sizeof(char*) * MAXARGS); //명령어 옵션 저장
        for(int j=0; j<MAXARGS; j++){
            cmd[i][j] = (char*)malloc(sizeof(char) * MAXLINE); //명령어 저장
        }
    }

    int cur_cmd = 0;
    int cmd_len = 0;

    for(int i=0; argv[i] != NULL; i++){
        if(!strcmp(argv[i], "|")){
            cmd[cur_cmd][cmd_len] = NULL;
            cur_cmd++;
            cmd_len = 0;
        }
        else{
            strcpy(cmd[cur_cmd][cmd_len], argv[i]);
            cmd_len++;
        }
    }
    cmd[cur_cmd][cmd_len] = NULL;


    pid_t pid;

    //dup을 이용한 pipelining 시행
    for(int i=0; i< num_cmds; i++){

        if((pid=fork()) == 0){ /*child process*/
            if(i>0){
                dup2(fds[i-1][0], 0); //read, 처음 입력은 stdin
                close(fds[i-1][0]);
            }
            if ( i< pipe_cnt){
                dup2(fds[i][1], 1); //write
                close(fds[i][0]);
            }
            for (int j = 0; j < pipe_cnt; j++) {
                close(fds[j][0]);
                close(fds[j][1]);
            }
            if (execvp(cmd[i][0], cmd[i]) < 0) {
                fprintf(stderr, "Execvp error: command %s\n", cmd[i][0]);
                exit(1);
            }
        }
        else if(pid<0){ 
            perror("fork");
            exit(1);
        }
    }

    for(int i=0; i<pipe_cnt; i++){
        close(fds[i][0]);
        close(fds[i][1]);
    }
    

    while(wait(NULL) >0);


    
    //메모리 해제
    for (int i = 0; i < num_cmds; i++) {
        for (int j = 0; cmd[i][j] != NULL; j++) {
            free(cmd[i][j]);
        }
        free(cmd[i]);
    }
    free(cmd);
    

}


int main(){
    char cmdline[MAXLINE];
    

    while(1){
        printf("CSE4100-SP-P2> ");
        fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin)){
            exit(0); 
        }

        /*pipe 초기화*/
        pipe_flag = 0;
        pipe_cnt = 0;

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

    if(pipe_flag){
        run_pipeline(argv);
    }
    else{
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
                if (waitpid(pid, &status, 0) < 0)
                    unix_error("waitfg: waitpid error");
            }else{
                printf("%d %s", pid, cmdline);
            }
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

    /*user for 명령어 전처리*/
    char newbuf[MAXLINE]; 
    memset(newbuf,0,sizeof(newbuf));
    int idx = 0;

    buf[strlen(buf)-1] = ' '; /*replace '\n' with space*/
    while(*buf && (*buf == ' ')){ /*Ignore leading spaces*/
        buf++;
    }

    for(int i=0; i<strlen(buf); i++){
        if (buf[i] == '"' || buf[i] == '\''){ //따옴표 제거
            char again = buf[i];
            while(buf[++i] != again){
                newbuf[idx++] = buf[i];
            }
        }
        else if(buf[i] == '|'){ //pipeline 개수 세기 + 공백 추가
            pipe_cnt++;
            pipe_flag=1;
            newbuf[idx++] = ' ';
            newbuf[idx++] = '|';
            newbuf[idx++] = ' ';
        }
        else{
            newbuf[idx++] = buf[i];
        }
    }

    newbuf[idx] = '\0';
    strcpy(buf,newbuf);


    /*Build the argv list*/
    argc = 0;

    while((delim = strchr(buf, ' '))){
        /* PHASE 1에서 사용했지만, 명령어 전처리로 삭제
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
        */

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