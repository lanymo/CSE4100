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


typedef struct job{
    pid_t pid;
    char state; /*N : nothing, f : foreground, b : background, s : stop, t: terminate, D : Done*/
    char cmd[MAXLINE];
}job; 


//job_list 설정
job jobs[MAXARGS];
int num_jobs = 0;


/*Phase3 jobs*/
void Init_job(){
    for(int i=0; i<MAXARGS; i++){
        jobs[i].pid = 0;
        jobs[i].state = 'N';
        jobs[i].cmd[0] = '\0';
    }
    num_jobs = 0;
}

void Addjob(pid_t pid, char* cmdline, char state){
        if(pid >= 1){ 
            for(int i=0; i<MAXARGS; i++){
                if(jobs[i].pid == 0){
                    jobs[i].pid = pid;
                    jobs[i].state = state;
                    strcpy(jobs[i].cmd, cmdline);
                    num_jobs++;
                    break;
                }
            }
        }
}

void delete_job(pid_t pid) {
    if(pid >= 1){
        for(int i=0; i < MAXARGS; i++){
            if(jobs[i].pid == pid){
                jobs[i].pid = 0;
                jobs[i].state = 'N';
                jobs[i].cmd[0] = '\0';
                num_jobs--;
                break;
            }
        }
    }
}


//Handler
void sigint_handler(int sig){
   for(int i = 0; i < MAXARGS; i++) {
        if(jobs[i].state == 'f' && jobs[i].pid > 0) {
            kill(jobs[i].pid, SIGINT); 
            jobs[i].state = 't'; 
        }
    };
}
void sigchild_handler(int sig){
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAXARGS; i++) {
            if (jobs[i].pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    printf("[%d] Done %s\n", i, jobs[i].cmd); 
                    jobs[i].state = 'D';  // Done
                    delete_job(pid);
                }
            }
        }
    }  
}
void sigstp_handler(int sig){
    for(int i = 0; i < MAXARGS; i++) {
        if(jobs[i].state == 'f' && jobs[i].pid > 0) {
            kill(jobs[i].pid, SIGTSTP); 
            jobs[i].state = 's';
        }
    }
}

pid_t Fpid(){ 
    for(int i=0; i<MAXARGS; i++){
        if(jobs[i].state == 'f'){
            return jobs[i].pid;
        }
    }
    return 0;
}



/*Phase2 pipeline*/
void run_pipeline(char **argv) {
    int num_cmds = pipe_cnt + 1;
    int fds[pipe_cnt][2]; //fds[cmd_num][0] -> read, fds[cmd_num][1] -> write
 
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

    //pipe 생성
    for(int i=0; i < pipe_cnt; i++){
        if(pipe(fds[i]) < 0){
            perror("pipe");
            exit(1);
        }
    }

    pid_t pid;

    //dup을 이용한 pipelining 시행
    for(int i=0; i< num_cmds; i++){

        if((pid=Fork()) == 0){ /*child process*/
            if(i>0){
                dup2(fds[i-1][0], 0); //read, 처음 입력은 stdin
                close(fds[i-1][0]);
            }
            if ( i< pipe_cnt){
                dup2(fds[i][1], 1); //write
                close(fds[i][0]);
            }
            for (int j = 0; j < pipe_cnt; j++) { //사용하지 않는 fd close
                close(fds[j][0]);
                close(fds[j][1]);
            }
            if (execvp(cmd[i][0], cmd[i]) < 0) { //명령어 수행
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
    
    /*signal 추가*/
    Signal(SIGCHLD, sigchild_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigstp_handler);

    Init_job();

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
       
        if((pid = Fork()) == 0){ //child run

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
                delete_job(pid);
            }else{
                Addjob(pid, cmdline, 'b');
                printf("[%d] %s", pid, cmdline);
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

    /*User - PHASE 1*/
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

    /*User - PHASE 3*/
    if(!strcmp(argv[0], "jobs")){
        for(int i=0; i< MAXARGS; i++){
            if(jobs[i].pid != 0 && jobs[i].state != 'N'){
                if(jobs[i].state == 's'){
                    printf("[%d] suspended %s", i, jobs[i].cmd);
                }
            }
            if(jobs[i].state == 'b'){
                printf("[%d] running %s\n", i, jobs[i].cmd);
            }
        }
        return 1;
    }
    if(!strcmp(argv[0], "bg")){
        int job_id = atoi(argv[1] + 1); 
        if (job_id > 0 && job_id <= MAXARGS && jobs[job_id - 1].pid != 0) {
            kill(jobs[job_id - 1].pid, SIGCONT);
            jobs[job_id - 1].state = 'R';  
            printf("[%d] %s &\n", job_id, jobs[job_id - 1].cmd);
        }
        return 1;
    }
    if(!strcmp(argv[0], "fg")){
        int job_id = atoi(argv[1] + 1);  
        if (job_id > 0 && job_id <= MAXARGS && jobs[job_id - 1].pid != 0) {
            kill(jobs[job_id - 1].pid, SIGCONT);
            jobs[job_id - 1].state = 'R'; 
            int status;
            waitpid(jobs[job_id - 1].pid, &status, WUNTRACED); 
            printf("[%d] %s\n", job_id, jobs[job_id - 1].cmd);
        }
        return 1;
    }
    if(!strcmp(argv[0], "kill")){
         int job_id = atoi(argv[1] + 1); 
        if (job_id > 0 && job_id <= MAXARGS && jobs[job_id - 1].pid != 0) {
            kill(jobs[job_id - 1].pid, SIGKILL); 
            printf("Job [%d] killed\n", job_id);
        }
        return 1;
    }

    return 0; /*Not a builtin command*/
}

int parseline(char *buf, char **argv){

    char *delim;  /*points to first spcae delimiter*/
    int argc; /*number of args*/
    int bg; /*backgrount job*/

    /*user for 명령어 전처리 - Phase 2*/
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