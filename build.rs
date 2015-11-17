extern crate gcc;

fn main() {
    gcc::compile_library("lib7z.a", &["lzmasdk/rust7z.cc"]);
    println!("cargo:rustc-flags=-l dylib=oleaut32");
}
