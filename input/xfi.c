#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <elf.h>


void map_section(int fd, Elf64_Shdr *shdr, const char *section_name, void *base_address) {
    void *buffer = malloc(shdr->sh_size);
    if (buffer == NULL) {
        perror("Failed to allocate buffer");
        return;
    }

    if (pread(fd, buffer, shdr->sh_size, shdr->sh_offset) != shdr->sh_size) {
        perror("Failed to read section content");
        free(buffer);
        return;
    }

    void *new_address = base_address + shdr->sh_addr;
    memcpy(new_address, buffer, shdr->sh_size);

    printf("Section %s mapped at %p, size %ld\n", section_name, new_address, (long)shdr->sh_size);
    free(buffer);
}

void map_process(const char* filename, void *base_address) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }

    Elf64_Ehdr ehdr;
    if (pread(fd, &ehdr, sizeof(ehdr), 0) != sizeof(ehdr)) {
        perror("Failed to read ELF header");
        close(fd);
        return;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file\n");
        close(fd);
        return;
    }

    Elf64_Shdr shstrtab_hdr;
    if (pread(fd, &shstrtab_hdr, sizeof(shstrtab_hdr), ehdr.e_shoff + ehdr.e_shstrndx * sizeof(Elf64_Shdr)) != sizeof(Elf64_Shdr)) {
        perror("Failed to read section header string table header");
        close(fd);
        return;
    }

    char *shstrtab = malloc(shstrtab_hdr.sh_size);
    if (!shstrtab) {
        perror("Failed to allocate memory for section header string table");
        close(fd);
        return;
    }

    if (pread(fd, shstrtab, shstrtab_hdr.sh_size, shstrtab_hdr.sh_offset) != shstrtab_hdr.sh_size) {
        perror("Failed to read section header string table");
        free(shstrtab);
        close(fd);
        return;
    }

    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr shdr;
        off_t shdr_offset = ehdr.e_shoff + i * sizeof(shdr);
        if (pread(fd, &shdr, sizeof(shdr), shdr_offset) != sizeof(shdr)) {
            perror("Failed to read section header");
            continue;
        }

        const char *section_name = shstrtab + shdr.sh_name;

        if (strcmp(section_name, ".rodata") == 0 || strcmp(section_name, ".data") == 0 || strcmp(section_name, ".text") == 0) {
            map_section(fd, &shdr, section_name, base_address);
        }
    }

    free(shstrtab);
    close(fd);
}

__attribute__((constructor))
void init() {
    printf("Loading process...\n");
    char binary_path[1024];
    ssize_t len = readlink("/proc/self/exe", binary_path, sizeof(binary_path) - 1);
    if (len != -1) {
        binary_path[len] = '\0';
        // Determine the size required for the new memory space
        size_t size = 1024 * 1024; // 1MB, adjust as necessary

        // Request specific base address for new memory space
        void *xfi_address = (void *)0x7f0000000000;

        // Map new memory space
        xfi_address = mmap(xfi_address, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (xfi_address == MAP_FAILED) {
            perror("Failed to allocate new memory space");
            return;
        }

        printf("New memory space allocated at %p\n", xfi_address);

        // Map the process data
        map_process(binary_path, xfi_address);

        // Verify the content, here you could add your own checks or operations
    } else {
        perror("Failed to get binary path");
    }
}

__attribute__((destructor))
void cleanup() {
    printf("Cleaning up process...\n");
}
