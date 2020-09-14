// Title:	Implementation of UNIX du Shell Command in C
// Filename:	mydu.c
// Usage:	./mydu [-h] [-a] [-B M | -b | -m] [-c] [-d N] [-H] [-L] [-s] <dir1> <dir2> ...
// Author:	Jared Diehl (jmddnb@umsystem.edu)
// Date:	September 14, 2020
// Description:	Displays the size of subdirectories of the tree rooted at the
// 		directories/files specified on the command-line arguments.

#include <ctype.h>
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

struct pinfo {
	char *program_name;
	char options[10];
	int scaler;
	int max_depth;
	struct node *inodes;
};

struct node {
	int value;
	struct node *next;
};

static struct pinfo *program_info;

const char *program_name = NULL;
static bool opt_all = false;
static bool apparent_size = false;
static bool print_grand_total = false;
static int max_depth = 999;
static bool human_output = false;
static int output_block_size;

struct node *create(int value);
void add(int value);
bool contains(int value);
void display();

int showtreesize(char *path);
int depthfirstapply(char *path, int pathfun(char *path1), int depth);
int sizepathfun(char *path);
void showformattedusage(long long int size, char *path);

void human(int x);

void set_program_name(const char *argv0) {
	program_name = argv0;
}

struct node *create(int value) {
	struct node *new = malloc(sizeof(struct node));
	new->value = value;
	new->next = NULL;
	return new;
}

void add(int value) {
	struct node *current = NULL;
	if (program_info->inodes == NULL) program_info->inodes = create(value);
	else {
		current = program_info->inodes;
		while (current->next != NULL)
			current = current->next;
		current->next = create(value);
	}
}

bool contains(int value) {
	struct node *current = program_info->inodes;
	if (program_info->inodes == NULL) return false;
	for (; current != NULL; current = current->next)
		if (current->value == value) return true;
	return false;
}

void display() {
	struct node *current = program_info->inodes;
	for (; current != NULL; current = current->next)
		printf("%d\n", current->value);
}

static bool isBytes = false;

char *formatBytes(int blocks, char *path) {
//	char *suffix[] = {"", "K", "M", "G"};
//	char length = sizeof(suffix) / sizeof(suffix[0]);
//	
//	int i = 0;
//	double dblBytes = bytes;
//	
//	if (bytes > 1024) {
//		for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024)
//			dblBytes = bytes / 1024.0;
//	}
//	
//	static char output[200];
//	sprintf(output, "%2.1f%s", dblBytes, suffix[i]);
//	return output;
	
	bool test = false;
	if (test) {
		// assuming blocks are being usaged (512-byte blocks)
		// 512 * 8 = 4096, 512 * 2 = 1024
		int scaler = output_block_size;
		char *s = "KMG";
		int i = strlen(s);
		while (blocks >= scaler && i > 1) {
			blocks /= scaler;
			i--;
		}
	}
	
	bool humanReadable = human_output;
	char *result = malloc(sizeof(char) * 255);
	//printf("output_block_size: %d\n", output_block_size);
	//int block_size = output_block_size;
	//printf("[%s] [isBytes: %s] ", path, isBytes ? "true" : "false");
	
	if (humanReadable) {
		int bytes = blocks * S_BLKSIZE; // output_block_size;
		if (strstr(program_info->options, "b") != NULL) bytes = blocks;
		if (bytes >= 10L * 1000 * 1000 * 1000) sprintf(result, "%lluG", (long long) (bytes / 1e9));
		else if (bytes >= 1000 * 1000 * 1000) sprintf(result, "%2.fG", (float) (bytes / 1e9));
		else if (bytes >= 10 * 1000 * 1000) sprintf(result, "%lluM", (long long) (bytes / 1e6));
		else if (bytes >= 1000 * 1000) sprintf(result, "%2.1fM", (float) (bytes / 1e6));
		else if (bytes >= 10 * 1000) sprintf(result, "%lluK", (long long) (bytes / 1e3));
		else if (bytes >= 1000) sprintf(result, "%2.1fK", (float) (bytes / 1e3));
		else sprintf(result, "%d", bytes);
//		if (bytes > 10L * (1 << 30)) sprintf(result, "%dG", bytes / (1 << 30));
//		else if (bytes >= (1 << 30)) sprintf(result, "%2.1fG", (float) (bytes / (1 << 30)));
//		else if (bytes > 10 * (10 << 20)) sprintf(result, "%dM", bytes / (1 << 20));
//		else if (bytes > (1 << 20)) sprintf(result, "%2.1fM", (float) (bytes / (1 << 20)));
//		else if (bytes > 10 * (1 << 10)) sprintf(result, "%dK", bytes / (1 << 10));
//		else if (bytes > (1 << 10)) sprintf(result, "%2.1fK", (float) (bytes / (1 << 10)));
//		else sprintf(result, "%d", bytes);
	} else {
		if (strstr(program_info->options, "b") != NULL) sprintf(result, "%d", blocks);
		else {
			if (strstr(program_info->options, "B") != NULL) {
				int size = blocks * S_BLKSIZE / output_block_size;
				if (size < 1) size = 1;
				if (output_block_size == (1 << 30)) sprintf(result, "%dG", size);
				else if (output_block_size == (1 << 20)) sprintf(result, "%dM", size);
				else if (output_block_size == (1 << 10)) sprintf(result, "%dK", size);
				else sprintf(result, "%d", size);
			} else {
				if (strstr(program_info->options, "m") != NULL) {
					int size = blocks * S_BLKSIZE / output_block_size;
					if (size < 1) size = 1;
					sprintf(result, "%d", size);
				} else sprintf(result, "%d", blocks * S_BLKSIZE / 1024);
			}
		}
	}
	
	return result;
}

