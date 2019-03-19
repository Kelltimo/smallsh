#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <fcntl.h>

// length limits per assignment instructions
#define BUFFER_LENGTH 2048
#define ARG_LIMIT 512
#define TOK_DELIM " \t\r\n\a"

int foreMode = 0; 				// Cycle to foreground only mode
int backgroundFlag = 0; 		// Cycle background flag
int backCount = 0; 				// Count background PIDs
int status; 					// Exit status for program
int inFlag; 					// Controls whether input flag is set
int outFlag;					// Controls whether output flag is set
char *directory; 				// starting dir	
char *input;					// input file
char *output;					// output file 
int bgPids[200]; 				// array of PIDs in the background 
int runMode = 1; 				// controls program running

// set signal structs empty
struct sigaction SIGINT_action = {0};  
struct sigaction SIGTSTP_action = {0};


// Function to cycle foreground mode 
void cycleFore(int sig) {
		if (!foreMode) {
		// Switch to enter foreground only mode
		char *output = "\nEntering foreground only mode\n";
		write(STDOUT_FILENO, output, strlen(output));
		foreMode = 1;
		fflush(stdout); 
	}

	else {
		// Switch to exit foreground only mode
		char *output = "\nExiting foreground only mode\n";
		write(STDOUT_FILENO, output, strlen(output));
		foreMode = 0;
		fflush(stdout); 
	}
	//  write out prompt 
	char *prompt = ": "; 
	write(STDOUT_FILENO, prompt, strlen(prompt));  

} 

// Remove PIDs 
void removeBackground(int pidInt) {
	int n; 
	// while n is less than our background count
	for(n = 0; n < backCount; n++) {
			// if PID matches
			if(bgPids[n] == pidInt) {
			// remove PID from tracking
			while (n < backCount - 1) {
				bgPids[n] = bgPids[n + 1]; 
				n++; 
			}
			backCount--; 
			break; 
		}
	}
}

// Replace string with another string - sourced from Geeks for Geeks
// https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
char *pidReplace(char *source, const char *searchVal, const char *replaceVal) {
	char *result; 
	int i, cnt = 0; 
	int newWlen = strlen(replaceVal); 
	int oldWlen = strlen(searchVal); 

	// count number of times old word occurs
	for(i = 0; source[i] != '\0'; i++) {
		if(strstr(&source[i], searchVal) == &source[i]) {
			cnt++; 

			// jump to index after old word
			i += oldWlen - 1; 
		}
	}

	// make new string with enough length to store
	result = (char *)malloc(i + cnt * (newWlen - oldWlen) + 1); 

	i = 0; 
	while(*source) {
		// comprae substring with result
		if(strstr(source, searchVal) == source) {
			strcpy(&result[i], replaceVal); 
			i += newWlen; 
			source += oldWlen; 
		} 
		else 
			result[i++] = *source++; 
	}

	result[i] = '\0'; 

	return result; 

} 

// Get line of input and expands any instance of $$ into
// process ID
char* readLine() {
	printf(": "); 
	fflush(stdout); 
	
	// get line
	char *line = NULL; 
	ssize_t buffer = 0;
	getline(&line, &buffer, stdin); 

	// put in pid 
	char *c = strstr(line, "$$"); 
	if (c) {
		char pidArr[6]; 
		sprintf(pidArr, "%d", getpid()); 
		line = pidReplace(line, "$$", pidArr); 
	}
	return line; 
} 

