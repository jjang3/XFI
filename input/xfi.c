#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <immintrin.h>
#include <string.h>
#include <elf.h>
#include <sys/auxv.h>

/* Will be eventually in asm/hwcap.h */
#ifndef HWCAP2_FSGSBASE
#define HWCAP2_FSGSBASE        (1 << 1)
#endif
#define PAGE_SIZE 4096
#define FUN_COUNTS 10

void **table;

extern unsigned long long data_section_offset = 0; // Define the external variable


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
            printf("%s section offset: 0x%lx\n", section_name, section_offset);
            break;
        }
    }

    free(shstrtab);
    close(fd);
    return section_offset;
}

const char* intrinsic_hidden_symbols[] = {
    "__dso_handle",     // Used by the C++ runtime for dynamic shared object (DSO) management
    "__TMC_END__",      // Used by the runtime for thread-local storage (TLS) management.
    "__cxa_atexit",     // Used by the C++ runtime for managing destructors of static objects
    "_ITM_deregisterTMCloneTable",  // Used by the GCC runtime for transactional memory.
    "_ITM_registerTMCloneTable",    //  Also used by the GCC runtime for transactional memory.
    "__stack_chk_guard",    // Used for stack smashing protection.
    "__stack_chk_fail",     // Used for stack smashing protection.
    NULL // Null-terminated array
};

bool is_intrinsic_hidden_symbol(const char* symbol_name) {
    for (int i = 0; intrinsic_hidden_symbols[i] != NULL; i++) {
        if (strcmp(symbol_name, intrinsic_hidden_symbols[i]) == 0) {
            return true;
        }
    }
    return false;
}

void print_section_content(void *address, size_t size) {
    unsigned char *data = (unsigned char *)address;
    printf("Section content at %p:\n", address);
    for (size_t i = 0; i < size; i++) {
        printf("Offset %04lx: 0x%02x\n", i, data[i]);
    }
}

int table_entries;

int get_hidden_symbol_size(const char *symbol_name, Elf64_Sym *sym) {
    if (ELF64_ST_VISIBILITY(sym->st_other) == STV_HIDDEN) {
        printf("Found hidden symbol %ld\n", sym->st_size);
        if (sym->st_size == 0) {
            // If size is 0, then assume 8 bytes of offset address will be used.
            return 8;
        }
        return sym->st_size;
    }
    return 0;
}
void map_section(int fd, Elf64_Shdr *shdr, const char *section_name, void *base_address, Elf64_Shdr *symtab_hdr, Elf64_Shdr *strtab_hdr) {
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
    printf("Base address: %p, Section address: %p\n", base_address, new_address);

    if ((char *)new_address + shdr->sh_size > (char *)base_address + 1024 * 1024) {
        fprintf(stderr, "Section %s exceeds allocated memory space\n", section_name);
        free(buffer);
        return;
    }

    memcpy(new_address, buffer, shdr->sh_size);
    printf("Section %s mapped at %p, offset %ld, size %ld, aligned at %p\n", section_name, new_address, (long)shdr->sh_offset, (long)shdr->sh_size, new_address);

    if (strcmp(section_name, ".text") == 0) {
        table[0] = new_address;
    } 
    else if (strcmp(section_name, ".data") == 0) {
        uintptr_t hidden_offset = 0;
        if (symtab_hdr && strtab_hdr) {
            int num_symbols = symtab_hdr->sh_size / sizeof(Elf64_Sym);
            printf("Number of symbols: %d\n", num_symbols);
            
            Elf64_Sym *symtab = malloc(symtab_hdr->sh_size);
            if (symtab == NULL) {
                perror("Failed to allocate memory for symbol table");
                free(buffer);
                return;
            }

            if (pread(fd, symtab, symtab_hdr->sh_size, symtab_hdr->sh_offset) != symtab_hdr->sh_size) {
                perror("Failed to read symbol table");
                free(symtab);
                free(buffer);
                return;
            }

            char *strtab = malloc(strtab_hdr->sh_size);
            if (strtab == NULL) {
                perror("Failed to allocate memory for string table");
                free(symtab);
                free(buffer);
                return;
            }

            if (pread(fd, strtab, strtab_hdr->sh_size, strtab_hdr->sh_offset) != strtab_hdr->sh_size) {
                perror("Failed to read string table");
                free(strtab);
                free(symtab);
                free(buffer);
                return;
            }

            for (int i = 0; i < num_symbols; i++) {
                if (ELF64_ST_VISIBILITY(symtab[i].st_other) == STV_HIDDEN) {
                    const char *symbol_name = strtab + symtab[i].st_name;
                    size_t symbol_size = symtab[i].st_size ? symtab[i].st_size : 8;
                    if (is_intrinsic_hidden_symbol(symbol_name) 
                        && strcmp(symbol_name, "__TMC_END__") != 0) { // We avoid TMC_END
                        printf("Found intrinsic hidden symbol: %s, size: %lu bytes\n", symbol_name, symbol_size);
                        hidden_offset += 8;
                    } else {
                        printf("Found artificial hidden symbol: %s, size: %lu bytes\n", symbol_name, symbol_size);
                    }
                }
            }

            free(strtab);
            free(symtab);
        } else {
            printf("Symbol table or string table header is missing\n");
        }
        table[1] = new_address + hidden_offset;
        if (hidden_offset > 0) {
            printf("Address updated: %p\n", table[1]);
        }
    } 
    else if (strcmp(section_name, ".rodata") == 0) {
        table[2] = new_address;
    } 
    else if (strcmp(section_name, ".bss") == 0) {
        table[3] = new_address;
    }
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

    Elf64_Shdr symtab_hdr;
    Elf64_Shdr strtab_hdr;
    int found_symtab = 0;
    int found_strtab = 0;

    // First pass to find the .symtab and .strtab sections
    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr shdr;
        off_t shdr_offset = ehdr.e_shoff + i * sizeof(shdr);
        if (pread(fd, &shdr, sizeof(shdr), shdr_offset) != sizeof(shdr)) {
            perror("Failed to read section header");
            continue;
        }

        const char *section_name = shstrtab + shdr.sh_name;

        if (strcmp(section_name, ".symtab") == 0) {
            symtab_hdr = shdr;
            found_symtab = 1;
        } else if (strcmp(section_name, ".strtab") == 0) {
            strtab_hdr = shdr;
            found_strtab = 1;
        }

        // If both .symtab and .strtab are found, no need to continue this loop
        if (found_symtab && found_strtab) {
            break;
        }
    }

    // Second pass to map sections
    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr shdr;
        off_t shdr_offset = ehdr.e_shoff + i * sizeof(shdr);
        if (pread(fd, &shdr, sizeof(shdr), shdr_offset) != sizeof(shdr)) {
            perror("Failed to read section header");
            continue;
        }

        const char *section_name = shstrtab + shdr.sh_name;

        if (strcmp(section_name, ".rodata") == 0 || strcmp(section_name, ".data") == 0 || strcmp(section_name, ".text") == 0 || strcmp(section_name, ".bss") == 0) {
            if (found_symtab && found_strtab) {
                map_section(fd, &shdr, section_name, base_address, &symtab_hdr, &strtab_hdr);
            } else {
                map_section(fd, &shdr, section_name, base_address, NULL, NULL);
            }
        }
    }

    free(shstrtab);
    close(fd);
}

