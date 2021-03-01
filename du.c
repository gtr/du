/*
 * du.c 
 * Usage: du [OPTION]... [FILE]...
 * Summarize disk usage of the set of FILEs, recursively for directories.
 *  
 * Option:
 * -a   --all   write counts for all files, not just directories 
 */


#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define TRUE 1          
#define FALSE 0
#define INODES_CAPACITY 16  // An arbitriary capacity 

/* visited_inodes dynamically stores the inodes numbers we've visited */
typedef struct {
    int     num;    // Number of inodes stored
    int     cap;    // Capacity of our array
    ino_t*  arr;    // Pointer to the start of our array
} visited_inodes;

/*
 * addInode adds an inode to the array of visited inodes and dynamically 
 * reallocates memory if neccesary.
 * 
 * visited: the struct containing information about the visited inodes
 * inode:   the inode to be added
 */
void addInode(visited_inodes* visited, ino_t inode) {
    visited->arr[visited->num++] = inode;
    
    // Resize the array if we've reached capacity.
    if (visited->num == visited->cap) {
        visited->cap *= 2;
        ino_t* newArr = realloc(visited->arr, visited->cap * sizeof(ino_t));
        if (newArr == NULL) {
            printf("addInode(): error reallocing");
            exit(1);
        }
        visited->arr = newArr;
    }
}

/*
 * isInodeVisited returns TRUE if the given inode is found in the array of 
 * visited inodes and else otherwise.
 */
int isInodeVisited(visited_inodes* visited, ino_t inode) {
    for (int i = 0; i < visited->num; i++) {
        if (inode == visited->arr[i])
            return TRUE;
    }
    return FALSE;
}

void printUsage() {
    printf("Usage: du [DIRECTORY]\n");
    printf("Recursively summarize disk usage for directories.\n");
}

void printSize(char* path, size_t size) {
    printf("%zu\t%s\n", size, path);
}

/*
 * createPath creates the current full_path using the parent_path and the
 * child_path with a "/" in the middle.
 */
char* createPath(char* parent_path, char* child_path) {
    // Allocate space for full_path.
    int parent_len = strlen(parent_path);
    int child_len = strlen(child_path);
    char* full_path = calloc(parent_len + child_len + 1, sizeof(char*));
    
    // Populate full_path.
    strncat(full_path, parent_path, parent_len);
    strncat(full_path, "/", 1);
    strncat(full_path, child_path, child_len);

    return full_path;
}

/*
 * getFileSize returns the size of a file given its path and makes sure not to
 * count soft links and not to double count hard links.
 * 
 * file_stat: the stat struct for a file
 * visited: the struct containing information about the visited inodes
 * 
 * returns: the size of the file
 */
int getFileSize(struct stat file_stat, visited_inodes* visited) {
    if (file_stat.st_nlink > 1) {
        if (isInodeVisited(visited, file_stat.st_ino)) 
            return 0;
        addInode(visited, file_stat.st_ino);
    }
    return file_stat.st_blocks / 2;
}

/*
 * getDirectorySize recursively finds the size of a given directory.
 *
 * path:        the current directory path
 * visited:     the struct containing information about the visited inodes
 * all_files:   if the "-a" option was used
 * 
 * returns: the total size of the directory
 */
int getDirectorySize(char* path, visited_inodes* visited, int all_files) {
    int errno = 0;      // Error number for readdir().
    size_t total = 0;   // Total size in the directory.
    
    DIR *dirp = opendir(path);
    struct dirent *dir_entry = readdir(dirp);
    for (; dir_entry != NULL && errno == 0; dir_entry = readdir(dirp)) {
        char* entry_name = dir_entry->d_name;
        
        // Skip "..", (the directory above) to avoid cycles.
        if (strcmp("..", entry_name) == 0) continue;

        // Create the current path of the entry and get its `stat` struct.
        char* curr_path = createPath(path, entry_name);
        struct stat entry_stat;
        lstat(curr_path, &entry_stat);

        // Skip "." (the currrent directory)
        if (strcmp(".", entry_name) == 0) {
            total += entry_stat.st_blocks / 2;
            continue;
        }
        if (S_ISDIR(entry_stat.st_mode)) {
            // Find the size for the current directory entry.
            size_t size = getDirectorySize(curr_path, visited, all_files);
            printSize(curr_path, size);
            total += size;
        } else if (S_ISREG(entry_stat.st_mode)) {
            // Find the size for the current file entry.
            size_t size = getFileSize(entry_stat, visited);
            if (all_files == TRUE) {
                printSize(curr_path, size);
            }
            total += size;
        }

        free(curr_path);
    }

    return total;
}

/*
 * trimPath trims excess trailing "/" from the input path.
 * returns TRUE if there was at least one "/".
 */
int trimPath(char* path) {
    int len = strlen(path);
    int slash = FALSE;
    for (int i = len-1; i > 0 && strcmp(&path[i], "/") == 0; i--) {
        path[i] = '\0';
        slash = TRUE;
    }
    return slash;
}

int doesPathExist(char* path) {
    struct stat path_stat;
    return stat(path, &path_stat) == 0;
}

/*
 * parseOptions returns TRUE if the -a, --all option was used
 */
int parseOptions(int argc, char* argv[]) {
    if (argc == 3) {
        if (strcmp(argv[2], "-a") == 0 || strcmp(argv[2], "--all") == 0)
            return TRUE;
        if (strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--all") == 0)
            return TRUE;
    } else if (argc == 2) {
        if (strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "--all") == 0)
            return TRUE;
    }
    return FALSE;
}

char* parsePath(int argc, char* argv[]) {
    if (argc == 3) {
        if (strcmp(argv[2], "-a") != 0 && strcmp(argv[2], "--all") != 0)
            return argv[2];
        if (strcmp(argv[1], "-a") != 0 && strcmp(argv[1], "--all") != 0)
            return argv[1];
    } else if (argc == 2) {
        if (strcmp(argv[1], "-a") != 0 && strcmp(argv[1], "--all") != 0)
            return argv[1];
    }
    return ".";
}

int main(int argc, char* argv[]) {
    if (argc > 3) {
        printUsage();
        exit(0);
    }
    int all_files = parseOptions(argc, argv);
    char* path = parsePath(argc, argv);

    // Trim path and check if it exists.
    int slash = trimPath(path);
    if (!doesPathExist(path)) {
        printf("Error: path does not exist\n");
        exit(1);
    }

    // Initialize our visited_inodes struct and allocate space for the array 
    // of visited inodes.
    visited_inodes visited = { 
        0,  INODES_CAPACITY,  calloc(INODES_CAPACITY, sizeof(ino_t))
    };
    
    // Get the size of the directory.
    int size = getDirectorySize(path, &visited, all_files);

    // Concatenate a "/" to the path if necessary/
    if (slash == TRUE) {
        strncat(path, "/", 1);
    }

    // Print the size of the directory.
    printSize(path, size);
    free(visited.arr);
}
