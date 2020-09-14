// Title:	Implementation of the UNIX du Shell Command in C
// Filename:	mydu.c
// Usage:	./mydu [-h]
// 		./mydu [-a] [-B M | -b | -m] [-c] [-d N] [-H] [-L] [-s] <dir1> <dir2> ...
// Author:	Jared Diehl
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

#define DEFAULT_BLOCK_SIZE 1024

struct inode {
	ino_t value;
	struct inode *next;
};

const char *program_name = NULL;
static bool opt_all = false;
static bool opt_summarize_only = false;
static bool apparent_size = false;
static bool max_depth_specified = false;
static int max_depth = 999;
static bool human_output = false;
static bool symlink_deref = false;
static int output_block_size;

static struct inode *inodes;

struct inode *inode_create(ino_t value);
void inode_add(ino_t value);
bool inode_contains(ino_t value);
void inode_display();

int showtreesize(char *path);
int depthfirstapply(char *path, int pathfun(char *path1), int level);
int sizepathfun(char *path);
void showformattedusage(int size, char *info);

void set_program_name(const char *name) {
	program_name = name;
}

struct inode *inode_create(ino_t value) {
	struct inode *new = malloc(sizeof(struct inode));
	new->value = value;
	new->next = NULL;
	return new;
}

void inode_add(ino_t value) {
	struct inode *current = NULL;
	if (inodes == NULL) inodes = inode_create(value);
	else {
		current = inodes;
		while (current->next != NULL)
			current = current->next;
		current->next = inode_create(value);
	}
}

bool inode_contains(ino_t value) {
	struct inode *current = inodes;
	if (inodes == NULL) return false;
	for (; current != NULL; current = current->next)
		if (current->value == value) return true;
	return false;
}

void inode_display() {
	struct inode *current = inodes;
	for (; current != NULL; current = current->next)
		printf("%d\n", (int) current->value);
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
	
	bool print_grand_total = false;
	bool ok = true;
	
	while (true) {
		int c = getopt(argc, argv, "habd:cHmsB:L");
		
		if (c == -1) break;
		
		switch (c) {
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
	
	if (opt_summarize_only) max_depth = 0;
	
	char *cwd = ".";
	int total_size = 0;
	
	if (argv[optind] == NULL) {
		int size = showtreesize(cwd);
		if (size >= 0) total_size += size;
	} else {
		for (; optind < argc; optind++) {
			char *path = argv[optind];
			int size = sizepathfun(path);
			if (size >= 0) {
				total_size += size;
				showformattedusage(size, path);
			} else total_size += showtreesize(path);
		}
	}
	
	if (print_grand_total) showformattedusage(total_size, "total");
	
	return ok ? EXIT_SUCCESS: EXIT_FAILURE;
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
	int total_size = 0;
	
	bool max_depth_exceeded = max_depth_specified && level > max_depth;
	
	DIR *dir;
	struct dirent *entry;
	struct stat stats;
	
	if (!(dir = opendir(path))) {
		perror("opendir");
		return -1;
	}
	
	while ((entry = readdir(dir)) != NULL) {
		char *name = entry->d_name;
		
		char full_path[256];
		sprintf(full_path, "%s/%s", path, name);
		
		if (lstat(full_path, &stats) == -1) {
			perror("lstat");
			return -1;
		}
		
		mode_t mode = stats.st_mode;
		ino_t inode = stats.st_ino;
		
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		
		if (inode_contains(inode)) continue;
		inode_add(inode);
		
		if (S_ISDIR(mode)) {
			int size = depthfirstapply(full_path, pathfun, level + 1);
			
			if (apparent_size) size += stats.st_size;
			else size += stats.st_blocks;
			
			if (size >= 0) {
				total_size += size;
				if (!opt_summarize_only && !max_depth_exceeded) showformattedusage(size, full_path);
			}
		} else if (S_ISLNK(mode) && symlink_deref) {
			if (stat(full_path, &stats) == -1) {
				perror("stat");
				return -1;
			}
			
			inode = stats.st_ino;
			
			if (inode_contains(inode)) continue;
			inode_add(inode);
			
			int size = depthfirstapply(full_path, pathfun, level + 1);
			
			if (apparent_size) size += stats.st_size;
			else size += stats.st_blocks;
			
			if (size >= 0) {
				total_size += size;
				if (!opt_summarize_only && !max_depth_exceeded) showformattedusage(size, full_path);
			}
		} else {
			int size = pathfun(full_path);
			
			if (size >= 0) {
				total_size += size;
				if (opt_all && !max_depth_exceeded) showformattedusage(size, full_path);
			} else {
				if (apparent_size) size = stats.st_size;
				else size = stats.st_blocks;
				total_size += size;
				if (opt_all && !opt_summarize_only && !max_depth_exceeded) showformattedusage(size, full_path);
			}
		}
	}
	
	closedir(dir);
	
	return total_size;
}

int sizepathfun(char *path) {
	struct stat stats;
	
	if (stat(path, &stats) == -1) {
		perror("lstat");
		return -1;
	}
	
	ino_t inode = stats.st_ino;
	if (!inode_contains(inode)) inode_add(inode);
	
	if (S_ISREG(stats.st_mode)) {
		int size;
		if (apparent_size) size = stats.st_size;
		else size = stats.st_blocks;
		return size;
	}
	
	return -1;
}

void showformattedusage(int size, char *info) {
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
			if (output_block_size >= (1 << 20)) {
				int blocks = size * S_BLKSIZE / output_block_size;
				
				if (blocks < 1) blocks = 1;
				
				sprintf(result, "%d", blocks);
			} else if (output_block_size > 1) {
				int blocks = size * S_BLKSIZE / output_block_size;
				
				if (blocks < 1) blocks = 1;
				
				if (output_block_size == (1 << 30)) sprintf(result, "%dG", blocks);
				else if (output_block_size == (1 << 20)) sprintf(result, "%dM", blocks);
				else if (output_block_size == (1 << 10)) sprintf(result, "%dK", blocks);
				else sprintf(result, "%d", blocks);
			} else {
				int blocks = size * S_BLKSIZE / DEFAULT_BLOCK_SIZE;
				
				sprintf(result, "%d", blocks);
			}
		}
	}
	
	printf("%-7s %s\n", result, info);
}
