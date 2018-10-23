#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

extern char **getaline();

void getPrePipeArgs(char **args, char **prePipeArgs);
void getPostPipeArgs(char **args, char **postPipeArgs);
int do_command(char **args, int block,
               int input, char *input_filename,
               int output, char *output_filename,
			   int inputPipeBool, char *pipe_input_file);
int ampersand(char **args);
int semiColon(char **args);
int getPipe(char **args);
int parentheses(char **args);
int internal_command(char **args);
int redirect_input(char **args, char **input_filename);
int redirect_output(char **args, char **output_filename);

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

		do_command(args, block,
				   input, input_filename,
				   output, output_filename, 0, "file1");
	}
}

/*
 * Do the command
 * args - The list of args to execute
 * block - Whetether or not we are backgrounding the process
 * input - If there is input in the command
 * input_filename - Where we are getting the input from
 * output - If there is output in the command
 * output_filename - Where we are sending the output to
 * inputPipeBool - If this call of do_command has pipe input
 * pipe_input_file - Input from the pipe is in this file
 */
int do_command(char **args, int block,
               int input, char *input_filename,
               int output, char *output_filename, 
			   int inputPipeBool, char *pipe_input_file) {

	pid_t child_id;
	int status;

	char **prePipeArgs = malloc(1000 *sizeof(char*));
	char **postPipeArgs = malloc(1000 *sizeof(char*));
	
	char *pipe_output_file;

	int pipeInArgs = getPipe(args);
	

	// Get pipe info and split args into prePipeArgs and postPipeArgs
	if (pipeInArgs) {
		
		int pipeIndex = getPipeIndex(args);
		int i = 0;
		for (i; i < pipeIndex; i++) {
			prePipeArgs[i] = args[i];
		}
		int j = 0;
		for (i = pipeIndex+1; args[i] != NULL; i++) {
			postPipeArgs[j] = args[i];
			j++;
		}
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
	if(child_id == 0 || (inputPipeBool && (getpid() % 2 == 0))) {
		// This call of do_command has input from a previous pipe
		if (inputPipeBool) {
			// Take input from the file instead of stdin
			int input_file_desc;
			input_file_desc = open(*pipe_input_file, O_RDONLY);
			dup2(input_file_desc, stdin);
		}

		// We have to pipe to another process
		if (pipeInArgs) {
			printf("%s\n", args[0]);
			// We have two files, if we receive input from one, we write to the other
			if (strcmp(*pipe_input_file, "file1")) {
				pipe_output_file = "file2";
			} else {
				pipe_output_file = "file1";
			}
			
			// Set output to go to the file and not to stdout
			int output_file_desc;
			output_file_desc = open(*pipe_output_file, O_WRONLY);
			dup2(output_file_desc, stdout);
			execvp(prePipeArgs[0], prePipeArgs);
		
		// No pipe
		} else {
			printf("%s\n", args[0]);
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
			execvp(args[0], args);
		}
		
	// Runs second
	} else {
		// Wait for input from child
		if (pipeInArgs) {
			waitpid(child_id, &status, 0);
			
			// Execute the command with the input from prePipeArgs
			do_command(postPipeArgs, block,
						input, input_filename,
						output, output_filename, 1, pipe_output_file);
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

/*
 * Check for ampersand as the last argument
 * args - Argument array to check
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
 * args - Argument array to check
 */
int getPipe(char **args) {
	int i;

	for(i = 1; args[i] != NULL; i++) {

		if(args[i][0] == '|') {
			return 1;
		}
	}
	return 0;
}

/**
 * Checks for a valid set of parentheses
 * args Argument array to check
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
 * args - Argument array to check
 */
int internal_command(char **args) {
	if(strcmp(args[0], "exit") == 0) {
		exit(0);
	}
	return 0;
}

/*
 * Check for input redirection
 * args - Argument array to check
 * input_filename - Where we would get input from, if input is true
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

/**
 * Gets the index of the pipe
 * args Array of arguments to parse
 */
int getPipeIndex(char **args) {
	int i;

	for(i = 0; args[i] != NULL; i++) {

		if(args[i][0] == '|') {
			return i;
		}
	}
	return -1;
}

/*
 * Check for output redirection
 * args - Array of arguments to check
 * output_filename Where the output would go, if output is true
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