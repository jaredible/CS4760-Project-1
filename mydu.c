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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

struct node {
	int value;
	struct node *next;
};

char options[10];
const char *program_name = NULL;
static bool opt_all = false;
static bool apparent_size = false;
static bool print_grand_total = false;
static int max_depth = 999;
static bool human_output = false;
static bool symlink_deref = false;
static int output_block_size;
static struct node *inodes;

struct node *create(int value);
void add(int value);
bool contains(int value);
void display();

int showtreesize(char *path);
int depthfirstapply(char *path, int pathfun(char *path1), int level);
int sizepathfun(char *path);
void showformattedusage(int size, char *path);

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
	if (inodes == NULL) inodes = create(value);
	else {
		current = inodes;
		while (current->next != NULL)
			current = current->next;
		current->next = create(value);
	}
}

bool contains(int value) {
	struct node *current = inodes;
	if (inodes == NULL) return false;
	for (; current != NULL; current = current->next)
		if (current->value == value) return true;
	return false;
}

void display() {
	struct node *current = inodes;
	for (; current != NULL; current = current->next)
		printf("%d\n", current->value);
}

void error(const char *fmt, ...) {
	int n;
	int size = 100;
	char *p, *np;
	va_list ap;
	
	if ((p = malloc(size)) == NULL) return;
	
	while (true) {
		va_start(ap, fmt);
		n = vsnprintf(p, size, fmt, ap);
		va_end(ap);
		
		if (n < 0) return;
		
		if (n < size) break;
		
		size = n + 1;
		
		if ((np = realloc(p, size)) == NULL) {
			free(p);
			return;
		} else p = np;
	}
	
	fprintf(stderr, "%s: %s\n", program_name, p);
}

void usage(int status) {
	if (status != EXIT_SUCCESS) fprintf(stderr, "Try '%s -h' for more information.\n", program_name);
	else {
		printf("NAME\n");
		printf("       %s - estimate file space usage\n", program_name);
		printf("\nUSAGE:\n");
		printf("       %s [-h]\n", program_name);
		printf("       %s [-a] [-B M | -b | -m] [-c] [-d N] [-H] [-L] [-s] <dir1> <dir2> ...\n", program_name);
		printf("\nDESCRIPTION\n");
		printf("       -a     : Write count for all files, not just directories\n");
		printf("       -B M   : Scale sizes by M before printing; for example, -BM prints size in units of 1,048,576 bytes\n");
		printf("       -b     : Print size in bytes\n");
		printf("       -c     : Print a grand total\n");
		printf("       -d N   : Print the total for a directory only if it is N or fewer levels below the command line argument\n");
		printf("       -h     : Print a help message or usage, and exit\n");
		printf("       -H     : Human readable; print size in human readable format, for example, 1K, 234M, 2G\n");
		printf("       -L     : Dereference all symbolic links\n");
		printf("       -m     : Same as -B 1048576\n");
		printf("       -s     : Display only a total for each argument\n");
	}
	exit(status);
}

int get_index(char *string, char c) {
	char *e = strchr(string, c);
	if (e == NULL) return -1;
	return (int) (e - string);
}

void human_options(const char *spec) {
	if (isdigit(*spec)) output_block_size = atoi(spec);
	else {
		apparent_size = false;
		human_output = false;
		char *s = "KMG";
		int i;
		if ((i = get_index(s, *spec)) == -1) {
			error("invalid -B argument '%s'", spec);
			usage(EXIT_FAILURE);
		} else output_block_size = (1 << (10 * (i + 1)));
	}
}

int main(int argc, char **argv) {
	set_program_name(argv[0]);
	
	bool max_depth_specified = false;
	bool opt_summarize_only = false;
	bool ok = true;
	
	int opt;
	while ((opt = getopt(argc, argv, "habd:cHmsB:L")) != -1) {
		char str[2];
		sprintf(str, "%c", opt);
		strcat(options, str);
		
		switch (opt) {
			case 'h':
				usage(EXIT_SUCCESS);
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
				if (isdigit(*optarg)) {
					max_depth_specified = true;
					max_depth = atoi(optarg);
				} else {
					error("invalid maximum depth '%s'", optarg);
					ok = false;
				}
				break;
			case 'm':
				output_block_size = 1024 * 1024;
				break;
			case 's':
				opt_summarize_only = true;
				break;
			case 'B':
				human_options(optarg);
				break;
			case 'L':
				symlink_deref = true;
				break;
			default:
				ok = false;
		}
	}
	
	if (!ok) usage(EXIT_FAILURE);
	
	if (opt_all && opt_summarize_only) {
		error("cannot both summarize and show all entries");
		usage(EXIT_FAILURE);
	}
	
	if (opt_summarize_only && max_depth_specified && max_depth == 0) {
		error("warning: summarizing is the same as using --max-depth=0");
	}
	
	if (opt_summarize_only && max_depth_specified && max_depth != 0) {
		error("warning: summarizing conflicts with --max-depth=%d", max_depth);
		usage(EXIT_FAILURE);
	}
	
	if (opt_summarize_only) {
		max_depth = 0;
	}
	
	char *pwd = ".";
	int totalsize = 0;
	
	if (argv[optind] == NULL) {
		int size = showtreesize(pwd);
		totalsize += size;
	} else {
		for (; optind < argc; optind++) {
			char *path = argv[optind];
			int size = sizepathfun(path);
			if (size >= 0) {
				totalsize += size;
				showformattedusage(size, path);
			} else totalsize += showtreesize(path);
		}
	}
	
	if (strstr(options, "c") != NULL) showformattedusage(totalsize, "total");
	
	return EXIT_SUCCESS;
}

