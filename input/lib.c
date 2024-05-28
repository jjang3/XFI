#include <stdio.h>
#include <elf.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/auxv.h>
#include <unistd.h>
#include <gelf.h>
#include <immintrin.h>
/* Will be eventually in asm/hwcap.h */
#ifndef HWCAP2_FSGSBASE
#define HWCAP2_FSGSBASE        (1 << 1)
#endif
#define _GNU_SOURCE

typedef struct mapped_section {
    void *addr;
    size_t size;
    struct mapped_section *next;
} mapped_section;

mapped_section *head = NULL;
mapped_section *tail = NULL; // Tail of the linked list

int sandbox_counts = 0;

void map_sandbox_sections(const char* filename) {
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return;
    }

    // Fetch the system's page size, used for aligning mmap offsets
    long pagesize = sysconf(_SC_PAGESIZE);

    // Initialize the libelf library; required before calling any other libelf functions
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "Libelf initialization failed: %s\n", elf_errmsg(-1));
        close(fd);
        return;
    }

    // Begin reading an ELF file, creating an Elf pointer from the file descriptor
    Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
    if (!elf) {
        fprintf(stderr, "Failed to read ELF file: %s\n", elf_errmsg(-1));
        close(fd);
        return;
    }

    // Retrieve the index of the section header string table (shstrndx) of the ELF file
    size_t shstrndx;
    if (elf_getshdrstrndx(elf, &shstrndx) != 0) {
        fprintf(stderr, "Failed to get section header string table index: %s\n", elf_errmsg(-1));
        elf_end(elf);  // Clean up the Elf descriptor
        close(fd);
        return;
    }

    // Traverse all sections of the ELF file
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {  // elf_nextscn iterates over sections
        if (gelf_getshdr(scn, &shdr) != NULL) {  // Retrieve the section header for the current section
            // Get the name of the section from the section header string table
            const char *name = elf_strptr(elf, shstrndx, shdr.sh_name);
            // Check if the section name starts with ".sandbox"
            if (name && strncmp(name, ".sandbox", 8) == 0) {
                printf("Mapping section: %s\n", name);
                // Align the section offset to the system's page size
                /*
                shdr.sh_offset: This is the offset in bytes of the section from the start of the file. 
                                It indicates where the section begins within the ELF file.
                pagesize:   This is the size of a page in memory, as returned by sysconf(_SC_PAGESIZE).
                            Page sizes are typically 4 KB on most systems but can vary.
                
                The calculation involves two steps:
                Div. by pagesize:   shdr.sh_offset / pagesize calculates how many complete pages fit into the offset. 
                                    This division truncates any remainder because both shdr.sh_offset and pagesize are size_t (integer type), so the result is an integer number of pages.
                Mult. by pagesize:  The result from the division is then multiplied by pagesize, giving the highest multiple of pagesize 
                                    that is less than or equal to shdr.sh_offset. 
                                    This results in an aligned address that is appropriate for use with mmap().
                */
                size_t aligned_offset = (shdr.sh_offset / pagesize) * pagesize;

                // Map the section into memory
                /*
                    shdr.sh_size + (shdr.sh_offset - aligned_offset) (second parameter): This defines the total size of the memory to be mapped. 
                    Since the offset used in mmap() must be aligned to a page boundary, this size compensates for any bytes before the section start 
                    within its starting page. The formula ensures that enough memory is mapped to include the entire section:
                    - shdr.sh_offset - aligned_offset:  This is the number of bytes from the start of the aligned page to 
                                                        the actual start of the section data.
                    - shdr.sh_size: This is the actual size of the section.
                */
                void *addr = mmap(  NULL, shdr.sh_size + (shdr.sh_offset - aligned_offset), PROT_READ | PROT_WRITE, 
                                    MAP_ANONYMOUS | MAP_32BIT | MAP_PRIVATE, fd, aligned_offset);
                if (addr == MAP_FAILED) {
                    perror("mmap failed");
                } else {
                    sandbox_counts += 1;
                    printf("Section %s mapped at %p, size: %lu\n", name, addr, shdr.sh_size);
                    // Compute the actual address where the section data begins
                    char *actual_addr = (char*)addr + (shdr.sh_offset - aligned_offset);
                    printf("Actual section data starts at %p\n", actual_addr);

                    // Save the mapped section information for later unmapping or usage
                    mapped_section *new_section = malloc(sizeof(mapped_section));
                    new_section->addr = addr;
                    new_section->size = shdr.sh_size + (shdr.sh_offset - aligned_offset);
                    new_section->next = NULL;

                    if (head == NULL) {
                        head = new_section;
                        tail = new_section;
                    } else {
                        tail->next = new_section;
                        tail = new_section;
                    }
                }
            }
        }
    }

    elf_end(elf);  // Close the Elf descriptor
    close(fd);     // Close the file descriptor
}

void **table;

void create_table()
{
    table = malloc(sizeof(void*)*sandbox_counts);

    if (!table) {
        perror("Failed to allocate memory for page table");
        exit(EXIT_FAILURE);
    }
    mapped_section *current = head;
    for (int i = 0; i < sandbox_counts; ++i) {
        table[i] = current->addr;
        current = current->next;
    }
	_writegsbase_u64((long long unsigned int)table);
}

__attribute__((constructor))
void init() {
    printf("Initializing library...\n");
    char binary_path[1024];
    ssize_t len = readlink("/proc/self/exe", binary_path, sizeof(binary_path) - 1);
    if (len != -1) {
        binary_path[len] = '\0';
        map_sandbox_sections(binary_path);
    } else {
        perror("Failed to get binary path");
    }
}

__attribute__((destructor))
void cleanup() {
    printf("Cleaning up library...\n");
    while (head) {
        if (munmap(head->addr, head->size) == -1) {
            perror("munmap failed");
        }
        mapped_section *temp = head;
        head = head->next;
        free(temp);
    }
    free(table);
}
