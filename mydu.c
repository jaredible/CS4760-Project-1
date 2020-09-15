/* Title: Implementation of the UNIX du Shell Command in C
 * Filename: mydu.c
 * Usage: ./mydu [-h]
 *        ./mydu [-a] [-B M | -b | -m] [-c] [-d N] [-H] [-L] [-s] <dir1> <dir2> ...
 * Author(s): Jared Diehl
 * Date: September 14, 2020
 * Description: Displays the size of subdirectories of the tree rooted at the
 *              directories/files specified on the command-line arguments.
 * Source(s): https://github.com/coreutils/coreutils/blob/master/src/du.c */

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

#define DEFAULT_MAX_DEPTH 999

/* A structure for collecting inode information. */
struct inode {
	/* Actual inode value. */
	ino_t value;
	/* Pointer to next inode structure. */
	struct inode *next;
};

/* The name of this program set at execution. */
const char *program_name = NULL;

/* If true, display counts for all files, not just directories. */
static bool opt_all = false;

/* If true, display only a total for each argument. */
static bool opt_summarize_only = false;

/* If true, use the apparent size of a file. */
static bool apparent_size = false;

/* If true, print a grand total at the end. */
static bool print_grand_total = false;

/* If true, a block scaler has been specified. */
static bool opt_block_scaler = false;

/* If true, a max depth has been specified. */
static bool max_depth_specified = false;

/* Show the total for each directory that is at most this depth. */
static int max_depth = DEFAULT_MAX_DEPTH;

/* Human readable options for output. */
static bool human_output = false;

/* Dereference symbolic links. */
static bool symlink_deref = false;

/* The units to use when printing sizes. */
static int output_block_size;

/* The head pointer of the inodes linked-list. */
struct inode *inodes;

/* Prototypes for disk usage operations. */
int showtreesize(char *path);
int depthfirstapply(char *path, int pathfun(char *path1), int level);
int sizepathfun(char *path);
int sizeoptions(struct stat *stats);
void showformattedusage(int size, char *info);

/* Prototypes for inode linked-list operations. */
struct inode *inode_create(ino_t value);
void inode_add(ino_t value);
bool inode_contains(ino_t value);
void inode_list();

/* Only sets the program's name. */
void set_program_name(const char *name) {
	program_name = name;
}

/* Returns the index of the first occurence of c in string. */
int get_index(char *string, char c) {
	char *e = strchr(string, c);
	if (e == NULL) return -1;
	return (int) (e - string);
}

/* used for displaying formatted error string. */
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

/* Prints usage information. */
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

/* Used to set human readability options. */
void human_options(const char *spec) {
	human_output = false;
	if (isdigit(*spec)) output_block_size = atoi(spec);
	else {
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
	
	char *cwd = ".";
	int total_size = 0;
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
				opt_block_scaler = true;
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
		error("warning: summarizing is the same as using -d 0");
	}
	
	if (opt_summarize_only && max_depth_specified && max_depth != 0) {
		error("warning: summarizing conflicts with -d %d", max_depth);
		usage(EXIT_FAILURE);
	}
	
	if (opt_summarize_only) max_depth = 0;
	
	/* If no files are specified in the command-line arguments, then use current working directory. */
	if (argv[optind] == NULL) {
		int size = showtreesize(cwd);
		if (size >= 0) total_size += size;
	/* Get files from command-line arguments and get their disk usage. */
	} else {
		for (; optind < argc; optind++) {
			char *path = argv[optind];
			int size = sizepathfun(path);
			if (size >= 0) {
				total_size += size;
				showformattedusage(size, path);
			} else {
				size = showtreesize(path);
				if (size >= 0) total_size += size;
			}
		}
	}
	
	/* Print a grand total of all the files. */
	if (print_grand_total) showformattedusage(total_size, "total");
	
	return ok ? EXIT_SUCCESS: EXIT_FAILURE;
}

/* Responsible for showing the tree size of a directory at a specified path. */
int showtreesize(char *path) {
	/* Contains construct that facilitates getting file information. */
	struct stat stats;
	
	/* Attempt to return information of the directory at path. */
	if (lstat(path, &stats) == -1) {
		/* Getting the directory's information failed, so display an error and return -1. */
		perror("lstat");
		return -1;
	}
	
	/* Add the traversed files/subdirectories' sizes of the directory, and its size too. */
	int size = depthfirstapply(path, sizepathfun, 1) + sizeoptions(&stats);
	
	/* Display the directory's size and path. */
	showformattedusage(size, path);
	
	/* Return the size of the directory. */
	return size;
}