void formatBlocks(int blocks) {
	//int bytes = blocks * S_BLKSIZE;
	//printf("%d\n", bytes);
	//formatBytes(bytes);
}

void printusage(int size, char *path) {
	char *formattedBytes = formatBytes(size, path);
	printf("%-7s %s\n", formattedBytes, path);
}

void traverse(char *path) {
	DIR *dir;
	struct dirent *entry;
	struct stat stats;
	
	if (!(dir = opendir(path))) {
		perror("opendir");
		return;
	}
	
	while ((entry = readdir(dir)) != NULL) {
		char *name = entry->d_name;
		
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		if (strncmp(name, "f", 1) != 0) continue;
		
		char fullpath[255];
		sprintf(fullpath, "%s/%s", path, name);
		
		if (lstat(fullpath, &stats) == -1) {
			perror("lstat");
			continue;
		}
		
		int mode = stats.st_mode;
		
		if (S_ISREG(mode)) {
			int size = stats.st_blocks;// / 2;
			if (size >= 0) printusage(size, fullpath);
		}
	}
}

void error(const char *message) {
	fprintf(stderr, "%s: %s\n", program_name, message);
	fprintf(stderr, "Try '%s -h' for more information.", program_name);
}

void usage(int status) {
	printf("USAGE\n");
	exit(status);
}

