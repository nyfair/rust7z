extern crate rust7z;

use std::ffi::{OsStr, OsString};
use std::os::windows::ffi::{OsStrExt, OsStringExt};

fn u2w(u8str: &str) -> *const u16 {
	OsStr::new(u8str).encode_wide().chain(Some(0).into_iter()).collect::<Vec<_>>().as_ptr()
}

fn w2u(wstr: *const u16) -> String {
    unsafe {
        let len = (0..std::isize::MAX).position(|i| *wstr.offset(i) == 0).unwrap();
        let slice = std::slice::from_raw_parts(wstr, len);
        OsString::from_wide(slice).to_string_lossy().into_owned()
    }
}

fn main() {
	unsafe {
		// info
		rust7z::init7z();
		let fmt_count = rust7z::getFormatCount();
		println!("Support archive formats count:{}", fmt_count);
		for i in 0..fmt_count {
			println!("{}:{}", w2u(rust7z::getArchiveType(i)), w2u(rust7z::getArchiveExts(i)));
		}

		// extract
		rust7z::open(u2w("examples/test.7z"));
		let archive = rust7z::getFileList();
		println!("{}", archive.file_count);
	}
}
