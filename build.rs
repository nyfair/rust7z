extern crate cc;

fn main() {
    cc::Build::new().cpp(true).file("lzmasdk/rust7z.cc").compile("lib7z.a");
    println!("cargo:rustc-flags=-l dylib=oleaut32");
}
