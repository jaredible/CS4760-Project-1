#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

char options[10];
int scaler = -1;
int max_depth = -1;

int sizepathfun(char *path);
int depthfirstapply(char *path, int pathfun(char *path1), int depth);
int showtreesize(char *path);

int main(int argc, char *argv[]) {
	int opt;
	
	while ((opt = getopt(argc, argv, "haB:bmcd:HLs")) != -1) {
		char str[2];
		sprintf(str, "%c", opt);
		strcat(options, str);
		
		switch (opt) {
			case 'B':
				scaler = atoi(optarg);
				break;
			case 'd':
				max_depth = atoi(optarg);
				break;
			case 'm':
				scaler = 1024 * 1024;
				break;
			default:
				break;
		}
	}
	
	int size = 0;
	if (argv[optind] == NULL) size += showtreesize(".");
	else {
		for (; optind < argc; optind++) {
			size += showtreesize(argv[optind]);
		}
	}
	printf("%-7d total\n", size);
	
	return EXIT_SUCCESS;
}

int showtreesize(char *path) {
	int size = depthfirstapply(path, sizepathfun, 0);
	printf("%-7d %s\n", size, path);
	return size;
}

int depthfirstapply(char *path, int pathfun(char *path1), int depth) {
	int result = 0;
	
	DIR *dir;
	struct dirent *entry;
	struct stat stats;
	
	if (!(dir = opendir(path))) {
		perror("Error");
		return -1;
	}
	
	while ((entry = readdir(dir)) != NULL) {
		char *name = entry->d_name;
		
		char fullpath[256];
		sprintf(fullpath, "%s/%s", path, name);
		
		lstat(fullpath, &stats);
		
		int mode = stats.st_mode;
		
		if (S_ISDIR(mode)) {
			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
			int size = depthfirstapply(fullpath, pathfun, depth + 1);
			size += stats.st_size;
			if (size >= 0) {
				printf("%-7d %s\n", size, fullpath);
				result += size;
			}
		} else {
			int size = pathfun(fullpath);
			if (size >= 0) {
				printf("%-7d %s\n", size, fullpath);
				result += size;
			}
		}
	}
	
	closedir(dir);
	
	return result;
}

int sizepathfun(char *path) {
	struct stat stats;
	
	if (lstat(path, &stats) == -1) {
		perror("Error");
		return -1;
	}
	
	mode_t mode = stats.st_mode;
	
	if (S_ISREG(mode) | S_ISLNK(mode)) return stats.st_size;
	
	return -1;
}
