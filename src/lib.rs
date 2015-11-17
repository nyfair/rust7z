extern "C" {
	pub fn init7z() -> i32;
	pub fn getFormatCount() -> u32;
	pub fn getArchiveExts(fmt_id: u32) -> *const u16;
	pub fn getArchiveType(fmt_id: u32) -> *const u16;
}
