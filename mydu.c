#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	
	return EXIT_SUCCESS;
}

int showtreesize(char *path) {
	return 0;
}

int depthfirstapply(char *path, int pathfun(char *path1), int depth) {
	return 0;
}

int sizepathfun(char *path) {
	return 0;
}
