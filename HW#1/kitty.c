#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char* argv[]){
	int buffer = 4096, opt, fdout, fdin, amtRead, amtWritten = 0, totalWritten, sysRead, sysWrite, isBinary;
	char* output;
	char* inFile;
	
	output = "";
	char buf[buffer];
	
	while((opt = getopt(argc, argv, "o:")) != -1){
		switch(opt){
			case 'o': 
				output = optarg;
				break;
			case '?':
				exit(-1);
			default:
				fprintf(stderr, "Error. Incorrect format: %s", argv[0]);
				exit(-1);
		}	   
	}	

	if(output != ""){
		fdout = open(output, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if(fdout < 0){
			fprintf(stderr, "Error opening output file <%s> for writing: %s\n",output,strerror(errno));
			return -1;
		}
	} else {
		fdout = STDOUT_FILENO;
		output = "standard output";
	}
	
	if(optind == argc){
		optind = 0; argc = 0;
		argv[argc++] = "-";
	}

	for( ; optind < argc; optind++){
		if(strcmp(argv[optind], "-") == 0){
			fdin = STDIN_FILENO;
		} else if((fdin = open(argv[optind], O_RDONLY)) < 0) {	
			fprintf(stderr, "Error opening input file <%s> for reading:%s\n",argv[optind],strerror(errno));
			return -1;
		}
		
		sysRead = sysWrite = totalWritten = isBinary = 0;
		while((amtRead = read(fdin, buf, (sizeof(char)) * buffer)) != 0){
			if(amtRead < 0){
				fprintf(stderr, "Error reading input file <%s>: %s\n",argv[optind],strerror(errno));
				return -1;
			} else {
				sysRead++;
				if(isBinary == 0){
					int i = 0;
					for(i = 0; i < amtRead; ++i){
						if(!(isprint(buf[i]) || isspace(buf[i]))){
							isBinary = 1;
							fprintf(stderr, "Warning: Attempting to concatenate binary file <%s>\n", argv[optind]);
							break;
						}
					}
				}

				while(amtWritten < amtRead){
					if((amtWritten = write(fdout, buf, amtRead)) < 0){
						fprintf(stderr, "Error writing to output file <%s>: %s\n", output, strerror(errno));
						return -1;
					}						
					amtRead -= amtWritten;
					totalWritten += amtWritten;
					amtWritten = 0;
					sysWrite++;	
				}
			}
		}

		if(fdin == STDIN_FILENO){
			fprintf(stderr, "%d bytes transferred to <%s> from <standard input>. # of read sys call = %d. # of write sys call = %d\n", totalWritten, output, sysRead, sysWrite);
		} else {
			fprintf(stderr, "%d bytes transferred to <%s> from <%s>. # of read sys call = %d. # of write sys call = %d\n", totalWritten, output, argv[optind], sysRead, sysWrite);
		}
		
		if(fdin != STDIN_FILENO){
			if(close(fdin) < 0){
				fprintf(stderr, "Error closing input file <%s>: %s\n", argv[optind], strerror(errno));
				return -1;
			}
		}
									
	}

	if(fdout != STDOUT_FILENO){
		if(close(fdout) < 0){
			fprintf(stderr, "Error closing output file <%s>: %s\n", output, strerror(errno));
		}
	}

	return 0; 		
}