// Parse user input 
char** parseLine(char *line) {
	// list of tokens
	char **tokens = malloc(ARG_LIMIT * sizeof(char*)); 
	char *token; 
	// set flags and pos to NULL 
	inFlag = 0; 
	outFlag = 0; 
	int position = 0; 

	// return a pointer to first token 
	token = strtok(line, TOK_DELIM); 

	// while token isn't zero
	while(token != NULL) {
		// Redirection special character so set input flag
		if(strcmp(token, "<") == 0) {
			inFlag = 1; 
			input = strtok(0, TOK_DELIM); 
			token = strtok(0, TOK_DELIM); 
			tokens[position] = 0; 
			position++; 
			continue; 
		}

		else if(strcmp(token, ">") == 0) {
			// Redirection special character so set output flag
			outFlag = 1; 
			output = strtok(0, TOK_DELIM); 
			token = strtok(0, TOK_DELIM); 
			tokens[position] = 0; 
			position++; 
			continue; 
		}

		else if(strcmp(token, "&") == 0) {
			// decides whether program should be run in 
			tokens[position] = 0; 
			if(!foreMode) {
				// if foreground mode is false, set background flag
				backgroundFlag = 1; 
			} 
			else {
				// else reverse background flag 
				backgroundFlag = 0; 
			} 
			break; 
		} 

		else {
		// save to tokens array we're going to return
		tokens[position] = token; 
		position++; 
		// get next token 
		token = strtok(0, TOK_DELIM); 
		}

	} 
		
	tokens[position] = 0; 
	// return tokens of strings that will be used 
	return tokens; 

}


// start processes utilizing fork system 
int startProcess(char **argArr) {
	pid_t createPid, waitPid; 
	

	// create fork 
	createPid = fork(); 

	if(createPid == 0) {
		// child processes 
		fflush(stdout); 

		// set input 
		if(inFlag) {
			int inFd = open(input, O_RDONLY); 
			if(inFd == -1) {
				printf("Can't open %s for input\n", input); 
				fflush(stdout); 
				exit(1); 
			} 
			// else throw error
			else { 
				if(dup2(inFd, STDIN_FILENO) == -1) {
					perror("dup2"); 
				}
				close(inFd); 
			}
		}

		// set output 
		if(outFlag) {
			int outFd = creat(output, 0755); 
			if (outFd == -1) {
				printf("Can't create %s for output\n", output); 
				fflush(stdout); 
				exit(1); 
			}
			// else throw error
			else {
				if(dup2(outFd, STDOUT_FILENO) == -1) {
					perror("dup2"); 
				}
				close(outFd); 
			}
		}
	
		// Check background flags
		if(backgroundFlag) {
			if(!inFlag) {
				// input to /dev/null read only 
				int inFd = open("/dev/null", O_RDONLY); 
				// throw error 
				if (inFd == -1) {
					printf("Cant set /dev/null to input\n"); 
					fflush(stdout); 
					exit(1); 
				}
				else {
					// throw error 
					if(dup2(inFd, STDOUT_FILENO) == -1) {
					perror("dup2"); 

					}
					close(inFd); 
				}
			}
		
	
		// set output 
		if(!outFlag) {
			// output to /dev/null 
			int outFd = creat("/dev/null", 0755); 
			if(outFd == -1) {
				// throw error
				printf("Cant set /dev/null to output\n"); 
				fflush(stdout); 
				exit(1); 
			}	
			else {
				if(dup2(outFd, STDOUT_FILENO) == -1) {
					perror("dup2"); 
				}
				close(outFd); 
				}
			}
		}

		if(!backgroundFlag) {
			// set SIGINT default 
			SIGINT_action.sa_handler = SIG_DFL; 
			SIGINT_action.sa_flags = 0; 
			sigaction(SIGINT, &SIGINT_action, NULL); 
		}

		if(execvp(argArr[0], argArr)) {
			perror(argArr[0]); 
			exit(1); 
		}
	}

	// if PID less than 0, throw error
	else if(createPid < 0) {
		perror("fork"); 
		exit(1); 
	}

	else {
		if(!backgroundFlag) {
			do {
				// wait until it's completed 
				waitPid = waitpid(createPid, &status, WUNTRACED); 

				// throw error 
				if(waitPid == -1) {
					perror("waitpid"); 
					exit(1); 
				}

				// print out signal that's terminated
				if(WIFSIGNALED(status)) {
					printf("Terminated by signal %d\n", WTERMSIG(status)); 
					fflush(stdout); 
				}

				// print out signal that's stopped by
				if(WIFSTOPPED(status)) {
					printf("Stopped by signal %d\n", WSTOPSIG(status)); 
					fflush(stdout); 
				}
			} while (!WIFEXITED(status) && !WIFSIGNALED(status)); 
		}
		else {
			// else create background PID and add to count, reset flag
			printf("Background pid is %d\n", createPid); 
			fflush(stdout); 
			bgPids[backCount] = createPid; 
			backCount++; 
			backgroundFlag = 0; 
		}
	}
	return 0; 
}

