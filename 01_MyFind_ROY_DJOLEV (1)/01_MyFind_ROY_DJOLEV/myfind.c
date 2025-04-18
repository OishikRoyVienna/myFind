#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH_LEN 1024
#define MAX_FILENAME_LEN 256

// function to search for a specific file in a given path
void searchFile(char *fullPath, char *fileName, int caseInsensitive, int *fileFound)
{
    struct stat statbuf; // statbuf stores file information (filetype, size,..)
    if (stat(fullPath, &statbuf) == -1) // check if stat function got information about "fullPath" 
    { 
        return;
    }

    // check if it is a regular file and matches the specified file name 
    // case insensitive logic
    if (S_ISREG(statbuf.st_mode) && 
        ((caseInsensitive && strcasecmp(fileName, strrchr(fullPath, '/') + 1) == 0) ||
         (!caseInsensitive && strcmp(fileName, strrchr(fullPath, '/') + 1) == 0)))
    {
        printf("%d: %s: %s\n", getpid(), fileName, fullPath);
        *fileFound = 1;
    }
}

// function to recursively search in a directory
void searchDirectory(char *searchPath, char *fileName, int recursive, int caseInsensitive, int *fileFound)
{
    DIR *dir = opendir(searchPath); // check if the directory can be opened
    if (dir == NULL)
    {
        return;
    }

    struct dirent *entry;
    // iterate through the directory entries
    while ((entry = readdir(dir)) != NULL)
    {
        // check if the current directory entry is either "." or ".." and skips it to prevent unnecessary processing
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        char fullPath[MAX_PATH_LEN];
        // construct the full path of the entry
        snprintf(fullPath, MAX_PATH_LEN, "%s/%s", searchPath, entry->d_name);

        struct stat statbuf; // declares statbuf for file info, if stat fails for fullPath it continues to the next iteration
        // get file information
        if (stat(fullPath, &statbuf) == -1)
        {
            continue;
        }
        // recursive logic 
        if (S_ISDIR(statbuf.st_mode) && recursive)
        {
            searchDirectory(fullPath, fileName, recursive, caseInsensitive, fileFound);
        }
        else
        {
            // search for the file in the current directory
            searchFile(fullPath, fileName, caseInsensitive, fileFound);
        }
    }

    closedir(dir);
}

/*
 * The main program forks a child process for each filename provided in the command-line arguments
 * and looks for the file within the specified search path. If a file is found, an entry in the
 * following output format is printed to stdout:
 *
 *     <pid>: <filename>: <complete-path-to-found-file>
 *
 * - <pid>: process-id of the child process that finds the entry.
 * - <filename>: references the filename that was passed into the main program as an argument.
 * - <complete-path-to-found-file>: absolute path to the found file.
 *
 * This behavior is achieved by using fork() to create child processes, where each child process
 * is responsible for searching for a specific file. The search results are printed in the specified
 * format from within the searchDirectory function.
 */

int main(int argc, char *argv[])
{
    int opt;
    int recursive = 0;
    int caseInsensitive = 0;
    int fileFound = 0;
    int foundInCurrentProcess = 0;

    // parse command line options
    while ((opt = getopt(argc, argv, "Ri")) != -1)
    {
        switch (opt)
        {
            case 'R':
                recursive = 1;
                break;
            case 'i':
                caseInsensitive = 1;
                break;
            default:
                // print usage information for invalid options
                fprintf(stderr, "Usage: %s [-R] [-i] searchpath filename1 [filename2] ... [filenameN]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // check if there are enough command line arguments
    if (optind + 2 > argc)
    {
        fprintf(stderr, "Insufficient arguments\n");
        exit(EXIT_FAILURE);
    }

    char *searchPath = argv[optind++];

    // iterate through the specified file names
    while (optind < argc)
    {
        char *fileName = argv[optind++];

        // fork a new process
        pid_t child_pid = fork();

        // check if fork was successful
        if (child_pid == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        // check if process is the child
        if (child_pid == 0)
        {
            // in the child process, search for files in the directory
            searchDirectory(searchPath, fileName, recursive, caseInsensitive, &foundInCurrentProcess);
            exit(foundInCurrentProcess);
        }
        else
        {
            // in the parent process, wait for the child process to complete
            int status;
            waitpid(child_pid, &status, 0);

            // check if files were found in the child process
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            {
                fileFound = 1;
                printf("Process ID %d was terminated.\n", child_pid);
            }
            else if (WIFEXITED(status) && WEXITSTATUS(status) == 0 && fileFound == 0)
            {
                fileFound = 0;
            }
        }
    }

    // output if no files were found
    if (!fileFound)
    {
        printf("No files in %s found\n", searchPath);
    }

    return 0;
}