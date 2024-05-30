#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

void map_process(const char* filename) {
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }

    // Get the file size
    off_t filesize = lseek(fd, 0, SEEK_END);
    if (filesize == -1) {
        perror("Failed to get file size");
        close(fd);
        return;
    }

    // Reset the file offset to the beginning of the file
    lseek(fd, 0, SEEK_SET);

    // Allocate a buffer to read the file content
    void* buffer = malloc(filesize);
    if (buffer == NULL) {
        perror("Failed to allocate buffer");
        close(fd);
        return;
    }

    // Read the file content into the buffer
    ssize_t bytes_read = read(fd, buffer, filesize);
    if (bytes_read != filesize) {
        perror("Failed to read file content");
        free(buffer);
        close(fd);
        return;
    }

    // Allocate a new memory region for writing
    void* new_mapped = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_mapped == MAP_FAILED) {
        perror("Failed to allocate new memory area");
        free(buffer);
        close(fd);
        return;
    }

    // Copy the content from the buffer to the new memory region
    memcpy(new_mapped, buffer, filesize);

    // Optionally, change memory protection of the new memory area to READ | EXECUTE if it contains code
    if (mprotect(new_mapped, filesize, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect failed");
        munmap(new_mapped, filesize);
        free(buffer);
        close(fd);
        return;
    }

    printf("File %s mapped at %p, size %ld\n", filename, new_mapped, filesize);
    // Verify the content
    if (memcmp(new_mapped, buffer, filesize) == 0) {
        printf("Content copied successfully!\n");
    } else {
        printf("Content mismatch!\n");
    }
    // Clean up the buffer and close the file descriptor
    free(buffer);
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
