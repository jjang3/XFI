SECTIONS
{  
    .text BLOCK(4096) : ALIGN(4096)
    {
        __text_start = .;
            *(.text)
        __text_end = .;
    }
    .fini : 
    {
        __fini_begin = .;
            *(.fini)
        __fini_end = .;
    } 
    .sandbox : AT(__fini_end)
    {
        _start_sandbox = .;
            *(.sandbox*)
        _end_sandbox = .;
    } 
} INSERT BEFORE .eh_frame_hdr;