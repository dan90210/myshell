/*
 * This code implements a simple shell program
 * It supports the internal shell command "exit",
 * backgrounding processes with "&", input redirection
 * with "<" and output redirection with ">".
 * However, this is not complete.
 * order of opps: paren, background, left to right
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>

extern char **getaline();

/*
 * Handle exit signals from child processes
   https://www.thegeekstuff.com/2012/03/catch-signals-sample-c-code
   ^^^ refer to this when trying to do signals
 */
void sig_handler(int signal) {
        int status;
        int result = wait(&status);

        // printf("Wait returned %d\n", result);

        if (signal == SIGTTOU) {
                printf("recived SIGTTOU signal\n");
        }
}

/*
 * The main shell function
 */
main() {
        int i;
        char **args;
        int result;
        int block;
        int output;
        int input;
        char *output_filename;
        char *input_filename;
        int paren;
        int semiC;
        int _pipe;

        // Set up the signal handler
        sigset(SIGCHLD, sig_handler);

        // Loop forever
        while(1) {

                // Print out the prompt and get the input
                printf("->");
                args = getaline();

                // No input, continue
                if(args[0] == NULL)
                        continue;

                // Check for internal shell commands, such as exit
                if(internal_command(args))
                        continue;

                // Check for an ampersand
                // if there is an ampersand, block = false
                // if there is not an ampersand, block = true
                paren = (parentheses(args) == 1);
                block = (ampersand(args) == 0);
                semiC = (semiColon(args) == 1);
                //_pipe = (findPipe(args) == 1);


                // Check for redirected input
                input = redirect_input(args, &input_filename);

                switch(input) {
                case -1:
                        printf("Syntax error!\n");
                        continue;
                        break;
                case 0:
                        break;
                case 1:
                        printf("Redirecting input from: %s\n", input_filename);
                        break;
                }

                // Check for redirected output
                output = redirect_output(args, &output_filename);

                switch(output) {
                case -1:
                        printf("Syntax error!\n");
                        continue;
                        break;
                case 0:
                        break;
                case 2:
                        printf("Redirecting output, and appending, to: %s\n", output_filename);
                        break;
                case 1:
                        printf("Redirecting output to: %s\n", output_filename);
                        break;

                }

                // Do the command
                do_command(args, block,
                           input, input_filename,
                           output, output_filename, paren, semiC);
        }
}

/*
 * Check for ampersand as the last argument
 */
int ampersand(char **args) {
        int i;

        for(i = 1; args[i] != NULL; i++);

        if(args[i-1][0] == '&') {
                free(args[i-1]);
                args[i-1] = NULL;
                return 1;
        } else {
                return 0;
        }

        return 0;
}

/*
 * Checks for a semicolon
 */
int semiColon(char **args) {
        int i;
        for (i=0; args[i] != NULL; i++) {
                if (args[i][0] == ';') {
                        free(args[i]);
                        args[i] = NULL;
                        return 1;
                }
        }
        return 0;
}

/*
 * Checks for a pipe
 */
int findPipe(char **args) {
        int i;
        for (i=0; args[i] != NULL; i++) {
                if (args[i][0] == '|') {
                        free(args[i]);
                        args[i] = NULL;
                        return 1;
                }
        }
        return 0;
}

int parentheses(char **args) {
        int i, j;
        int openingParen = 0;
        int closingParen = 0;

        for(i = 0; args[i] != NULL; i++) {
                if (args[i][0] == '(') {
                        openingParen = 1;
                        for(j = i+1; args[j] != NULL; j++) {
                                if (args[j][0] == ')') {
                                        closingParen = 1;
                                }
                        }
                }


        }

        if (openingParen == 1 && closingParen == 1) {
                printf("our parentheses works\n");
                return 1;
        } else if (openingParen == 0 && closingParen == 0) {
                return 0;
        } else {
                printf("missmatched parentheses\n");
                return -1;
        }


}

