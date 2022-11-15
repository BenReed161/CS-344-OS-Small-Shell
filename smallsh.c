/* ******************************
 * Program Name: 
 * By: Benjamin Reed
 * ONID: 933 889 437
 * Email: reedbe@oregonstate.edu
 * Desc: Smallsh is a recreation of the unix sh shell.
 * It's main goal is to demonstrate the use of child processes and their relation to parent processes
 * The program continually prompts the user for input like a unix shell does, and executes the commands
 * that the user enters using the exec suite of functions.
 ****************************** */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

/* ******************************
 * Struct: command
 ****************************** */ 
struct command{
    //args 512 allowed arguments aech being 2048 long
    char args[2048][512];
    //telling the program whether a & has been inputted 
    int wait;
    //number of arguments specified by command
    int arg_nums;
    //the name of the output file if there is one
    char *ofile;
    //the name of the input file is there is one
    char *ifile;
    //whether there's an output file or not
    int o;
    //whether there's an input file or not
    int i;
    //number of redirections present in the command
    int redir_num;
    //which type of redirection either i or o comes first
    int first_redir;
    //whether to ignore the & at the end of the command
    int ignore;
};

//Globals: 
struct command user_cmd;
int status;

/* ******************************
 * Function Name: catch_sigtstp
 * Input: int signo - the signal type sent to the function
 * Output: NA
 * Desc: catch_sigtstp is the handler function for the sigaction struct. 
 * The function catches the signal being sent, in this case it's SIGTSTP, and instead
 * prints out a message "toggling" between the foreground mode. Signal handlers
 * functions can only run async safe functions, therefore I use the write function
 * instead of the printf, printf which is a non-async funcion. 
 ****************************** */ 
void catch_sigtstp(int signo){
    if(user_cmd.ignore == 0){
        char* message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(1, message, 50);
        fflush(stdout);
        user_cmd.ignore = 1;
    }
    else{
        char* message = "\nExiting foreground-only mode\n";
        write(1, message, 30);
        fflush(stdout);
        user_cmd.ignore = 0;
    }
}

/* ******************************
 * Function Name: check_status
 * Input: int child_exit
 * Output: NA
 * Desc: check_status function takes the exit value given when the child process
 * exits and then uses the macro queries WIFEXITED, as well as the macros WEXITSTATUS
 * and WTERMSIG to interpret the child exit status and print that to the smallsh. WIFEXITED
 * determines if the child process exited in a normal way. If it did interpret the normal exit
 * status and print it. Else, tell the user the process terminated and provide the termination status
 ****************************** */ 
void check_status(int child_exit){
    if(WIFEXITED(child_exit)){
        printf("exit value %d\n", WEXITSTATUS(child_exit));
    }
    else{
        printf("terminated by signal %d\n", WTERMSIG(child_exit));
    }
}

/* ******************************
 * Function Name: convert_string
 * Input: char * cmd
 * Output: int j
 * Desc: Function that gets the command entered by the user and places each arguement
 * of the command into a seperate index of the user_cmd struct. It then returns the number of 
 * commands entered by the user.
 ****************************** */ 
int convert_string(char* cmd){
    int j = 0;
    int x = 0;
    unsigned char *buffer;
    for(int i = 0; i < strlen(cmd); i++){
        if(cmd[i] != 32){
            strncat(&user_cmd.args[j][x], &cmd[i], 1);
            x++;
        }
        else{
            j++;
            x=0;
        }
    }
    return ++j;
}

/* ******************************
 * Function Name: run_in_back
 * Input: NA
 * Output: NA
 * Desc: function that checks the last arguement for the & and sets the wait argument to 1
 * it also removes that argument from the overall command and takes one off of the command number.
 ****************************** */ 
void run_in_back(){
    if(user_cmd.args[user_cmd.arg_nums-1][0] == '&'){
        //printf("wait found.\n");
        user_cmd.wait = 1;
        user_cmd.args[user_cmd.arg_nums-1][0] = '\0';
        user_cmd.arg_nums-=1;
    }
}

/* ******************************
 * Function Name: pid_expansion
 * Input: NA
 * Output: NA
 * Desc: Function that takes the command arguements checks for a $$ and replaces it with the pid
 ****************************** */ 