void __attribute__((constructor)) create_table() {
    // 4 are reserved sections
    table_entries = 4 + FUN_COUNTS;
    table = malloc(sizeof(void*) * table_entries);
    if (!table) {
        perror("Failed to allocate memory for table");
        exit(EXIT_FAILURE);
    }
    printf("Address to the table: %p\n", table);
    // Pointer to shared memory region
    // Map each page
    for (int i = 0; i < table_entries; ++i) {
        table[i] = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_32BIT | MAP_PRIVATE, -1, 0);
        if (table[i] == MAP_FAILED) {
            perror("Memory mapping failed");
            // Clean up previously mapped pages
            for (int j = 0; j < i; ++j) {
                munmap(table[j], PAGE_SIZE);
            }
            free(table);
            exit(EXIT_FAILURE);
        }
    }

    _writegsbase_u64((long long unsigned int)table);

    printf("Loading process...\n");
    char binary_path[1024];
    ssize_t len = readlink("/proc/self/exe", binary_path, sizeof(binary_path) - 1);
    if (len != -1) {
        binary_path[len] = '\0';
        // Determine the size required for the new memory space
        size_t size = 1024 * 1024; // 1MB, adjust as necessary

        // Request specific base address for new memory space
        void *xfi_address = (void *)0x100000000000;

        // Map new memory space
        xfi_address = mmap(xfi_address, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (xfi_address == MAP_FAILED) {
            perror("Failed to allocate new memory space");
            return;
        }
        data_section_offset = get_section_offset(binary_path, ".data");
        
        printf("New memory space allocated at %p\n", xfi_address);
        
        // Map the process data
        map_process(binary_path, xfi_address);
    } else {
        perror("Failed to get binary path");
    }
}

void __attribute__((destructor)) cleanup_table() {
    // Unmap each page and free the table
    for (int i = 0; i < table_entries; ++i) {
        if (table[i]) {
            munmap(table[i], PAGE_SIZE);
        }
    }
    free(table);
}
