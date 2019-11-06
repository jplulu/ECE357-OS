// Junpeng Lu
// OS PS3

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int lastExitStatus = 0;

void processCommand(int argc, char *argv[], int redirCount, char *redirArgs[]) {	
	pid_t pid;
	struct timeval start, end;
	struct rusage ru;
	int status;
	
	if((gettimeofday(&start, NULL)) == -1) {
		fprintf(stderr, "Error: Failed to obtain real time. %s\n", strerror(errno));
		lastExitStatus = -1;
		return;
	}
	
	if(strcmp(argv[0], "cd") == 0) {
		char *newDir;
		if(argv[1])
			newDir = argv[1];
		else
			newDir = getenv("HOME");
		if(chdir(newDir) == -1) {
			fprintf(stderr, "Error: Failed to change working directory to [%s]. %s\n", argv[1], strerror(errno));
			lastExitStatus = -1;
			return;
		}
		return;
	} 
	
	if(strcmp(argv[0], "exit") == 0) {
		int exitStatus;
		errno = 0;
		if(argv[1]) {
			long val = strtol(argv[1], NULL, 10);
			if(errno) {
				fprintf(stderr, "Error: %s is not a valid input. %s\n", argv[1], strerror(errno));
				exit(-1);
			}
			exitStatus = (int)val;
			exit(exitStatus);
		}
		else 
			exit(lastExitStatus);
	}
	
	if((strcmp(argv[0], "pwd") == 0) && redirCount == 0) {
		char cwd[4096];
		if(getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr, "Error. Failed to get current working directory. %s\n", strerror(errno));
			lastExitStatus = -1;
			return;
		}

		fprintf(stdout, "%s\n", cwd);
		return;
	}
	
	switch(pid = fork()) {
		case -1:
			fprintf(stderr, "Error: Fork failed for %s. %s\n", argv[0], strerror(errno));
			lastExitStatus = -1;
			return;
			break;
		
		case 0:
			if(redirCount > 0) {
				int i, fd, redirFlag, dupFd;
				char redirFile[4096];
				char *arg;
				for(i=0; i < redirCount; i++) {
					arg = redirArgs[i];
					if(arg[0] == '<') {
						strcpy(redirFile, (arg + 1));
						redirFlag = O_RDONLY;
						dupFd = 0;
					}
					else if(arg[0] == '>') {
						if(arg[1] == '>') {
							strcpy(redirFile, (arg + 2));
							redirFlag = O_WRONLY | O_APPEND | O_CREAT;
						}
						else {
							strcpy(redirFile, (arg + 1));
							redirFlag = O_WRONLY | O_TRUNC | O_CREAT;
						}
						dupFd = 1;
					}
					else {
						if(arg[2] == '>') {
							strcpy(redirFile, (arg + 3));
							redirFlag = O_WRONLY | O_APPEND | O_CREAT;
						}
						else {
							strcpy(redirFile, (arg + 2));
							redirFlag = O_WRONLY | O_TRUNC | O_CREAT;
						}
						dupFd = 2;
					}

					if((fd = open(redirFile, redirFlag, 0666)) < 0) {
						fprintf(stderr, "Error: Failed to open file [%s]. %s\n", redirFile, strerror(errno));
						lastExitStatus = 1;
						exit(1);
					}
					if(dup2(fd, dupFd) < 0) {
						fprintf(stderr, "Error: Failed to dup file [%s]. %s\n", redirFile, strerror(errno));
						lastExitStatus = 1;
						exit(1);
					}
					if(close(fd) < 0) {
						fprintf(stderr, "Error: Failed to close file [%s]. %s\n", redirFile, strerror(errno));
						lastExitStatus = 1;
						exit(1);
					}
				}
			}
			
			if(execvp(argv[0], argv) == -1) {
					fprintf(stderr, "Error: Failed to execute command [%s]. %s\n", argv[0], strerror(errno));
					lastExitStatus = 127;
					exit(127);
			}
			
			break;
			
		default:
			if(wait3(&status, 0, &ru) == -1) {
				fprintf(stderr, "Error: Failed to wait.\n");
				lastExitStatus = -1;
				return;
			}
			
			if(status != 0) {
				if(WIFSIGNALED(status)) {
					lastExitStatus = WTERMSIG(status);
					fprintf(stderr, "Child process %d exited with signal %d\n", pid, lastExitStatus);
				}
				else {
					lastExitStatus = WEXITSTATUS(status);
					fprintf(stderr, "Child process %d exited with exit status %d\n", pid, lastExitStatus);
				}
			}
			else {
				lastExitStatus = 0;
				fprintf(stderr, "Child process %d exited normally\n", pid);
			}
			
			if((gettimeofday(&end, NULL)) == -1) {
				fprintf(stderr, "Error: Failed to obtain real time. %s\n", strerror(errno));
				lastExitStatus = -1;
				return;
			}

			fprintf(stderr, "Real: %ld.%06ds User: %ld.%06ds Sys: %ld.%06ds\n",
					end.tv_sec - start.tv_sec, end.tv_usec - start.tv_usec,
					ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
					ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
			break;
	}
	return;
}

int main(int argc, char *argv[]) {
	FILE *inFile;
	if(argc > 1) {
		if((inFile = fopen(argv[1], "r")) == NULL) {
			fprintf(stderr, "Error: Failed to open file [%s] for reading. %s\n", argv[1], strerror(errno));
			exit(1);
		}
	}
	else
		inFile = stdin;
	
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	
	while((read = getline(&line, &len, inFile)) != -1) {
	
		char **newArgv = NULL;
		char **redirArgs = NULL;
		int newArgc = 0, redirCount = 0;
	
		if(line[0] == '#')
			continue;
	
		line[strlen(line) - 1] = '\0';	
		char *word = strtok(line, " \t");
		while(word != NULL) {
			if(word[0] == '<' || word[0] == '>' || (word[0] == '2' && word[1] == '>')) {
				redirArgs = realloc(redirArgs, (sizeof(char *) * ++(redirCount)));
				if(redirArgs == NULL) {
					fprintf(stderr, "Error: Failed to reallocate memory.\n");
					redirCount = 0;
					break;
				}
				redirArgs[redirCount - 1] = word;
			}
			else {
				newArgv = realloc(newArgv, (sizeof(char *) * ++(newArgc)));
				if(newArgv == NULL) {
					fprintf(stderr, "Error: Failed to reallocate memory.\n");
					newArgc = 0;
					break;
				}
				newArgv[newArgc - 1] = word;
			}
			
			word = strtok(NULL, " \t");
		}
		
		newArgv = realloc(newArgv, (sizeof(char *) * (newArgc + 1)));
		if(newArgv == NULL) {
			fprintf(stderr, "Error: Failed to reallocate memory.\n");
			newArgc = 0;
			continue;
		}

		newArgv[newArgc] = 0;
		
		if(newArgc > 0)
			processCommand(newArgc, newArgv, redirCount, redirArgs);
		
		free(newArgv);
		free(redirArgs);
		free(line);
		line = NULL;
		len = 0;
	}
	
	if(inFile != stdin)
		fclose(inFile);
		
	exit(lastExitStatus);
}