/* Responsible for traversing through a directory's files/subdirectories at a specified path. */
int depthfirstapply(char *path, int pathfun(char *path1), int level) {
	/* The sum of all the files/sumdirectories of the directory. */
	int total_size = 0;
	
	bool max_depth_exceeded = max_depth_specified && level > max_depth;
	
	/* A type representing a directory stream. */
	DIR *dir;
	
	/* Contains construct that facilitates directory traversing. */
	struct dirent *entry;
	
	/* Contains construct that facilitates getting file information. */
	struct stat stats;
	
	/* Check if able to open the directory stream corresponding to the directory path. */
	if (!(dir = opendir(path))) {
		/* Opening the directory stream failed, so display an error and return -1. */
		perror("opendir");
		return -1;
	}
	
	/* Loop through reading each of the directory's entries. */
	while ((entry = readdir(dir)) != NULL) {
		/* Get the name of the file. */
		char *name = entry->d_name;
		
		/* Construct a full path using path and name. */
		char full_path[256];
		sprintf(full_path, "%s/%s", path, name);
		
		/* Attempt to return information of the file at full path. */
		if (lstat(full_path, &stats) == -1) {
			/* Getting the file's information failed, so display an error and return -1. */
			perror("lstat");
			return -1;
		}
		
		/* Get the file mode of the current file entry. */
		mode_t mode = stats.st_mode;
		
		/* Get the inode number of the current file entry. */
		ino_t inode = stats.st_ino;
		
		/* Ignore these two directories, since it would cause an infinite loop. */
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		
		/* If we've already seen this inode, ignore this file and continue to the next file entry. */
		if (inode_contains(inode)) continue;
		
		/* Since we haven't seen this inode, add it to the list of inodes. */
		inode_add(inode);
		
		/* Check if mode corresponds to a directory. */
		if (S_ISDIR(mode)) {
			/* Add the traversed files/subdirectories' sizes of the directory, and its size too. */
			int size = depthfirstapply(full_path, pathfun, level + 1) + sizeoptions(&stats);
			
			/* If size isn't -1, then add it to the total sum and print usage information. */
			if (size >= 0) {
				total_size += size;
				if (!opt_summarize_only && !max_depth_exceeded) showformattedusage(size, full_path);
			}
		/* Check if mode corresponds to a symbolic link and dereferencing is enabled. */
		} else if (S_ISLNK(mode) && symlink_deref) {
			/* Attempt to return information of the symbolic link at full path. */
			if (stat(full_path, &stats) == -1) {
				/* Getting the symbolic link's information failed, so display an error and return -1. */
				perror("stat");
				return -1;
			}
			
			/* Update the inode number since we are dereferencing the symbolic link. */
			inode = stats.st_ino;
			
			/* If we've already seen this inode, ignore this symbolic link and continue to the next file entry. */
			if (inode_contains(inode)) continue;
			
			/* Since we haven't seen this inode, add it to the list of inodes. */
			inode_add(inode);
			
			/* Add the traversed files/subdirectories' sizes of the symbolic link, and its size too. */
			int size = depthfirstapply(full_path, pathfun, level + 1) + sizeoptions(&stats);
			
			/* If size isn't -1, then add it to the total sum and print usage information. */
			if (size >= 0) {
				total_size += size;
				if (!opt_summarize_only && !max_depth_exceeded) showformattedusage(size, full_path);
			}
		} else {
			/* Get the file's size at full path. */
			int size = pathfun(full_path);
			
			/* If size isn't -1, then add it to the total sum and print usage information. */
			if (size >= 0) {
				total_size += size;
				if (opt_all && !max_depth_exceeded) showformattedusage(size, full_path);
			/* Since it's size is -1, this means it's a symbolic link, so then get its size, add it to the total sum, and print usage information. */
			} else {
				size = sizeoptions(&stats);
				total_size += size;
				if (opt_all && !opt_summarize_only && !max_depth_exceeded) showformattedusage(size, full_path);
			}
		}
	}
	
	/* Don't forget to close the directory stream. */
	closedir(dir);
	
	/* Return the total sum of files and directories specified at path. */
	return total_size;
}