int pid_expansion(){
    int exp_num = 0;
    char buffer[256];
    char pid_buffer[15];
    int numOfPID = 0;
    for(int i = 0; i < user_cmd.arg_nums; i++){
        for(int j = 0; j < strlen(user_cmd.args[i]); j++){
            if(user_cmd.args[i][j] == '$'){
                exp_num++;
                if(exp_num % 2 == 0){
                    numOfPID++;
                    for(int x = j+1; x < strlen(user_cmd.args[i]); x++){
                        strncat(buffer, &user_cmd.args[i][x], 1);
                        user_cmd.args[i][x] == '\0';
                    }
                    user_cmd.args[i][j-1] = '\0';
                    user_cmd.args[i][j] = '\0';
                    sprintf(pid_buffer, "%d", getpid());
                    strcat(user_cmd.args[i], pid_buffer);
                    strcat(user_cmd.args[i], buffer);
                    buffer[0] = '\0';
                }
            }
            else{
                exp_num=0;
            }
        }
    }
}

/* ******************************
 * Function Name: redirection
 * Input: NA
 * Output: NA
 * Desc: function that handles the checking of redirection of the files with commands the '<' and '>'.
 * If the command contains a < or > it will set those variables in the struct telling the program if there are any occurances of
 * < and >. It will also find the specified input of output file.
 ****************************** */ 
void redirection(){
    int i;
    int j;

    for(i = 0; i < user_cmd.arg_nums-1; i++){
        //INPUT
        if(user_cmd.args[i][0] == '<'){
            user_cmd.i = 1;
            user_cmd.first_redir = 0;
            //printf("input present.\n");
            if(user_cmd.args[i+1] != NULL){
                user_cmd.ifile = user_cmd.args[i+1];
                //return;
            }
            /*
            else{
                return;
            }
            */
        }
        //OUTPUT
        if(user_cmd.args[i][0] == '>'){
            user_cmd.o = 1;
            user_cmd.first_redir = 1;
            //printf("output present.\n");
            if(user_cmd.args[i+1] != NULL){
                user_cmd.ofile = user_cmd.args[i+1];
                //return;
            }
            /*
            else{
                return;
            }
            */
        }
    }
}

/* ******************************
 * Function Name: execute_cmd
 * Input: struct sigaction struct
 * Output: NA
 * Desc: main function that handles the execution of the unix commands. The function forks the program
 * here and then uses the execvp function to run the specified unix command. If & is included in the command
 * the program will continue and run in the background, and print the pid when the program is finished.
 * This program also handles all the writing and reading of input and output files. If there are any errors in the
 * function then it will also return an message and exit the process to continue in the parent.
 ****************************** */ 
void execute_cmd(struct sigaction sa){  
    FILE * fp;
    int result;
    
    char *tmp[2] = {"", NULL};
    char *tmp2[512];
    for(int i = 0; i < 511; i++){
        tmp2[i] = 0;
    }
    for(int i = 0; i < user_cmd.arg_nums; i++){
        tmp2[i] = user_cmd.args[i];
    }
    pid_t pid;
    pid = fork();

    //if there is an error forking
    if(pid == -1){
        printf("ERROR FORKING.");
        exit(EXIT_FAILURE);
    }
    //else there is no error in forking
    else if(pid == 0){
        //Allows the SIG_INT handler to be set back to the default, which implies it will handle any sigint given to it whilst in foreground mode
        sa.sa_handler = SIG_DFL;
        sigaction(SIGINT, &sa, NULL);
        // Input and Output redirection, if both are present
        if(user_cmd.o == 1 && user_cmd.i == 1){
            if(user_cmd.first_redir == 0){
                freopen(user_cmd.ofile, "w+", stdout);
                tmp[0] = user_cmd.args[0];
                if(freopen(user_cmd.ifile, "r", stdin) == NULL){
                    printf("Error : the file / directory \"%s\" does not exist\n", user_cmd.ifile);
                    exit(-1);
                }
                execvp(tmp[0], tmp);
                exit(0);
            }
            if(user_cmd.first_redir == 1){
                if(freopen(user_cmd.ifile, "r", stdin) == NULL){
                    printf("Error : the file / directory \"%s\" does not exist\n", user_cmd.ifile);
                    exit(-1);
                }
                tmp[0] = user_cmd.args[0];
                freopen(user_cmd.ofile, "w+", stdout);
                execvp(tmp[0], tmp);
                exit(0);
            }
        }
        // < if only input is present 
        if(user_cmd.i == 1){
            if(freopen(user_cmd.ifile, "r", stdin) == NULL){
                printf("Error : the file / directory \"%s\" does not exist\n", user_cmd.ifile);
                exit(-1);
            }
            tmp[0] = user_cmd.args[0];
            execvp(tmp[0], tmp);
            //cmd_status = 0;
            exit(0);
        }
        // > if only output is preseent
        if(user_cmd.o == 1){
            freopen(user_cmd.ofile, "w+", stdout);
            tmp[0] = user_cmd.args[0];
            execvp(tmp[0], tmp);
            //cmd_status = 0;
            exit(0);
        }
        fflush(NULL);
        result = execvp(tmp2[0], tmp2);
        if(result < 0){
            printf("Error : command could not be executed\n");
            exit(1);
        }
        exit(0);
    }
    if(user_cmd.wait == 1){
        printf("background pid is %d\n", pid);
    }
    if(user_cmd.wait == 0 || user_cmd.ignore == 1){
        result = waitpid(pid, &status, 0);
    }
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		printf("background pid %d is done: ", pid);
        check_status(status);
		fflush(stdout);
	}
}

