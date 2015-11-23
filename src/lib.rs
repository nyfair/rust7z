#[repr(C)]
pub struct arcItem {
	pub path: [u16; 256],
	pub isdir: u32,
	pub size: u32,
}

#[repr(C)]
pub struct arcFile {
	pub file_count: u32,
	pub arc_item: arcItem,
}

extern "C" {
	pub fn init7z() -> i32;
	pub fn getFormatCount() -> u32;
	pub fn getArchiveExts(fmt_id: u32) -> *const u16;
	pub fn getArchiveType(fmt_id: u32) -> *const u16;
	pub fn open(arcFile: *const u16) -> i32;
	pub fn close();
	pub fn getFileList() -> arcFile;
}
