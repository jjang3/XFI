#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <elf.h>
#include <immintrin.h>

#define PAGE_SIZE 4096

void *xfi_base_address = NULL;  // Global variable to store base address
size_t xfi_size = 0;            // Global variable to store size of the mapped region

unsigned long long get_section_offset(const char *filename, const char *section_name) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return -1;
    }

    Elf64_Ehdr ehdr;
    if (pread(fd, &ehdr, sizeof(ehdr), 0) != sizeof(ehdr)) {
        perror("Failed to read ELF header");
        close(fd);
        return -1;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file\n");
        close(fd);
        return -1;
    }

    Elf64_Shdr shstrtab_hdr;
    if (pread(fd, &shstrtab_hdr, sizeof(shstrtab_hdr), ehdr.e_shoff + ehdr.e_shstrndx * sizeof(Elf64_Shdr)) != sizeof(Elf64_Shdr)) {
        perror("Failed to read section header string table header");
        close(fd);
        return -1;
    }

    char *shstrtab = malloc(shstrtab_hdr.sh_size);
    if (!shstrtab) {
        perror("Failed to allocate memory for section header string table");
        close(fd);
        return -1;
    }

    if (pread(fd, shstrtab, shstrtab_hdr.sh_size, shstrtab_hdr.sh_offset) != shstrtab_hdr.sh_size) {
        perror("Failed to read section header string table");
        free(shstrtab);
        close(fd);
        return -1;
    }

    unsigned long long section_offset = 0;
    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr shdr;
        if (pread(fd, &shdr, sizeof(shdr), ehdr.e_shoff + i * sizeof(shdr)) != sizeof(shdr)) {
            perror("Failed to read section header");
            continue;
        }

        const char *name = shstrtab + shdr.sh_name;
        if (strcmp(name, section_name) == 0) {
            section_offset = shdr.sh_offset;
            printf("%s section offset: 0x%llx\n", section_name, section_offset);
            break;
        }
    }

    free(shstrtab);
    close(fd);
    return section_offset;
}

void print_section_content(void *address, size_t size) {
    unsigned char *data = (unsigned char *)address;
    printf("Section content at %p:\n", address);
    for (size_t i = 0; i < size; i++) {
        printf("Offset %04lx: 0x%02x\n", i, data[i]);
    }
}

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
    uintptr_t section_offset = (uintptr_t)shdr->sh_offset;

    void *new_address = base_address + section_offset;
    printf("Base address: %p, Section address: %p\n", base_address, new_address);

    if ((char *)new_address + shdr->sh_size > (char *)base_address + 1024 * 1024) {
        fprintf(stderr, "Section %s exceeds allocated memory space\n", section_name);
        free(buffer);
        return;
    }
    
    memcpy(new_address, buffer, shdr->sh_size);
    printf("Section %s mapped at %p, offset %lx, size %lx, aligned at %p\n", section_name, new_address, (long)shdr->sh_offset, (long)shdr->sh_size, new_address);

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

    // Map each section to the new address
    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr shdr;
        off_t shdr_offset = ehdr.e_shoff + i * sizeof(shdr);
        if (pread(fd, &shdr, sizeof(shdr), shdr_offset) != sizeof(shdr)) {
            perror("Failed to read section header");
            continue;
        }

        const char *section_name = shstrtab + shdr.sh_name;

        if (strcmp(section_name, ".rodata") == 0 || strcmp(section_name, ".data") == 0 || strcmp(section_name, ".text") == 0 || strcmp(section_name, ".bss") == 0) {
            map_section(fd, &shdr, section_name, base_address);
        }
    }

    free(shstrtab);
    close(fd);
}

void __attribute__((constructor)) create_xfi() {
    printf("Loading process...\n");
    char binary_path[1024];
    ssize_t len = readlink("/proc/self/exe", binary_path, sizeof(binary_path) - 1);
    if (len != -1) {
        binary_path[len] = '\0';
        // Determine the size required for the new memory space
        xfi_size = 1024 * 1024; // 1MB, adjust as necessary

        // Request specific base address for new memory space
        xfi_base_address = (void *)0x100000000000;

        // Map new memory space
        xfi_base_address = mmap(xfi_base_address, xfi_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (xfi_base_address == MAP_FAILED) {
            perror("Failed to allocate new memory space");
            return;
        }
        _writegsbase_u64((long long unsigned int)xfi_base_address);
        printf("New memory space allocated at %p\n", xfi_base_address);
        
        // Map the process data
        map_process(binary_path, xfi_base_address);
    } else {
        perror("Failed to get binary path");
    }
}

void __attribute__((destructor)) cleanup_xfi() {
    // Perform cleanup by unmapping the allocated memory space
    if (xfi_base_address != NULL && xfi_size > 0) {
        if (munmap(xfi_base_address, xfi_size) == -1) {
            perror("Failed to unmap memory");
        } else {
            printf("Memory successfully unmapped\n");
        }
    }
}
