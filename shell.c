#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#define CMD_SIZE 1024
//  export MYPATH=/usr/local/bin:/usr/bin:/bin:.
int background_processes = 0;
int exists(char* path){
    struct stat buffer;
    return lstat(path, &buffer);
}
char** parse_input(char* input, int* arg_count){
    char ** args = NULL;
    char* piece = strtok (input," ");
    int count = 0;
    while (piece)
    {
        args = realloc(args, sizeof(char*)* (++count));
        if(args == NULL){
            exit(-1);
        }
        args[count-1] = calloc(strlen(piece)+1, sizeof(char));
        strcpy(args[count-1],piece);
        piece = strtok (NULL, " ");
    }

    args = realloc(args, sizeof(char*)*(count+1));
    args[count-1][strlen(args[count-1])-1] = '\0';
    args[count] = NULL;
    *arg_count = count;
    return args;
}
int find_path(char** args, char* cmd){
	char* path = getenv("MYPATH");
	if (path == NULL) {
		path = "/bin:.";
	}
    char* env = calloc(strlen(path)+1, sizeof(char));   
	strcpy(env, path);
    char* piece = strtok (env,":");
	while(piece) {
		char* strpath = calloc(strlen(piece)+strlen(cmd)+2, sizeof(char));
		strcat(strpath, piece);
		strcat(strpath, "/");
		strcat(strpath, cmd);
        if (exists(strpath) == 0)
        {
            free(args[0]);
            args[0] = calloc(strlen(strpath)+1, sizeof(char));
            strcpy(args[0], strpath);
            free(strpath);
            free(env);
            return 1;
        }
        piece = strtok(NULL, ":");
        free(strpath);
	}
    free(env);
	return 0;
}
char* get_cmd(char* path){
    char *last = strrchr(path, '/');
    char * cmd = calloc(strlen(last+1)+1, sizeof(char));
    strcpy(cmd, last+1);
    //printf("CMD: %s\n", cmd);
    return cmd;
}
void exec_path_fore(char** args){
    char* path = calloc(strlen(args[0])+1, sizeof(char));
    char* cmd = get_cmd(args[0]);
    strcpy(path,args[0]);
    strcpy(args[0], cmd);
    pid_t pid = fork();
    int status;
    if(pid == 0){
        if(execv(path, args) < 0){
            fprintf(stderr,"ERROR: Failed to execute\n");
        }
        exit(0);
    }
    free(path);
    free(cmd);
    waitpid(pid,&status,0);
}
void exec_path_back(char** args){
    char *s = calloc(strlen(args[0])+1, sizeof(char));
    strcpy(s, args[0]);
    char *last = strrchr(s, '/');
    printf("[running background process \"%s\"]\n", last+1);
    free(s);
    char* path = calloc(strlen(args[0])+1, sizeof(char));
    char* cmd = get_cmd(args[0]);
    strcpy(path,args[0]);
    strcpy(args[0], cmd);
    pid_t pid = fork();
    
    if(pid == 0){
        if(execv(path, args) < 0){
            fprintf(stderr,"ERROR: Failed to execute\n");
        }
        exit(0);
    }
    free(path);
    free(cmd);
    background_processes++;

}
void freeAll(char* input, char* currpath, char* path, char** args, int arg_count, char* curr_cmd){
    for(int i = 0; i < arg_count; i++){
        //printf("Freeing args[%d]: [%s]\n", i, args[i]);
        free(args[i]);
    }
    free(args);
    free(input);
    free(path);
    free(curr_cmd);
    free(currpath);
}
int has_pipe(char** args, int arg_count){
    for(int i = 0; i < arg_count; i++){
        if(strcmp(args[i], "|")==0){
            return 1;
        }
    }
    return 0;
}

void pipe_cmd(char** cmdA, char** cmdB){
    int p[2];
    char* path = calloc(strlen(cmdA[0])+1, sizeof(char));
    char* cmd = get_cmd(cmdA[0]);
    strcpy(path,cmdA[0]);
    strcpy(cmdA[0], cmd);
    char* path2 = calloc(strlen(cmdB[0])+1, sizeof(char));
    char* cmd2 = get_cmd(cmdB[0]);
    strcpy(path2,cmdB[0]);
    strcpy(cmdB[0], cmd2);
    pipe(p);
    pid_t pidA = fork();
    pid_t pidB;
    int status;
    if(pidA == 0){
        close(p[0]);
        dup2(p[1], 1);
        close(p[1]);
        if(execv(path, cmdA) < 0){
            fprintf(stderr,"ERROR: First command failed\n");
            exit(0);
        }
    }else{
        waitpid(pidA, &status, 0);
        close(p[1]);
        pidB = fork();
        if(pidB == 0){
            dup2(p[0],0);
            close(p[0]);
            if(execv(path2, cmdB) < 0){
                fprintf(stderr,"ERROR: Second command failed\n");
                exit(0);
            }
        }else{
            close(p[0]);

            waitpid(pidB, &status, 0);
            free(path2);
            free(cmd2);
            free(cmd);
            free(path);
            return;
        }
    }

}

