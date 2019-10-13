#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

int mtime = 0, volume_flag = 0, user_flag = 0;

void get_mode(char* mode, struct stat* stats){
	char type = '-';
	if(S_ISREG(stats->st_mode))
		type = '-';
	else if(S_ISDIR(stats->st_mode))
		type = 'd';
	else if(S_ISCHR(stats->st_mode))
		type = 'c';	
	else if(S_ISBLK(stats->st_mode))
		type = 'b';
	else if(S_ISFIFO(stats->st_mode))
		type = 'p';
	else if(S_ISLNK(stats->st_mode))
		type = 'l';
	else if(S_ISSOCK(stats->st_mode))
		type = 's';
	
	mode[0] = type;
	mode[1] = (stats->st_mode & S_IRUSR) ? 'r' : '-';
	mode[2] = (stats->st_mode & S_IWUSR) ? 'w' : '-';
	mode[3] = (stats->st_mode & S_IXUSR) ? 'x' : '-';
	mode[4] = (stats->st_mode & S_IRGRP) ? 'r' : '-';
	mode[5] = (stats->st_mode & S_IWGRP) ? 'w' : '-';
	mode[6] = (stats->st_mode & S_IXGRP) ? 'x' : '-';
	mode[7] = (stats->st_mode & S_IROTH) ? 'r' : '-';
	mode[8] = (stats->st_mode & S_IWOTH) ? 'w' : '-';
	mode[9] = (stats->st_mode & S_IXOTH) ? 'x' : '-';
	mode[10] = '\0'; 
	
	if(stats->st_mode & S_ISUID)
		mode[3] = (stats->st_mode & 0100) ? 's' : 'S';
	if(stats->st_mode & S_ISGID)
		mode[6] = (stats->st_mode & 0010) ? 's' : 'l';
	if(stats->st_mode & S_ISVTX)
		mode[9] = (stats->st_mode & 0100) ? 't' : 'T';

	return;
}

void printStats(char* path_name, struct stat* stats, char* time_stamp){
	char permissions[11];
	char sym[4096];
	get_mode(permissions, stats);
	fprintf(stdout, "%d\t%d  %s %d  ", stats->st_ino, (stats->st_blocks)/2, permissions, stats->st_nlink);

	if(getpwuid(stats->st_uid))
		fprintf(stdout, "%s\t", getpwuid(stats->st_uid)->pw_name);
	else
		fprintf(stdout, "%d\t", stats->st_uid);
		
	if(getgrgid(stats->st_gid))
		fprintf(stdout, "%s\t", getgrgid(stats->st_gid)->gr_name);
	else
		fprintf(stdout, "%d\t", stats->st_gid);
	
	if(S_ISBLK(stats->st_mode) || S_ISCHR(stats->st_mode))
		fprintf(stdout, "%d,%d ", major(stats->st_dev), minor(stats->st_dev));
	else
		fprintf(stdout, "%d ", stats->st_size);
	
	fprintf(stdout, "%s %s ", time_stamp, path_name);
	
	if(S_ISLNK(stats->st_mode)){
		int i;
		if((i = readlink(path_name, sym, sizeof(sym)-1)) == -1)
			fprintf(stderr, "ERROR: Failed to read the value of a sym link %s: %s\n", path_name, strerror(errno));
		else{
			sym[i] = '\0';
			fprintf(stdout, "->%s", sym);
		}	
	}
	fprintf(stdout, "\n");
	return;
}

