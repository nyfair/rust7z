extern crate rust7z;

use std::ffi::{OsStr, OsString};
use std::os::windows::ffi::{OsStrExt, OsStringExt};
use std::io::prelude::*;
use std::io::BufWriter;
use std::fs::File;

fn u2w(u8str: &str) -> Vec<u16> {
	OsStr::new(u8str).encode_wide().chain(Some(0).into_iter()).collect::<Vec<_>>()
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
		let k = u2w("examples/test.7z");
		let file_count = rust7z::open(k.as_ptr()).file_count;
		println!("File Count: {}", file_count);
		for i in 0..file_count {
			let file = rust7z::getFileInfo(i);
			let fname = w2u(file.path);
			println!("{}: {}", fname, file.size);
			let buf = vec![0; file.size as usize];
			rust7z::extractToBuf(buf.as_ptr(), i, file.size as u64);
			let output = File::create(fname).unwrap();
			let mut writer = BufWriter::new(output);
			writer.write(&buf).unwrap();
			writer.flush().unwrap();
		}
		rust7z::close();
	}
}
