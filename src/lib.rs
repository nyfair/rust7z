#[repr(C)]
pub struct arcItem {
	pub isdir: u32,
	pub size: u32,
	pub path: *const u16,
}

extern "C" {
	pub fn init7z() -> i32;
	pub fn getFormatCount() -> u32;
	pub fn getArchiveExts(fmt_id: u32) -> *const u16;
	pub fn getArchiveType(fmt_id: u32) -> *const u16;
	pub fn openAndGetFileCount(archive: *const u16) -> u32;
	pub fn getFileInfo(index: u32) -> arcItem;
	pub fn extractToBuf(buf: *const u8, index: u32, size: u64);
	pub fn close();
}
