use std::ffi::c_char;
use std::ptr;
use llvm_sys::core::{LLVMCreateMemoryBufferWithContentsOfFile, LLVMDisposeModule, LLVMGetGlobalContext};
use llvm_sys::ir_reader::LLVMParseIRInContext;
use llvm_sys::prelude::{LLVMMemoryBufferRef, LLVMModuleRef};

pub unsafe fn llvm_ir_file_to_cranelift(path: *const c_char) {
    unsafe {
        let mut buffer: LLVMMemoryBufferRef = ptr::null_mut();
        let mut module: LLVMModuleRef = std::mem::MaybeUninit::zeroed().assume_init();
        let mut out_message: *mut c_char = ptr::null_mut();
        let mut res = LLVMCreateMemoryBufferWithContentsOfFile(path, &mut buffer, &mut out_message);
        if res != 0 {
            panic!("Could not create memory buffer with file {:?}\n", path);
        }
        res = LLVMParseIRInContext(LLVMGetGlobalContext(), buffer, &mut module, &mut out_message);
        if res != 0 {
            panic!("Could not parse LLVM IR file {:?}\n", path);
        }
        LLVMDisposeModule(module);
        println!("Done!");
        // let ctx = LLVMGetGlobalContext();
        // LLVMParseIRInContext
    }
}