void traverse(int initial_pass, char* node, time_t cur_time, dev_t cur_dev, char* user_name){
	DIR* dirp;
	struct dirent* dp;
	struct stat stats;
	struct tm* tim;
	char path_name[4096];
	char time_stamp[256];
	
	if((dirp = opendir(node)) == NULL){
		if(errno == EACCES){
			fprintf(stderr, "ERROR: Failed to open %s: %s\n", node, strerror(errno));
			return;
		}
		fprintf(stderr, "ERROR: Failed to open %s: %s\n", node, strerror(errno));
		exit(-1);			
	}
	
	errno = 0;
	while((dp = readdir(dirp)) != NULL){
		if(strcmp(dp->d_name, ".") == 0 && !initial_pass)
			continue;
		if(strcmp(dp->d_name, "..") == 0)
			continue;
		
		path_name[0] = '\0';
		if(strcmp(dp->d_name, ".") == 0)
			strncat(path_name, node, (sizeof(path_name) + sizeof(node)));
		else{
			strncat(path_name, node, (sizeof(path_name) + sizeof(node)));
			strncat(path_name, "/", (sizeof(path_name) + sizeof("/")));
			strncat(path_name, dp->d_name, (sizeof(path_name) + sizeof(dp->d_name)));
		}
		
		if((lstat(path_name, &stats)) == -1){
			fprintf(stderr, "ERROR: Cannot get stat for %s: %s\n", path_name, strerror(errno));
			continue;
		}
		
		if(user_flag){
			int user_match = 0, group_match = 0, read_perm = 1;
			if(strcmp(user_name, getpwuid(stats.st_uid)->pw_name) == 0){
				user_match = 1;
				if(!(stats.st_mode & S_IRUSR))
					read_perm = 0;
			}
			
			if(!user_match){
				struct passwd* pw;
				gid_t *groups;
				int ngroups, j;
				pw = getpwnam(user_name);
				if(pw){
					getgrouplist(user_name, pw->pw_gid, groups, &ngroups);
					groups = malloc(ngroups * sizeof(gid_t));
					getgrouplist(user_name, pw->pw_gid, groups, &ngroups);
					for(j = 0; j < ngroups; j++){
						if(groups[j] == stats.st_gid){
							group_match = 1;
							if(!(stats.st_mode & S_IRGRP))
								read_perm = 0;
							break;
						}
					}
					free(groups);
				}
			}
			
			if(!group_match){
				if(!(stats.st_mode & S_IROTH))
					read_perm = 0;
			}
			
			if(!read_perm)
				continue;
		}
		
		if(mtime > 0 && (mtime > (cur_time - stats.st_mtime)))
			continue;
		if(mtime < 0 && ((-1 * mtime) < (cur_time - stats.st_mtime)))
			continue;
			
		tim = localtime(&stats.st_mtime);
		strftime(time_stamp, sizeof(time_stamp), "%b %d, %Y %H:%M", tim);
		
		printStats(path_name, &stats, time_stamp);
		
		if(S_ISDIR(stats.st_mode) && strcmp(dp->d_name, ".") != 0){
			if(volume_flag && (cur_dev != stats.st_dev)){
				fprintf(stderr, "note: not crossing mount point at %s\n");
				continue;
			}
			traverse(0, path_name, cur_time, cur_dev, user_name);
		}
		
		errno = 0;							
	}	
	
	if(errno){
		fprintf(stderr, "ERROR: Failed to read directory %s: %s\n", node, strerror(errno));
		return;
	}
	if((closedir(dirp)) != 0){
		fprintf(stderr, "ERROR: Failed to close directory %s: %s\n", node, strerror(errno));
		exit(-1);
	}	
	
	return;
}

int main(int argc, char* argv[]){
	int opt, user_id;
	char* node, *user_name;
	dev_t cur_dev;
	time_t cur_time;
	while((opt = getopt(argc, argv, "m:vu:")) != -1){
		switch(opt){
			case 'm':
				mtime = atoi(optarg);
				break;
			case 'v':
				volume_flag = 1;
				break;
			case 'u':
				user_flag = 1;
				if(isdigit(optarg[0]) == 0)
					user_name = optarg;
				else{
					user_id = atoi(optarg);
					user_name = getpwuid(user_id)->pw_name;
				}
				break;
			case '?':
				fprintf(stderr, "Usage: [-m mtime] [-u user] [-v] %s\n", argv[0]);
				return -1;				
		}
	}
	
	if(optind >= argc){
		fprintf(stderr, "ERROR: No input node specified\n");
		return -1;
	}
	
	if((optind + 1) < argc){
		fprintf(stderr, "ERROR: Too many input arguments\n");
		return -1;
	}
	
	node = argv[optind];
	if(volume_flag){
		struct stat buf;
		if((lstat(node, &buf)) == -1){
			fprintf(stderr, "ERROR: Cannot get stat for %s: %s\n", node, strerror(errno));
			return -1;
		}
		cur_dev = buf.st_dev;
	}
	
	cur_time = time(NULL);
	
	traverse(1, node, cur_time, cur_dev, user_name);
	
	return 0;
}
