#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

int lexer(char *line, char ***args, int *num_args){
    *num_args = 0;
    // count number of args
    char *l = strdup(line);
    if(l == NULL){
        return -1;
    }
    char *token = strtok(l, " \t\n");
    while(token != NULL){
        (*num_args)++;
        token = strtok(NULL, " \t\n");
    }
    free(l);
    // split line into args
    *args = malloc(sizeof(char **) * *num_args);
    *num_args = 0;
    token = strtok(line, " \t\n");
    while(token != NULL){
        char *token_copy = strdup(token);
        if(token_copy == NULL){
            return -1;
        }
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " \t\n");
    }
    return 0;
}

// Reference: https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
int myPipe(char *line, int num){
    char error_message[30] = "An error has occurred\n";
    int status;
    int i = 0;
    int pid;
    char **args;
    int num_args;
    char *token;
    int pipefds[2*num];
    
    for(i = 0; i < (num); i++){
        if(pipe(pipefds + i * 2) < 0) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 1;
        }
    }


    int j = 0;
    while((token = strsep(&line, "|")) != NULL){
        pid = fork();
        if(pid == 0) {
            if(j < num){
                if(dup2(pipefds[j + 1], 1) < 0){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    return 1;
                }
            }

            if(j != 0){
                if(dup2(pipefds[j - 2], 0) < 0){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    return 1;
                }
            }

            for(i = 0; i < 2 * num; i++){
                close(pipefds[i]);
            }

            lexer(token, &args, &num_args);
            if (num_args < 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                return 1;
            }

            int redirection = 0;
            int position = num_args;
            int saved_stdout;
            int fo;
            for (int i = 0; i < num_args; i++){
                if (!strcmp(args[i],">")){
                    position = i;
                    redirection = 1;
                    if ((num_args - i - 1) != 1){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        return 1;
                    }
                    if ((fo = open(args[i + 1], O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        return 1;
                    }

                    saved_stdout = dup(1);
                    dup2(fo, 1); 
                    close(fo);
                    break;
                }
            }

            char **rc_args = (char **)malloc(position * sizeof(char*));
            for (int i = 0; i < position; i++){
                rc_args[i] = malloc(strlen(args[i]) + 1);
                memcpy(rc_args[i], args[i], strlen(args[i]) + 1);
            }
            rc_args[position] = NULL;

            if (fflush(stdout)){
                    exit(0);
            }
            
            if(execv(rc_args[0], rc_args) < 0 ){
                write(STDERR_FILENO, error_message, strlen(error_message));
                return 1;
            }

            for (int i = 0; i < position; i ++){
                free(rc_args[i]);
            }
            free(rc_args);

            if (redirection){
                redirection = 0;
                dup2(saved_stdout, 1);
                close(saved_stdout);
            }

        } else if(pid < 0){
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 1;
        }
        j+=2;
    }

    for(i = 0; i < 2 * num; i++){
        close(pipefds[i]);
    }

    for(i = 0; i < num + 1; i++)
        waitpid(pid, &status, 0);
    return 0;
}


int checker(int num_args, char *args[], char *line){
    char error_message[30] = "An error has occurred\n";
    int pipe = 0;

    char cmd[128];
    strcpy(cmd, args[0]);

    if (!strcmp(cmd, "exit")){   
        if (num_args > 1){
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 1;
        }
        exit(0);
    } else if(!strcmp(cmd, "cd")){ 
        if (num_args != 2) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 1;
        }

        char *path = args[1];
        if (chdir(path) != 0){
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 1;
        }

    } else if(!strcmp(cmd, "pwd")){
        char buf[128];
        if (getcwd(buf, sizeof(buf)) == NULL){
            write(STDERR_FILENO, error_message, strlen(error_message));
            return 1;
        }

        if (fflush(stdout)){
            exit(0);
        }

        printf("%s\n", buf);
        
    } else if (strstr(cmd, "/bin") == NULL){
        write(STDERR_FILENO, error_message, strlen(error_message));
        return 1;
    } else {
        int count = 0;
        for (int i = 0; i < num_args; i++){
            if (!strcmp(args[i],"|")){
                count ++;
                pipe = 1;
            }
        }

        int work = 1;
        if (pipe){
            work = !myPipe(line, count);
            pipe = 0;
            if (!work){
                write(STDERR_FILENO, error_message, strlen(error_message));
                return 1;
            } 

        } else {
            int redirection = 0;
            int position = num_args;
            int saved_stdout;
            int fo;

            for (int i = 0; i < num_args; i++){
                if (!strcmp(args[i],">")){
                    position = i;
                    redirection = 1;
                    if ((num_args - i - 1) != 1){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        return 1;
                    }
                    if ((fo = open(args[i + 1], O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        return 1;
                    }
                    saved_stdout = dup(1);
                    dup2(fo, 1); 
                    close(fo);
                    break;
                }
            }

            int rc = fork();
            if (rc < 0){
                write(STDERR_FILENO, error_message, strlen(error_message));
            } else if (rc == 0){
                char **rc_args = (char **)malloc(position * sizeof(char*));
                for (int i = 0; i < position; i++){
                    rc_args[i] = malloc(strlen(args[i]) + 1);
                    memcpy(rc_args[i], args[i], strlen(args[i]) + 1);
                }
                rc_args[position] = NULL;

                if (fflush(stdout)){
                    exit(0);
                }

                if (execv(rc_args[0], rc_args) < 0){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    return 1;
                }
                
                for (int i = 0; i < position; i ++){
                    free(rc_args[i]);
                }
                free(rc_args);

            } else {
                int status;
                int wait_rc = waitpid(rc, &status, 0);
                if (wait_rc < 0){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    return 1;
                }
            }

            if (redirection){
                redirection = 0;
                dup2(saved_stdout, 1);
                close(saved_stdout);
            }
        }   
    }
    return 0;
}

int main(int argc, char *argv[]){
    char error_message[30] = "An error has occurred\n";
    char *buffer = NULL;
    size_t bufsize = 256;
    char **args;
    int num_args;
    int multiple = 0;
    int repeat = 0;
    char *token;
    char *cmd;
    char *cp_line;
    char **rc_args;
    int rc_size;
    int error = 0;

    while (1){
        if (fflush(stdout)){
            exit(0);
        }
        write(STDOUT_FILENO, "smash> ", 7);
        

        buffer = (char *)calloc(sizeof(char), bufsize);
        if (buffer == NULL){
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        
        if (getline(&buffer, &bufsize, stdin) == -1){
            exit(0);
        }

        cmd = malloc(bufsize * sizeof(char));
        if (cmd == NULL){
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        strcpy(cmd, buffer);

        if (!strcmp(buffer, "\n")){
            continue;
        }

        for (int i = 0; i < bufsize; i++){
            if (buffer[i] == 59){
                if (i == 0){
                    if (buffer[i + 1] != 32 && buffer[i + 1] != 0 && buffer[i + 1] != 10) {
                        printf("%d", buffer[i + 1]);
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        error = 1;
                        break;
                    }
                } else{
                    if ((buffer[i - 1] != 32 && buffer[i - 1] != 0) || (i + 1) > bufsize || (buffer[i + 1] != 32 && buffer[i + 1] != 0 && buffer[i + 1] != 10)){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        error = 1;
                        break;
                    }
                }
            }
        }

        if (error){
            error = 0;
            continue;
        }
        


        if (lexer(buffer, &args, &num_args)){
            write(STDERR_FILENO, error_message, strlen(error_message));
            continue;
        }

        for (int i = 0; i < num_args; i++){
            if (!strcmp(args[i],";")){
                multiple = 1;
            }
        }

        if (multiple){
            cp_line = malloc(bufsize * sizeof(char));
            if (cp_line == NULL){
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            while((token = strsep(&cmd, ";")) != NULL){
                if (error){
                    break;
                }
                error = 0;
                strcpy(cp_line, token);
                lexer(token, &args, &num_args);
                if (num_args < 1) {
                    continue;
                }

                if (!strcmp(args[0],"loop")){
                    repeat = atoi(args[1]);
                    if (repeat == 0){
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        continue;
                    }

                    rc_size = num_args - 2;
                    rc_args = (char **)malloc(rc_size * sizeof(char*));
                    for (int i = 2; i < num_args; i++){
                        rc_args[i- 2] = malloc(strlen(args[i]) + 1);
                        memcpy(rc_args[i - 2], args[i], strlen(args[i]) + 1);
                    }

                    for (int i = 0; i < repeat; i ++){
                        error = checker(rc_size, rc_args, cp_line);
                        if (error){
                            break;
                        }
                    }
                } else {
                    error = checker(num_args, args, cp_line);
                }
            }
        }
        
        if (!multiple){
            error = 0;
            if (!strcmp(args[0],"loop")){
                repeat = atoi(args[1]);
                if (repeat == 0){
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }
                rc_size = num_args - 2;
                rc_args = (char **)malloc(rc_size * sizeof(char*));
                for (int i = 2; i < num_args; i++){
                    rc_args[i- 2] = malloc(strlen(args[i]) + 1);
                    memcpy(rc_args[i - 2], args[i], strlen(args[i]) + 1);
                }

                for (int i = 0; i < repeat; i ++){
                    error = checker(rc_size, rc_args, cmd);
                    if (error){
                        break;
                    }
                }
            } else {
                error = checker(num_args, args, cmd);
            }
                
        }

        if (repeat){
            for (int i = 0; i < rc_size ; i ++){
                    free(rc_args[i]);
                }
                free(rc_args);
        }
        multiple = 0;
        error = 0;
        repeat = 0;
        free(buffer);
    }
    
    exit(0);
}