#include <stdio.h>
#include <unistd.h>

int main();
//void foo() __attribute__ ((section (".sandbox1")));
//void bar() __attribute__ ((section (".sandbox2")));  
void foo();
void bar();
void baz();


void foo() {
    // Pointer to store the current instruction address
    void* current_address;

    // Inline assembly to get the current instruction pointer
    // 'lea' (Load Effective Address) loads the address of the next instruction into current_address
    __asm__("lea (%%rip), %0" : "=r"(current_address));

    printf("Function foo is at address: %p\n", (void*)&foo);
    printf("Current IP in foo: %p %p\n", current_address, current_address);
    /*
        30e5:	55                   	push   %rbp
        30e6:	48 89 e5             	mov    %rsp,%rbp
    */
    printf("From foo: 0x%08x\n", (*(unsigned int*)main));
    bar();
}


void bar() {
    // Pointer to store the current instruction address
    void* current_address;

    // Inline assembly to get the current instruction pointer
    // 'lea' (Load Effective Address) loads the address of the next instruction into current_address
    __asm__("lea (%%rip), %0" : "=r"(current_address));

    printf("Current IP in bar: %p\n", current_address);
    /*
        30e5:	55                   	push   %rbp
        30e6:	48 89 e5             	mov    %rsp,%rbp
    */
    printf("From bar: %p\n", (*foo));
    baz();
}

void baz() {
    printf("From baz: %p\n", (*baz));
}


int main()
{
    printf("Hello World!\n");
    foo();
    // char command[256];
    // sprintf(command, "cat /proc/%d/maps", getpid());
    // system(command);  // Display memory mapping for current process
    return 0;
}