/* Responsible for returning a regular file's size, otherwise -1. */
int sizepathfun(char *path) {
	/* Contains construct that facilitates getting file information. */
	struct stat stats;
	
	/* Attempt to return information of the file at path. */
	if (lstat(path, &stats) == -1) {
		/* Getting the file's information failed, so display an error and return -1. */
		perror("lstat");
		return -1;
	}
	
	/* Get the inode number of the file that path references. */
	ino_t inode = stats.st_ino;
	
	/* If we haven't seen this inode before, then add it to the list. */
	if (!inode_contains(inode)) inode_add(inode);
	
	/* If path references a regular file, then return its size. */
	if (S_ISREG(stats.st_mode)) return sizeoptions(&stats);
	
	/* If we get here, then path didn't reference a regular file. */
	return -1;
}

/* Responsible for return a file's size. */
int sizeoptions(struct stat *stats) {
	if (apparent_size) {
		/* Return the file's byte count. */
		return stats->st_size;
	} else {
		/* Return the file's block count. */
		return stats->st_blocks;
	}
}

/* Responsible for displaying a formatted size and some information. */
void showformattedusage(int size, char *info) {
	/* Buffer for size formatting. */
	char *pretty_size = malloc(sizeof(char) * 256);
	
	if (human_output) {
		/* Calculate the bytes, so treat size as blocks. */
		int bytes = size * S_BLKSIZE;
		
		/* If bytes are to be used instead, then treat size as bytes */
		if (apparent_size) bytes = size;
		
		/* Convert the bytes into a human readable format. */
		if (bytes >= 10L * (1 << 30)) sprintf(pretty_size, "%dG", bytes / (1 << 30));
		else if (bytes >= (1 << 30)) sprintf(pretty_size, "%2.1fG", (float) (bytes / 1e9));
		else if (bytes >= 10 * (1 << 20)) sprintf(pretty_size, "%dM", bytes / (1 << 20));
		else if (bytes >= (1 << 20)) sprintf(pretty_size, "%2.1fM", (float) (bytes / 1e6));
		else if (bytes >= 10 * (1 << 10)) sprintf(pretty_size, "%dK", bytes / (1 << 10));
		else if (bytes >= (1 << 10)) sprintf(pretty_size, "%2.1fK", (float) (bytes / 1e3));
		else sprintf(pretty_size, "%d", bytes);
	} else {
		if (opt_block_scaler) {
			/* Calculate the blocks in block size units. */
			int blocks = size * S_BLKSIZE / output_block_size;
			
			/* Round up to the least. */
			if (blocks < 1) blocks = 1;
			
			/* Convert the blocks into a human readable format. */
			if (output_block_size == (1 << 30)) sprintf(pretty_size, "%dG", blocks);
			else if (output_block_size == (1 << 20)) sprintf(pretty_size, "%dM", blocks);
			else if (output_block_size == (1 << 10)) sprintf(pretty_size, "%dK", blocks);
			else sprintf(pretty_size, "%d", blocks);
		} else if (apparent_size) {
			/* Just print the size, in bytes, into the formatting buffer. */
			sprintf(pretty_size, "%d", size);
		} else {
			/* Check if the output block size is in units of 1M (1048576 bytes). */
			if (output_block_size == (1 << 20)) {
				int blocks = size * S_BLKSIZE / output_block_size;
				if (blocks < 1) blocks = 1;
				sprintf(pretty_size, "%d", blocks);
			} else {
				int blocks = size * S_BLKSIZE / DEFAULT_BLOCK_SIZE;
				sprintf(pretty_size, "%d", blocks);
			}
		}
	}
	
	/* Print the formatted size with some information. */
	printf("%-7s %s\n", pretty_size, info);
}

/* Returns a new inode structure with the specified value. */
struct inode *inode_create(ino_t value) {
	struct inode *new = malloc(sizeof(struct inode));
	new->value = value;
	new->next = NULL;
	return new;
}

/* Adds to the list of inodes a new inode with the specified value. */
void inode_add(ino_t value) {
	/* If the inode already exists, then don't add it again. */
	if (inode_contains(value)) return;
	
	struct inode *current = NULL;
	if (inodes == NULL) inodes = inode_create(value);
	else {
		current = inodes;
		while (current->next != NULL)
			current = current->next;
		current->next = inode_create(value);
	}
}

/* Checks if an inode structure exists in the linked-list of inodes. */
bool inode_contains(ino_t value) {
	struct inode *current = inodes;
	if (inodes == NULL) return false;
	for (; current != NULL; current = current->next)
		if (current->value == value) return true;
	return false;
}

/* Prints all the inodes in the inode linked-list. */
void inode_list() {
	struct inode *current = inodes;
	for (; current != NULL; current = current->next)
		printf("%d\n", (int) current->value);
}
