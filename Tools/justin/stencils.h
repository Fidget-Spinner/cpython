// This is sort of an ideal for readability...

// This is just here so human readers can find the holes easily:
#define HOLE(SYMBOL) 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

// STORE_FAST
static const unsigned char STORE_FAST_stencil_bytes[] = {
// .text:
    // <_justin_entry>:
    0x50,                                              // pushq   %rax
    0x49, 0xbe, HOLE(_justin_next_instr),              // movabsq $0, %r14
    0x4c, 0x89, 0x75, 0x38,                            // movq    %r14, 56(%rbp)
    0x49, 0x8b, 0x44, 0x24, 0xf8,                      // movq    -8(%r12), %rax
    0x48, 0xb9, HOLE(_justin_oparg_0),                 // movabsq $0, %rcx
    0x48, 0x63, 0xc9,                                  // movslq  %ecx, %rcx
    0x48, 0x8b, 0x5c, 0xcd, 0x48,                      // movq    72(%rbp,%rcx,8), %rbx
    0x48, 0x89, 0x44, 0xcd, 0x48,                      // movq    %rax, 72(%rbp,%rcx,8)
    0x48, 0x85, 0xdb,                                  // testq   %rbx, %rbx
    0x74, 0x4f,                                        // je      0x7f <_justin_entry+0x7f>
    0x48, 0xb8, HOLE(_Py_DecRefTotal_DO_NOT_USE_THIS), // movabsq $0, %rax
    0xff, 0xd0,                                        // callq   *%rax
    0x48, 0x8b, 0x03,                                  // movq    (%rbx), %rax
    0x48, 0x89, 0xc1,                                  // movq    %rax, %rcx
    0x48, 0x83, 0xc1, 0xff,                            // addq    $-1, %rcx
    0x48, 0x89, 0x0b,                                  // movq    %rcx, (%rbx)
    0x74, 0x25,                                        // je      0x70 <_justin_entry+0x70>
    0x48, 0x85, 0xc0,                                  // testq   %rax, %rax
    0x7f, 0x2f,                                        // jg      0x7f <_justin_entry+0x7f>
    0x48, 0xbf, HOLE(.rodata.str1.1 + 168),            // movabsq $0, %rdi
    0x48, 0xb8, HOLE(_Py_NegativeRefcount),            // movabsq $0, %rax
    0xbe, 0xa0, 0x02, 0x00, 0x00,                      // movl    $672, %esi              # imm = 0x2A0
    0x48, 0x89, 0xda,                                  // movq    %rbx, %rdx
    0xff, 0xd0,                                        // callq   *%rax
    0xeb, 0x0f,                                        // jmp     0x7f <_justin_entry+0x7f>
    0x48, 0xb8, HOLE(_Py_Dealloc),                     // movabsq $0, %rax
    0x48, 0x89, 0xdf,                                  // movq    %rbx, %rdi
    0xff, 0xd0,                                        // callq   *%rax
    0x41, 0x0f, 0xb6, 0x46, 0x03,                      // movzbl  3(%r14), %eax
    0x48, 0x8b, 0x5c, 0xc5, 0x48,                      // movq    72(%rbp,%rax,8), %rbx
    0x48, 0xb8, HOLE(_Py_IncRefTotal_DO_NOT_USE_THIS), // movabsq $0, %rax
    0xff, 0xd0,                                        // callq   *%rax
    0x48, 0x83, 0x03, 0x01,                            // addq    $1, (%rbx)
    0x49, 0x89, 0x5c, 0x24, 0xf8,                      // movq    %rbx, -8(%r12)
    0x48, 0xb8, HOLE(_justin_continue),                // movabsq $0, %rax
    0x59,                                              // popq    %rcx
    0xff, 0xe0,                                        // jmpq    *%rax
// .rodata.str1.1:
    0x49, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x2f, // Include/
    0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x2e, 0x68, // object.h
    0x00,                                           // .
};
static const Hole STORE_FAST_stencil_holes[] = {
    {.offset =   3, .addend =   0, .kind = HOLE_next_instr                     },
    {.offset =  26, .addend =   0, .kind = HOLE_oparg                          },
    {.offset =  54, .addend =   0, .kind = LOAD__Py_DecRefTotal_DO_NOT_USE_THIS},
    {.offset =  86, .addend =   0, .kind = HOLE_continue                       },
    {.offset =  99, .addend =   0, .kind = LOAD__Py_Dealloc                    },
    {.offset = 114, .addend =   0, .kind = HOLE_continue                       },
    {.offset = 127, .addend = 168, .kind = HOLE_base                           },
    {.offset = 137, .addend =   0, .kind = LOAD__Py_NegativeRefcount           },
    {.offset = 155, .addend =   0, .kind = HOLE_continue                       },
};
static const Stencil STORE_FAST_stencil = {
    .nbytes = Py_ARRAY_LENGTH(STORE_FAST_stencil_bytes),
    .bytes = STORE_FAST_stencil_bytes,
    .nholes = Py_ARRAY_LENGTH(STORE_FAST_stencil_holes),
    .holes = STORE_FAST_stencil_holes,
};