void parse_pipe(char** args, int arg_count){
    if(strcmp(args[arg_count-1],"&")== 0){
        free(args[arg_count-1]);
        args[arg_count-1] = NULL;
        arg_count--;
    }
    
    
    int pipe_index;
    for(int i = 0; i < arg_count; i++){
        if(strcmp(args[i], "|")==0){
            pipe_index = i;
        }
    } // 5 - 2 = 3
    //2 params, 1 null
    //arg_count = 6
    //ls -w -f | grep a.out
    //pipe_index = 3
    //
    char** cmdA = calloc(pipe_index+1, sizeof(char*));
    char** cmdB = calloc(arg_count-pipe_index+1, sizeof(char*));
    int i;
    int cmdB_count = 0;
    for(i = 0; i < arg_count; i++){
        if(i < pipe_index){
            cmdA[i] = calloc(strlen(args[i])+1,sizeof(char));
            strcpy(cmdA[i],args[i]);
        }else if(i > pipe_index){
            cmdB[cmdB_count] = calloc(strlen(args[i])+1,sizeof(char));
            strcpy(cmdB[cmdB_count],args[i]);
            cmdB_count++;
        }
    }
    
    cmdA[pipe_index] = NULL;
    cmdB[cmdB_count] = NULL;
    
    if(find_path(cmdB, cmdB[0]) && find_path(cmdA, cmdA[0])){

        // printf("PATH A : %s\n", cmdA[0]);
        // printf("PATH B : %s\n", cmdB[0]);
        pipe_cmd(cmdA, cmdB);
    }

    for(i = 0; i < pipe_index; i++){
            free(cmdA[i]);
    }
    for(i = 0; i < cmdB_count; i++){
            free(cmdB[i]);
    }
    free(cmdA);
    free(cmdB);
}
int main(int argc, char* argv[]){
    //execv - run exec
    //lstat - determine if command exists and if executable
    //chdir - use in parent 
    setvbuf( stdout, NULL, _IONBF, 0 );
    char* input;
    char* currpath;
    char ** args;
    char* curr_cmd;
    int arg_count;
    while(1){
		int count = background_processes;
		for (int i = 0; i < count; i++) {
			int status;
			int pid = waitpid(-1, &status, WNOHANG);
			if (pid > 0) {
				if (WIFEXITED(status)) {
					printf("[process %i terminated with exit status %i]\n", pid, WEXITSTATUS(status));
					background_processes--;
				}
			}
		}
        char* path = NULL;
        currpath = calloc(CMD_SIZE, sizeof(char));
        getcwd(currpath, CMD_SIZE);
        printf("%s$ ", currpath);
        char buff[CMD_SIZE];
        fgets(buff, CMD_SIZE, stdin);
        if(strlen(buff)==1){
            free(currpath);
            free(path);
            continue;
        }
        input = calloc(strlen(buff)+1, sizeof(char));
        strcpy(input, buff);
        args = parse_input(input, &arg_count);
        curr_cmd = calloc(strlen(args[0])+1,sizeof(char));
        strcpy(curr_cmd, args[0]);
        if(strcmp(args[0],"exit") == 0){
            printf("bye\n");
            freeAll(input, currpath, path, args,arg_count,curr_cmd);
            exit(1);
        }
        if(strcmp(args[0], "cd")== 0){
            if(arg_count == 1){
                chdir(getenv("HOME"));
            }else{
                if(chdir(args[1]) < 0){
                    fprintf(stderr, "chdir() failed: Not a directory\n");
                    freeAll(input, currpath, path, args,arg_count,curr_cmd);
                    continue;
                }
            }
            getcwd(currpath, CMD_SIZE);
            freeAll(input, currpath, path, args,arg_count,curr_cmd);
            continue;
        }                           
        if(has_pipe(args,arg_count)){
            parse_pipe(args, arg_count);
            freeAll(input, currpath, path, args, arg_count,curr_cmd);
            continue;
        }
        if(find_path(args, args[0])){
            if(strcmp(args[arg_count-1],"&") == 0){
              //  args[arg_count-1] = NULL;
              //add pipe func, fix ampersand
                free(args[arg_count-1]);
                args[arg_count-1] = NULL;

                exec_path_back(args);
            }else{
                exec_path_fore(args);
            }
            freeAll(input, currpath, path, args,arg_count,curr_cmd);
        }else{

            fprintf(stderr, "ERROR: command \"%s\" not found\n", curr_cmd);
            freeAll(input, currpath, path, args,arg_count,curr_cmd);
        }
        
    }

}