int main(int argc, char **argv) {
	program_info = malloc(sizeof(struct pinfo));
	
//	add(1);
//	add(2);
//	add(3);
//	printf("has 1: %s\n", contains(1) ? "true" : "false");
//	printf("has 2: %s\n", contains(2) ? "true" : "false");
//	printf("has 3: %s\n", contains(3) ? "true" : "false");
//	printf("has 4: %s\n", contains(4) ? "true" : "false");
//	display();
//	return 0;
	
	set_program_name(argv[0]);
	
	//program_info = malloc(sizeof(struct pinfo));
	program_info->program_name = argv[0];
	
	bool max_depth_specified = false;
	//bool symlink_deref = false;
	bool opt_summarize_only = false;
	
	//traverse(".");
	//formatBytes(atoi(argv[1]));
	//printf("\n");
	//formatBlocks(atoi(argv[2]));
	//return 0;
	
	int opt;
	while ((opt = getopt(argc, argv, "habd:cHmsB:L")) != -1) {
		char str[2];
		sprintf(str, "%c", opt);
		strcat(program_info->options, str);
		
		switch (opt) {
			case 'h':
				usage(EXIT_SUCCESS);
				break;
			case 'a':
				opt_all = true;
				break;
			case 'b':
				apparent_size = true;
				human_output = false;
				output_block_size = 1;
				break;
			case 'c':
				print_grand_total = true;
				break;
			case 'H':
				human_output = true;
				output_block_size = 1;
				break;
			case 'd':
				if (isdigit(*optarg)) max_depth = atoi(optarg);
				else {
					fprintf(stderr, "%s: invalid maximum depth ‘%s’\n", program_name, optarg);
					fprintf(stderr, "Try '%s -h' for more information.\n", program_name);
				}
				break;
			case 'm':
				//human_output = true;
				output_block_size = 1024 * 1024;
				break;
			case 's':
				opt_summarize_only = true;
				break;
			case 'B':
				if (isdigit(*optarg)) output_block_size = atoi(optarg);
				else {
					//human_output = true;
					if (strcmp(optarg, "G") == 0) output_block_size = 1024 * 1024 * 1024;
					else if (strcmp(optarg, "M") == 0) output_block_size = 1024 * 1024;
					else if (strcmp(optarg, "K") == 0) output_block_size = 1024;
					else {
						fprintf(stderr, "%s: invalid -B argument '%s'\n", program_name, optarg);
						fprintf(stderr, "Try '%s -h' for more information.", program_name);
					}
				}
				break;
			case 'L':
				//symlink_deref = true;
				break;
			//case 'd':
			//	program_info->max_depth = atoi(optarg);
			//	break;
			//case 'm':
			//	program_info->scaler = 1024 * 1024;
			//	break;
		}
	}
	
	if (opt_all && opt_summarize_only) {
		error("cannot both summarize and show all entries");
		usage(EXIT_FAILURE);
	}
	
	if (opt_summarize_only && max_depth_specified && max_depth == 0) {
	}
	
	if (opt_summarize_only) {
		max_depth = 0;
	}
	
	//return 0;
	
//	if (strstr(program_info->options, "a") != NULL && strstr(program_info->options, "s") != NULL) {
//		fprintf(stderr, "%s: cannot both summarize and show all entries\nTry '%s -h' for more information.\n", program_info->program_name, program_info->program_name);
//		return EXIT_FAILURE;
//	}
	
	char *pwd = ".";
	int totalsize = 0;
	
	if (argv[optind] == NULL) {
		int size = showtreesize(pwd);
		totalsize += size;
		//if (strstr(program_info->options, "s") != NULL) showformattedusage(size, pwd);
	} else {
		for (; optind < argc; optind++) {
			char *path = argv[optind];
			int size = sizepathfun(path);
//			printf("TEST: %d\n", size);
			if (size >= 0) {
				totalsize += size;
				showformattedusage(size, path);
			} else totalsize += showtreesize(path);
//			printf("==========\n");
//			display();
//			printf("==========\n");
			//if (strstr(program_info->options, "s") != NULL) showformattedusage(size, path);
		}
	}
	
	//display();
	
	if (strstr(program_info->options, "c") != NULL) showformattedusage(totalsize, "total");
	
	//human(2069);
	
	return EXIT_SUCCESS;
}

int showtreesize(char *path) {
	struct stat stats;
	
	if (lstat(path, &stats) == -1) {
		perror("lstat");
		return -1;
	}
	
	int size = depthfirstapply(path, sizepathfun, 0);
	
	if (strstr(program_info->options, "b") != NULL) size += stats.st_size;
	else size += stats.st_blocks;// / 2;
	
	showformattedusage(size, path);
	
	return size;
}

