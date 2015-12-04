#[repr(C)]
pub struct ArcInfo {
	pub format: i32,
	pub file_count: u32,
	pub is_solid: u32,
}

#[repr(C)]
pub struct ArcItem {
	pub is_dir: u32,
	pub size: u32,
	pub path: *const u16,
}

extern "C" {
	pub fn init7z() -> i32;
	pub fn getFormatCount() -> u32;
	pub fn getArchiveExts(fmt_id: u32) -> *const u16;
	pub fn getArchiveType(fmt_id: u32) -> *const u16;
	pub fn open(archive: *const u16) -> ArcInfo;
	pub fn close();
	pub fn getFileInfo(index: u32) -> ArcItem;
	pub fn extractToBuf(buf: *const u8, index: *const u32, num_of_file: u32);
}