// <trampoline>
static const unsigned char trampoline_stencil_bytes[] = {
// .text:
    // <_justin_trampoline>:
    0x55,                                           // pushq   %rbp
    0x41, 0x57,                                     // pushq   %r15
    0x41, 0x56,                                     // pushq   %r14
    0x41, 0x55,                                     // pushq   %r13
    0x41, 0x54,                                     // pushq   %r12
    0x53,                                           // pushq   %rbx
    0x50,                                           // pushq   %rax
    0x48, 0xb8, HOLE(PyThreadState_Get),            // movabsq $0, %rax
    0xff, 0xd0,                                     // callq   *%rax
    0x48, 0x8b, 0x48, 0x38,                         // movq    56(%rax), %rcx
    0x0f, 0x1f, 0x44, 0x00, 0x00,                   // nopl    (%rax,%rax)
    0x48, 0x8b, 0x49, 0x08,                         // movq    8(%rcx), %rcx
    0x80, 0x79, 0x46, 0x01,                         // cmpb    $1, 70(%rcx)
    0x74, 0x1b,                                     // je      0x45 <_justin_trampoline+0x45>
    0x48, 0x8b, 0x11,                               // movq    (%rcx), %rdx
    0x48, 0x63, 0xb2, 0xa8, 0x00, 0x00, 0x00,       // movslq  168(%rdx), %rsi
    0x48, 0x8d, 0x14, 0x72,                         // leaq    (%rdx,%rsi,2), %rdx
    0x48, 0x81, 0xc2, 0xc0, 0x00, 0x00, 0x00,       // addq    $192, %rdx
    0x48, 0x39, 0x51, 0x38,                         // cmpq    %rdx, 56(%rcx)
    0x72, 0xdb,                                     // jb      0x20 <_justin_trampoline+0x20>
    0x48, 0x8b, 0x69, 0x08,                         // movq    8(%rcx), %rbp
    0x48, 0x85, 0xed,                               // testq   %rbp, %rbp
    0x74, 0x2d,                                     // je      0x7b <_justin_trampoline+0x7b>
    0x66, 0x90,                                     // nop
    0x80, 0x7d, 0x46, 0x01,                         // cmpb    $1, 70(%rbp)
    0x74, 0x27,                                     // je      0x7d <_justin_trampoline+0x7d>
    0x48, 0x8b, 0x4d, 0x00,                         // movq    (%rbp), %rcx
    0x48, 0x63, 0x91, 0xa8, 0x00, 0x00, 0x00,       // movslq  168(%rcx), %rdx
    0x48, 0x8d, 0x0c, 0x51,                         // leaq    (%rcx,%rdx,2), %rcx
    0x48, 0x81, 0xc1, 0xc0, 0x00, 0x00, 0x00,       // addq    $192, %rcx
    0x48, 0x39, 0x4d, 0x38,                         // cmpq    %rcx, 56(%rbp)
    0x73, 0x0b,                                     // jae     0x7d <_justin_trampoline+0x7d>
    0x48, 0x8b, 0x6d, 0x08,                         // movq    8(%rbp), %rbp
    0x48, 0x85, 0xed,                               // testq   %rbp, %rbp
    0x75, 0xd5,                                     // jne     0x50 <_justin_trampoline+0x50>
    0x31, 0xed,                                     // xorl    %ebp, %ebp
    0x48, 0x63, 0x4d, 0x40,                         // movslq  64(%rbp), %rcx
    0x4c, 0x8d, 0x24, 0xcd, 0x48, 0x00, 0x00, 0x00, // leaq    72(,%rcx,8), %r12
    0x49, 0x01, 0xec,                               // addq    %rbp, %r12
    0x48, 0xb9, HOLE(_justin_continue),             // movabsq $0, %rcx
    0x49, 0x89, 0xc5,                               // movq    %rax, %r13
    0xff, 0xd1,                                     // callq   *%rcx
    0x48, 0x83, 0xc4, 0x08,                         // addq    $8, %rsp
    0x5b,                                           // popq    %rbx
    0x41, 0x5c,                                     // popq    %r12
    0x41, 0x5d,                                     // popq    %r13
    0x41, 0x5e,                                     // popq    %r14
    0x41, 0x5f,                                     // popq    %r15
    0x5d,                                           // popq    %rbp
    0xc3,                                           // retq
};
static const Hole trampoline_stencil_holes[] = {
    {.offset =  13, .addend = 0, .kind = LOAD_PyThreadState_Get},
    {.offset = 142, .addend = 0, .kind = HOLE_continue         },
};
static const Stencil trampoline_stencil = {
    .nbytes = Py_ARRAY_LENGTH(trampoline_stencil_bytes),
    .bytes = trampoline_stencil_bytes,
    .nholes = Py_ARRAY_LENGTH(trampoline_stencil_holes),
    .holes = trampoline_stencil_holes,
};

static const Stencil stencils[] = {
    [STORE_FAST] = STORE_FAST_stencil,
};