// Change directory 
void changeDir(char** argArr) {
	if(argArr[1] != 0) {
		directory = argArr[1]; 
	} 
	else {
		// home directory
		directory = getenv("HOME"); 
	}
	// invalid directory
	if(chdir(directory) == -1) {
		printf("Invalid directory\n"); 
	}
}

// print to stdout status of last command 
void printStatus(int status) {
	if(WIFEXITED(status)) 
		printf("exit value of %d\n", WEXITSTATUS(status)); 
	else if(WIFSIGNALED(status)) {
		printf("Terminated by signal %d\n", WTERMSIG(status)); 

	} 
}

// Function to execute commands 
int runCommands(char **argArr) {
	if((argArr[0] == NULL) || strchr(argArr[0],'#')) {
		// if user hits enter, give notifcation to user
		printf("# that was a blank line command line, this is comment line\n");
		fflush(stdout);  
	} 
	else if(strcmp(argArr[0], "exit") == 0) {
		// if user hits exit, kill background PIDs and end program
		while(backCount > 0) {
			kill(bgPids[0], SIGTERM); 
			removeBackground(bgPids[0]); 
		}
		runMode = 0; 
	}
	else if(strcmp("cd", argArr[0]) == 0) {
		// chnage directory if called 
		changeDir(argArr); 
	}
	else if(strcmp("status", argArr[0]) == 0) {
		// print status if called 
		printStatus(status); 
	}

	else {
		// else start processes
		startProcess(argArr); 
	}
}

// 
void checkProcesses() {
	// setup pid for back id's
	pid_t bgPid; 
	// setup background status flag 
	int bgStatus; 

	do {
		bgPid = waitpid(-1, &bgStatus, WNOHANG);
		// if background pid greater than 0 we'll notify being done 
		if (bgPid > 0) {
			// print background pid
			printf("Background pid %d is done: ", bgPid);
			removeBackground(bgPid);
			// if terminated by signal 
			if(WIFSIGNALED(bgStatus)) {
				printf("terminated by signal %d\n", WTERMSIG(bgStatus)); 
				fflush(stdout); 
			} 
			// else terminated because it exited 
			else if(WIFEXITED(bgStatus)) {
				printf("exit value %d\n", WEXITSTATUS(bgStatus));
				printf("# the background sleep finally finished\n");  
			}
		} 
	} while (bgPid > 0); 
	
}


// Create prompt loop - reads lines, parse lines, 
// run commands and free memory/reset loop 
void promptLoop() {
	char *line; 
	char **args; 

	while(runMode) {
		if(backCount > 0) {
			checkProcesses(); 
		}

		line = readLine(); 
		args = parseLine(line); 
		runCommands(args); 

		// free memory
		if(line != 0) {
			free(line); 
		}

		if(args != 0) {
			free(args); 
		}

		// reset input/output 
		input = 0; 
		output = 0; 
	}
}  


int main(int argc, char* argv[]) {
			
	// Store directory 
	char *DIR; 
	directory = getenv("PWD"); 


	// SIGINT handling
	SIGINT_action.sa_handler = SIG_IGN; 
	sigaction(SIGINT, &SIGINT_action, NULL); 

	// SIGSTP handling 
	SIGTSTP_action.sa_handler = cycleFore; 
	sigfillset(&SIGTSTP_action.sa_mask);

	SIGTSTP_action.sa_flags = SA_RESTART; 
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);  
	
	
	// call prompt loop to call remainder of functions 
	promptLoop(); 

	chdir(DIR); 


	return 0; 

}

// Sources used 
// https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
// https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
// https://stackoverflow.com/questions/21248840/example-of-waitpid-in-use
// https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
// https://www.gnu.org/software/libc/manual/html_node/Sigaction-Function-Example.html