int depthfirstapply(char *path, int pathfun(char *path1), int depth) {
	if (strstr(program_info->options, "d") != NULL && depth > program_info->max_depth) return -1;
	
	int result = 0;
	
	DIR *dir;
	struct dirent *entry;
	struct stat stats;
	
	if (!(dir = opendir(path))) {
		perror("opendir");
		return -1;
	}
	
	while ((entry = readdir(dir)) != NULL) {
		char *name = entry->d_name;
		
		char fullpath[255];
		sprintf(fullpath, "%s/%s", path, name);
		
		if (lstat(fullpath, &stats) == -1) {
			perror("lstat");
			return -1;
		}
		
		int mode = stats.st_mode;
		int inode = stats.st_ino;
		
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		
		if (contains(inode)) {
//			printf("inode %d exists: %s\n", inode, fullpath);
			//display();
			continue;
		}
		add(inode);
//		display();
		
//		printf("inode: %d, fullpath: %s\n", inode, fullpath);
		
		if (S_ISDIR(mode)) {
			int size = depthfirstapply(fullpath, pathfun, depth + 1);
			
			if (strstr(program_info->options, "b") != NULL) size += stats.st_size;
			else size += stats.st_blocks;// / 2;
			
			if (size >= 0) {
				result += size;
				if (strstr(program_info->options, "s") == NULL) showformattedusage(size, fullpath);
			}
		} else if (S_ISLNK(mode) && strstr(program_info->options, "L") != NULL) {
			if (stat(fullpath, &stats) == -1) {
				perror("stat");
				return -1;
			}
			
			inode = stats.st_ino;
			
			int size = depthfirstapply(fullpath, pathfun, depth + 1);
			
			if (strstr(program_info->options, "b") != NULL) size += stats.st_size;
			else size += stats.st_blocks;// / 2;
			
			if (size >= 0) {
				result += size;
				if (strstr(program_info->options, "s") == NULL) showformattedusage(size, fullpath);
			}
		} else {
			int size = pathfun(fullpath);
			
			if (size >= 0) {
				result += size;
				if (strstr(program_info->options, "a") != NULL) showformattedusage(size, fullpath);
			} else {
				int s = 0;
				if (strstr(program_info->options, "b") != NULL) s = stats.st_size;
				else s = stats.st_blocks;// / 2;
				result += s;
				if (strstr(program_info->options, "a") != NULL && strstr(program_info->options, "s") == NULL) showformattedusage(s, fullpath);
			}
		}
	}
	
	closedir(dir);
	
	return result;
}

int sizepathfun(char *path) {
	struct stat stats;
	
	if (stat(path, &stats) == -1) {
		perror("lstat");
		return -1;
	}
	
	//int inode = stats.st_ino;
	//if (contains(inode)) return 0;
	//add(inode);
	
	if (S_ISREG(stats.st_mode)) {
		int size = 0;
		if (strstr(program_info->options, "b") != NULL) size = stats.st_size;
		else size = stats.st_blocks;// / 2;
		return size;
	}
	
	return -1;
}

void human(int x) {
	char *s = "KMGTEPYZ";
	int i = strlen(s);
	while (x >= 1024 && i > 1) {
		x /= 1024;
		i--;
	}
	//printf("%d%c\n", x, s[strlen(s) - i - 1]);
}

void showformattedusage(long long int size, char *path) {
//	return;
	//isBytes = !isBytes;
	isBytes = apparent_size;
	printusage(size, path);
	//isBytes = !isBytes;
	return;
	const char *sizesuffix = " ";
	if (strstr(program_info->options, "H") != NULL) {
		//if (strstr(program_info->options, "b") != NULL) {
		//	long double ss = size;
		//	char *s = "KMGTEPYZ";
		//	int i = strlen(s);
		//	while (ss >= 1024 && i > 1) {
		//		ss /= 1024;
		//		i--;
		//	}
		//	ss += 0.5;
		//	char buf[2];
		//	sprintf(buf, "%c", s[strlen(s) - i - 1]);
		//	sizesuffix = buf;
		//	size = ss;
		//}// else {
			if (size >= 1e9) {
				size = (long long) (size / 1e9) ;
				sizesuffix = "G";
			} else if (size >= 1e6) {
				size = (long long) (size / 1e6);
				sizesuffix = "M";
			} else if (size >= 1e3) {
				size = (long long) (size / 1e3);
				sizesuffix = "K";
			}
		//}
	}
	
	if (strstr(program_info->options, "B") != NULL) {
		size /= program_info->scaler;
		if (size < 1) size = 1;
	}
	
	char str[8];
	sprintf(str, "%llu%s", size, sizesuffix);

	printf("%-7s %s\n", str, path);
}