/*
 * Check for internal commands
 * Returns true if there is more to do, false otherwise
 */
int internal_command(char **args) {
        if(strcmp(args[0], "exit") == 0) {
                exit(0);
        }

        return 0;
}

/*
 * Do the command
 */
int do_command(char **args, int block,
               int input, char *input_filename,
               int output, char *output_filename,
               int paren, int semiC) {

        int result;
        pid_t child_id;
        int status;
        int i;
        int index = 0;
        int fd1[2];

        // Fork the child process
        child_id = fork();

        int _pipe = findPipe(args);

        // Check for errors in fork()
        switch(child_id) {
        case EAGAIN:
                perror("Error EAGAIN: ");
                return;
        case ENOMEM:
                perror("Error ENOMEM: ");
                return;
        }

        if(child_id == 0) {

          if (_pipe) {
            close(fd1[1]);
            char pipeInput[255];
            read(fd1[0], pipeInput, 255);
          }

          // Set up redirection in the child process
          if (input)
                  freopen(input_filename, "r", stdin);

          if (output == 1)
                  freopen(output_filename, "w+", stdout);

          if (output == 2)
                  freopen(output_filename, "a+", stdout);

          // Execute the command
          execvp(args[index], args);
          exit(-1);
        }

        // Wait for the child process to complete, if necessary
        // block is true if there is no '&'
        if (block) {

          if (_pipe) {
            close(fd1[0]);
            char pipeOutput[255];
            write(fd1[1], pipeOutput, strlen(pipeOutput)+1);
          }

          // printf("Waiting for child, pid = %d\n", child_id);
          result = waitpid(child_id, &status, 0);
        } else {
          // DOES NOT WORK
          // This is where we need to handle background processes
          printf("backgrounding child, pid = %d\n", child_id);
          result = sigset(child_id, &status, 0);
        }

        // if (semiColon(args))
        // update index
        // shift the contents of arguments
        // ^^^ refer to second for loop in redi

        // if(semiC) {
        //         // wrap all of this in while(semiC) so we can check for more ;
        //         for(i = 0; args[i] != NULL; i++) {
        //                 // find the index of the semiColon
        //                 if (args[i][0] == ';') {
        //                         index = args[i + 1];
        //                 }
        //         }
        // }
}

/*
 * Check for input redirection
 */
int redirect_input(char **args, char **input_filename) {
        int i;
        int j;

        for(i = 0; args[i] != NULL; i++) {

                // Look for the <
                if(args[i][0] == '<') {
                        free(args[i]);

                        // Read the filename
                        if(args[i+1] != NULL) {
                                *input_filename = args[i+1];
                        } else {
                                return -1;
                        }

                        // Adjust the rest of the arguments in the array
                        for(j = i; args[j-1] != NULL; j++) {
                                args[j] = args[j+2];
                        }

                        return 1;
                }
        }

        return 0;
}

/*
 * Check for output redirection
 */
int redirect_output(char **args, char **output_filename) {
        int i;
        int j;
        int k;

        for(i = 0; args[i] != NULL; i++) {

                // Look for the >>
                if(args[i][0] == '>' && args[i+1][0] == '>') {
                        free(args[i]);
                        free(args[i+1]);

                        // Get the filename
                        if(args[i+2] != NULL) {
                                *output_filename = args[i+2];
                        } else {
                                return -1;
                        }

                        // Adjust the rest of the arguments in the array
                        for(j = i; args[j-1] != NULL; j++) {
                                args[j] = args[j+2];
                        }

                        return 2;
                }


                // Look for the >
                if(args[i][0] == '>') {
                        free(args[i]);

                        // Get the filename
                        if(args[i+1] != NULL) {
                                *output_filename = args[i+1];
                        } else {
                                return -1;
                        }

                        // Adjust the rest of the arguments in the array
                        for(j = i; args[j-1] != NULL; j++) {
                                args[j] = args[j+2];
                        }

                        return 1;
                }
        }

        return 0;
}
