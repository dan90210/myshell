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

#define FILENAME "files"

extern char **getaline();

void getPrePipeArgs(char **args, char **prePipeArgs);
void getPostPipeArgs(char **args, char **postPipeArgs);
int do_command(char **args, int block,
               int input, char *input_filename,
               int output, char *output_filename,
			         int inputPipeBool );
int ampersand(char **args);
int semiColon(char **args);
int getPipe(char **args);
int parentheses(char **args);
int internal_command(char **args);
int redirect_input(char **args, char **input_filename);
int redirect_output(char **args, char **output_filename);

/*
 * Handle exit signals from child processes
   https://www.thegeekstuff.com/2012/03/catch-signals-sample-c-code
   ^^^ refer to this when trying to do signals
 */
void sig_handler(int signal) {
        int status;
        int result = wait(&status);

        if (signal == SIGTTOU) {}
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
		block = (ampersand(args) == 0);

		// Check for parentheses
		paren = (parentheses(args) == 1);

		// Check for redirected input
		input = redirect_input(args, &input_filename);

		switch(input) {
		case -1:
			continue;
			break;
		case 0:
			break;
		case 1:
			break;
		}

		// Check for redirected output
		output = redirect_output(args, &output_filename);

		switch(output) {
		case -1:
			continue;
			break;
		case 0:
			break;
		case 2:
			break;
		case 1:
			break;
		}

		// Do the command
		do_command(args, block,
				   input, input_filename,
				   output, output_filename, 0);
	}
}

/*
 * Do the command
 */
int do_command(char **args, int block,
               int input, char *input_filename,
               int output, char *output_filename, int inputPipeBool) {

	pid_t child_id;
	int status;
	int fd1[2];

  char *prePipeArgs;
  char *postPipeArgs;

  // og pre and post args
  // char **prePipeArgs;
  // char **postPipeArgs;

  int i = 0;
  printf("args ====== %s\n", &args);




	// We have pipe
	int pipeInArgs = getPipe(args);

	// Get pipe info
	if (pipeInArgs) {

    // this is what i'm trying rather than doing the separate methods
    // i truly think args is our problem and i have no idea how to solve it

    char *separator = "|";
    prePipeArgs = strtok(*args, separator);
    printf("%s\n", prePipeArgs);
    postPipeArgs = strtok(NULL, "");
    printf("%s\n", postPipeArgs);

    // // og method calls
    //
    // getPrePipeArgs(args, prePipeArgs);
    // getPostPipeArgs(args, postPipeArgs);



	}

	// Fork the child process
	child_id = fork();

	// Check for errors in fork()
	switch(child_id) {
	case EAGAIN:
			return;
	case ENOMEM:
			return;
	}

	// Runs first
	if(child_id == 0) {

		// This call of do_command has input from a previous pipe
		if (inputPipeBool) {
			// Take input from the file instead of stdin
			dup2(FILENAME, stdin);
		}

		// We have to pipe to another process
		if (pipeInArgs) {

			// We have to open the file and read to the end to get the size
			FILE *fp = fopen(FILENAME, "r");
			int size;

			if (fp) {
				// Get size of file
				fseek(fp, 0, SEEK_END);
				int size = ftell(fp);
				close(fp);
			}

			// Here we make another thread
			// Child - Execute our process
			// Parent - Wait for process to finish and return 0 so parent process can continue
			pid_t exec_id;
			int exec_status;

			exec_id = fork();
			if (exec_id == 0) {
				execvp(prePipeArgs[0], prePipeArgs);
				exit(0);
			}
			else {
				waitpid(exec_id, &exec_status, 0);
				write(fd1[1], FILENAME, size);
				close(fd1[1]);
				exit(0);
			}

		} else {

			// Set up redirection in the child process
			if (input) {
				freopen(input_filename, "r", stdin);
			}
			if (output == 1) {
				freopen(output_filename, "w+", stdout);
			}
			if (output == 2) {
				freopen(output_filename, "a+", stdout);
			}

			// Execute the command
			// Execute the command
			execvp(args[0], args);
			fclose (stdout);
			fclose(stdin);
			exit(0);
		}
	// Runs second
	} else {
		// Wait for input from child
		if (pipeInArgs) {
			waitpid(child_id, &status, 0);
			FILE *fp = fopen(FILENAME, "r");
			int size;

			if (fp) {
				// Get size of file
				fseek(fp, 0, SEEK_END);
				size = ftell(fp);
				fclose(fp);
			}

			read(fd1[0], FILENAME, size);
			do_command(postPipeArgs, block,
						input, input_filename,
						output, output_filename, 1);
		} else {
			// Wait for the child process to complete, if necessary
			// block is true if there is no '&'

			if (block) {
				waitpid(child_id, &status, 0);
			} else {
				// DOES NOT WORK
				// This is where we need to handle background processes
				// printf("backgrounding child, pid = %d\n", child_id);
				sigset(child_id, &status, 0);
			}
		}
	}
}

/**
 * Check here for errors
 */
void getPrePipeArgs(char **args, char **prePipeArgs) {
	int i = 0;

	char *token = strtok(*args, "|");
  // printf("%s\n", token);
  prePipeArgs = token;
}

/**
 * Check here for errors
 */
void getPostPipeArgs(char **args, char **postPipeArgs) {
	int i, j;
	while(!strcmp(args[i], "|")) {
		i = i+1;
	}
	j = i;
	while (args[j] != NULL) {
		j = j+1;
	}
	memcpy(postPipeArgs, args + i+2, (j-i)*sizeof(char*));
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
 * Checks for a pipe
 */
int getPipe(char **args) {
	int i = 0;
	while (args[i] != NULL) {
		if (args[i][0] == '|'){
			return 1;
		}
		i = i+1;
	}
	return 0;
}

/**
 * Checks for a valid set of parentheses
 * -1 - Invalid set of parentheses
 * 0 - No parentheses
 * 1 - Valid set of parentheses
 */
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
	// If we have a matching pair
	if (openingParen == closingParen) {
		return openingParen;
	}
	// Invlid pair
	else {
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