/* ******************************
 * Function Name: execute_custom_cmd
 * Input: NA
 * Output: int that tells whether the command was executed or not.
 * Desc: executes the commands that we created instead of being handled by the execvp.
 ****************************** */ 
int execute_custom_cmd(){
    if(strstr(user_cmd.args[0], "exit") != NULL){
        exit(EXIT_SUCCESS);
        return 0;
    }
    if(strstr(user_cmd.args[0], "status") != NULL){
        check_status(status);
        return 0;
    }
    if(strstr(user_cmd.args[0], "cd") != NULL){
        if(chdir(user_cmd.args[1]) == 0){
            return 0;
        }
        else{
            chdir(getenv("HOME"));
            return 0;
        }
        printf("Error : the file / directory \"%s\" does not exist\n", user_cmd.args[1]);
    }
    return 1;
}

/* ******************************
 * Function Name: reset_command
 * Input: NA
 * Output: NA
 * Desc: simple function that resets the user_cmd struct
 ****************************** */ 
void reset_command(){
    for(int i = 0; i < user_cmd.arg_nums; i++){
        user_cmd.args[i][0] = '\0';
    }
    user_cmd.arg_nums = 0;
    user_cmd.i = 0;
    user_cmd.o = 0;
    user_cmd.ifile = NULL;
    user_cmd.ofile = NULL;
    user_cmd.wait = 0;
}

/* ******************************
 * Function Name: main
 * Input: NA
 * Output: int
 * Desc: Handles the main loop of the program and prompts the user for input. It also creates the
 * sigaction structs used for signal handling. 
 ****************************** */ 
int main(){
    //closes the standard error stream so that I can handle the errors instead of printing unix errors
    //also prevents unnessisary output by the program.
    fclose(stderr);
    //signal handler for the sigint signal
    struct sigaction sa_sigint = {0};
    //tells the parent process to ignore any signal sigint it recieves
    sa_sigint.sa_handler = SIG_IGN;
    sigfillset(&sa_sigint.sa_mask);
    sa_sigint.sa_flags = 0;
    sigaction(SIGINT, &sa_sigint, NULL);

    struct sigaction sa_sigtstp = {0};
    //tells the parent process to redirect any sigtstp to the handler function I made
    //so that the correct output can be printed.
    sa_sigtstp.sa_handler = catch_sigtstp;
    sigfillset(&sa_sigtstp.sa_mask);
    sa_sigtstp.sa_flags = 0;
    sigaction(SIGTSTP, &sa_sigtstp, NULL);
    
    size_t cmd_size = 2048;
    size_t arg_size = 512;
    char *command;
    int ret = 1;
    int cmd_return;
    int current_out;
    int fd;
    while(1){
        reset_command();
        command = (char *)malloc(cmd_size * sizeof(char));
        printf(": ");
        fflush(stdout);
        getline(&command, &cmd_size, stdin);
        user_cmd.arg_nums = convert_string(command);

        for(int i = 0; i < user_cmd.arg_nums; i++){
            user_cmd.args[i][strcspn(user_cmd.args[i], "\n")] = 0;
        }
        pid_expansion();
        if(user_cmd.args[0][0] == 35 || user_cmd.args[0][0] == '\0'){
            free(command);
            continue;
        }
            run_in_back();
            redirection();
            cmd_return = execute_custom_cmd();
            if(cmd_return == 1)
                execute_cmd(sa_sigint);
        
        
        free(command);
    }
}