/*
Parses an LLVM IR file and converts it to cranelift backend.
*/
mod translator;

extern crate llvm_sys;

use std::ffi::{CString};

use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        panic!("Too many file arguments. Usage: ./translator llvm_ir_file_path")
    }
    let path = CString::new(args[1].clone()).expect("Bad CString");
    // https://www.reddit.com/r/rust/comments/uewxw3/convert_cstring_to_c_char_rrust/
    unsafe { translator::llvm_ir_file_to_cranelift(path.into_raw()) }
}
