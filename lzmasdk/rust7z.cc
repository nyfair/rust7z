#include <vector>

#include "CPP/Common/MyInitGuid.h"
#include "CPP/Common/MyCom.h"
#include "CPP/7zip/Archive/IArchive.h"
#include "CPP/7zip/IPassword.h"

using namespace std;
extern "C" {
	typedef HRESULT(WINAPI* Func_CreateObject)(const GUID* clsID, const GUID* iid, void** outObject);
	typedef HRESULT(WINAPI* Func_GetNumberOfFormats)(UINT32* numFormats);
	typedef HRESULT(WINAPI* Func_GetHandlerProperty2)(UINT32 formatIndex, PROPID propID, PROPVARIANT* value);
}
Func_CreateObject createObject;
Func_GetNumberOfFormats getNumberOfFormats;
Func_GetHandlerProperty2 getHandlerProperty2;

UINT64 archive_offset;
class MemOutStream : public ISequentialOutStream, public CMyUnknownImp {
public:
	char* m_pBuf;
	BOOL write = TRUE;

	void Init(char* pBuf) {
		m_pBuf = pBuf;
	}

	MY_UNKNOWN_IMP
	STDMETHOD(Write)(const void* data, UINT32 size, UINT32* processedSize) {
		if (write) {
			memcpy(m_pBuf + archive_offset, data, size);
			archive_offset += size;
		}
		return S_OK;
	}
};

class OpenCallbackImp : public IArchiveOpenCallback, public CMyUnknownImp {
public:
	MY_UNKNOWN_IMP
	STDMETHOD(SetTotal)(const UINT64* files, const UINT64* bytes) { return S_OK; }
	STDMETHOD(SetCompleted)(const UINT64* files, const UINT64* bytes) { return S_OK; }
};

class extractCallbackImp : public IArchiveExtractCallback, public CMyUnknownImp {
public:
	char* m_pBuf;
	MemOutStream* pRealStream;

	void Init(char* pBuf) {
		m_pBuf = pBuf;
	}

	MY_UNKNOWN_IMP
	STDMETHOD(SetTotal)(UINT64 size) { return S_OK; }
	STDMETHOD(SetCompleted)(const UINT64* completeValue) { return S_OK; }

	STDMETHOD(GetStream)(UINT32 index, ISequentialOutStream** outStream, INT32 askExtractMode) {
		pRealStream = new MemOutStream;
		pRealStream->AddRef();
		pRealStream->Init(m_pBuf);
		*outStream = pRealStream;
		return S_OK;
	}

	STDMETHOD(PrepareOperation)(INT32 askExtractMode) {
		if (askExtractMode == NArchive::NExtract::NAskMode::kSkip)
			pRealStream->write = FALSE;
		return S_OK;
	}
	STDMETHOD(SetOperationResult)(INT32 resultEOperationResult) { return S_OK; }
};

class FileStreamImp : public IInStream, public CMyUnknownImp {
public:
	HANDLE hFile;
	MY_UNKNOWN_IMP

