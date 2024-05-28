#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

// Function pointers for the original functions
void* (*original_mmap)(void*, size_t, int, int, int, off_t);
int (*original_mprotect)(void*, size_t, int);

// Our custom mmap function
void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    // Call the original mmap function
    void* result = original_mmap(addr, length, prot, flags, fd, offset);
    
    // Custom behavior: Print a message
    printf("Custom mmap: addr=%p, length=%zu, prot=%d, flags=%d, fd=%d, offset=%ld\n",
           result, length, prot, flags, fd, offset);

    return result;
}

// Our custom mprotect function
int mprotect(void* addr, size_t len, int prot) {
    // Call the original mprotect function
    int result = original_mprotect(addr, len, prot);
    
    // Custom behavior: Print a message
    printf("Custom mprotect: addr=%p, len=%zu, prot=%d\n", addr, len, prot);

    return result;
}

void map_process(const char* filename) {
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }

    // Fetch the system's page size, used for aligning mmap offsets
    long pagesize = sysconf(_SC_PAGESIZE);

    // Get the file size
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("Failed to get file size");
        close(fd);
        return;
    }
    size_t filesize = st.st_size;

    // Map the file into memory
    void* mapped = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("Failed to mmap file");
        close(fd);
        return;
    }

    printf("File %s mapped at %p, size %zu\n", filename, mapped, filesize);

    // Example of changing protection of the mapped memory
    if (mprotect(mapped, filesize, PROT_READ | PROT_WRITE) < 0) {
        perror("Failed to mprotect memory");
        munmap(mapped, filesize);
        close(fd);
        return;
    }

    printf("Memory protection changed to READ | WRITE\n");

    // Close the file descriptor
    close(fd);
}

__attribute__((constructor))
void init() {
    printf("Loading process...\n");
    char binary_path[1024];
    ssize_t len = readlink("/proc/self/exe", binary_path, sizeof(binary_path) - 1);
    if (len != -1) {
        binary_path[len] = '\0';
        map_process(binary_path);
    } else {
        perror("Failed to get binary path");
    }
}

__attribute__((destructor))
void cleanup() {
    printf("Cleaning up process...\n");
}

// Constructor to initialize function pointers
__attribute__((constructor))
void init_intercept() {
    original_mmap = dlsym(RTLD_NEXT, "mmap");
    original_mprotect = dlsym(RTLD_NEXT, "mprotect");
    if (!original_mmap || !original_mprotect) {
        fprintf(stderr, "Error loading original functions: %s\n", dlerror());
        _exit(1);
    }
}