int showtreesize(char *path) {
	struct stat stats;
	
	if (lstat(path, &stats) == -1) {
		perror("lstat");
		return -1;
	}
	
	int size = depthfirstapply(path, sizepathfun, 1);
	
	if (apparent_size) size += stats.st_size;
	else size += stats.st_blocks;
	
	showformattedusage(size, path);
	
	return size;
}

int depthfirstapply(char *path, int pathfun(char *path1), int level) {
	if (strstr(options, "d") != NULL && level > max_depth) return -1;
	
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
		
		if (contains(inode)) continue;
		add(inode);
		
		if (S_ISDIR(mode)) {
			int size = depthfirstapply(fullpath, pathfun, level + 1);
			
			if (apparent_size) size += stats.st_size;
			else size += stats.st_blocks;
			
			if (size >= 0) {
				result += size;
				if (strstr(options, "s") == NULL) showformattedusage(size, fullpath);
			}
		} else if (S_ISLNK(mode) && symlink_deref) {
			if (stat(fullpath, &stats) == -1) {
				perror("stat");
				return -1;
			}
			
			inode = stats.st_ino;
			
			if (contains(inode)) continue;
			add(inode);
			
			int size = depthfirstapply(fullpath, pathfun, level + 1);
			
			if (apparent_size) size += stats.st_size;
			else size += stats.st_blocks;
			
			if (size >= 0) {
				result += size;
				if (strstr(options, "s") == NULL) showformattedusage(size, fullpath);
			}
		} else {
			int size = pathfun(fullpath);
			
			if (size >= 0) {
				result += size;
				if (strstr(options, "a") != NULL) showformattedusage(size, fullpath);
			} else {
				int s = 0;
				if (apparent_size) s = stats.st_size;
				else s = stats.st_blocks;
				result += s;
				if (opt_all && strstr(options, "s") == NULL) showformattedusage(s, fullpath);
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
	
	if (S_ISREG(stats.st_mode)) {
		int size = 0;
		if (apparent_size) size = stats.st_size;
		else size = stats.st_blocks;
		return size;
	}
	
	return -1;
}

void showformattedusage(int size, char *path) {
	char *result = malloc(sizeof(char) * 255);
	
	if (human_output) {
		int bytes = size * S_BLKSIZE;
		
		if (apparent_size) bytes = size;
		
		if (bytes >= 10L * (1 << 30)) sprintf(result, "%dG", bytes / (1 << 30));
		else if (bytes >= (1 << 30)) sprintf(result, "%2.1fG", (float) (bytes / 1e9));
		else if (bytes >= 10 * (1 << 20)) sprintf(result, "%dM", bytes / (1 << 20));
		else if (bytes >= (1 << 20)) sprintf(result, "%2.1fM", (float) (bytes / 1e6));
		else if (bytes >= 10 * (1 << 10)) sprintf(result, "%dK", bytes / (1 << 10));
		else if (bytes >= (1 << 10)) sprintf(result, "%2.1fK", (float) (bytes / 1e3));
		else sprintf(result, "%d", bytes);
	} else {
		if (apparent_size) sprintf(result, "%d", size);
		else {
			if (output_block_size > 1) { //strstr(options, "B") != NULL) {
				size = size * S_BLKSIZE / output_block_size;
				
				if (size < 1) size = 1;
				
				if (output_block_size == (1 << 30)) sprintf(result, "%dG", size);
				else if (output_block_size == (1 << 20)) sprintf(result, "%dM", size);
				else if (output_block_size == (1 << 10)) sprintf(result, "%dK", size);
				else sprintf(result, "%d", size);
			} else {
				if (strstr(options, "m") != NULL) {
					size = size * S_BLKSIZE / output_block_size;
					if (size < 1) size = 1;
					sprintf(result, "%d", size);
				} else sprintf(result, "%d", size * S_BLKSIZE / 1024);
			}
		}
	}
	
	printf("%-7s %s\n", result, path);
}