	FileStreamImp(LPWSTR buf) {
		hFile = CreateFileW(buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	~FileStreamImp() {
		CloseHandle(hFile);
	}

	STDMETHOD(Read)(void* data, UINT32 size, UINT32* processedSize) {
		DWORD resultSize = 0;
		BOOL res = ReadFile(hFile, data, size, &resultSize, NULL);
			*processedSize = resultSize;
		return res ? S_OK : E_FAIL;
	}

	STDMETHOD(Seek)(INT64 offset, UINT32 seekOrigin, UINT64* newPosition) {
		LARGE_INTEGER liDistanceToMove, liNewFilePoINTer;
		liDistanceToMove.QuadPart = offset;
		BOOL res = SetFilePointerEx(hFile, liDistanceToMove, &liNewFilePoINTer, static_cast<DWORD>(seekOrigin));
		if (newPosition)
			*newPosition = liNewFilePoINTer.QuadPart;
		return res ? S_OK : E_FAIL;
	}
};

const UINT64 scanSize = 1 << 23;
const UINT32 fnameLen = 1 << 8;
const UINT32 entrySize = 16;

struct ArcInfo {
	INT32 format = -1;
	UINT32 file_count;
	BOOL is_solid;
};

struct ArcItem {
	BOOL is_dir;
	UINT32 size;
	wchar_t* path;
};

HMODULE dll;
UINT32 numf;
PROPVARIANT prop;
vector<CLSID> codecs;
vector<wstring> exts;
vector<wstring> types;
CMyComPtr<IInArchive> archive;

extern "C" {
	BOOL init7z() {
		dll = LoadLibraryW(L"7z.dll");
		if (dll == NULL) {
			HKEY hKey = NULL;
			if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\7-Zip", 0, NULL,
				REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, NULL) == ERROR_SUCCESS) {
				wchar_t regPath[fnameLen];
				DWORD cbData = sizeof(regPath);
				if (RegQueryValueExW(hKey, L"Path", 0, 0, (LPBYTE)regPath, &cbData) == ERROR_SUCCESS) {
					wstring regDll = regPath;
					regDll += L"7z.dll";
					dll = LoadLibraryW(regDll.c_str());
				}
			}
		}
		if (dll == NULL) {
			printf("7z.dll is missing");
			return FALSE;
		}
		createObject = (Func_CreateObject)GetProcAddress(dll, "CreateObject");
		getNumberOfFormats = (Func_GetNumberOfFormats)GetProcAddress(dll, "GetNumberOfFormats");
		getHandlerProperty2 = (Func_GetHandlerProperty2)GetProcAddress(dll, "GetHandlerProperty2");

		getNumberOfFormats(&numf);
		VariantInit(reinterpret_cast<VARIANTARG*>(&prop));
		for (UINT32 i = 0; i < numf; i++) {
			getHandlerProperty2(i, NArchive::NHandlerPropID::kClassID, &prop);
			codecs.push_back(*(prop.puuid));
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
			getHandlerProperty2(i, NArchive::NHandlerPropID::kExtension, &prop);
			exts.push_back(prop.bstrVal);
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
			getHandlerProperty2(i, NArchive::NHandlerPropID::kName, &prop);
			types.push_back(prop.bstrVal);
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		}
		return TRUE;
	}

	UINT32 getFormatCount() {
		return numf;
	}

	const wchar_t* getArchiveExts(UINT32 index) {
		return exts[index].c_str();
	}

	const wchar_t* getArchiveType(UINT32 index) {
		return types[index].c_str();
	}

	ArcInfo open(wchar_t* input) {
		ArcInfo arc;
		CMyComPtr<IInStream> file;
		OpenCallbackImp* openCallbackSpec = new OpenCallbackImp;
		CMyComPtr<IArchiveOpenCallback> openCallback = openCallbackSpec;
		for (UINT32 i = 0; i < numf; i++) {
			FileStreamImp* fileSpec = new FileStreamImp(input);
			file = fileSpec;
			createObject(&codecs[i], &IID_IInArchive, (void**)&archive);
			if (archive->Open(file, &scanSize, openCallback) == S_OK) {
				archive->GetNumberOfItems(&arc.file_count);
				arc.format = i;
				VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
				archive->GetArchiveProperty(kpidSolid, &prop);
				arc.is_solid = prop.bVal;
				break;
			} else {
				archive->Close();
			}
		}
		return arc;
	}

	void close() {
		archive->Close();
	}

	ArcItem getFileInfo(UINT32 index) {
		ArcItem file;
		archive->GetProperty(index, kpidIsDir, &prop);
		file.is_dir = prop.boolVal;
		VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		if (file.is_dir) {
			file.size = 0;
		} else {
			archive->GetProperty(index, kpidSize, &prop);
			file.size = prop.ulVal;
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		}
		archive->GetProperty(index, kpidPath, &prop);
		file.path = prop.bstrVal;
		VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		return file;
	}

	void extractToBuf(char* buf, UINT32* index, UINT32 num_of_file) {
		extractCallbackImp* extractCallbackSpec = new extractCallbackImp;
		CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);
		extractCallbackSpec->Init(buf);
		archive_offset = 0;
		archive->Extract(index, num_of_file, false, extractCallback);
	}

	int wmain(INT argc, wchar_t* argv) {
		init7z();
		ArcInfo arc = open(L"examples/test.7z");
		UINT64 fullSize = 0;
		UINT32* fullIndex = new UINT32[arc.file_count];
		for (UINT32 i = 0; i < arc.file_count; i++) {
			ArcItem file = getFileInfo(i);
			fullSize += file.size;
			fullIndex[i] = i;
		}
		char* buf = (char*) malloc(fullSize);
		extractToBuf(buf, fullIndex, arc.file_count);
		delete fullIndex;
		free(buf);
		close();
	}
}
