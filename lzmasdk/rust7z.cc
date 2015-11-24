#include <vector>

#include "CPP/Common/MyInitGuid.h"
#include "CPP/Common/MyCom.h"
#include "CPP/7zip/Archive/IArchive.h"
#include "CPP/7zip/IPassword.h"

using namespace std;
extern "C" {
	typedef HRESULT(WINAPI *Func_CreateObject)(const GUID *clsID, const GUID *iid, void **outObject);
	typedef HRESULT(WINAPI *Func_GetNumberOfFormats)(UINT32 *numFormats);
	typedef HRESULT(WINAPI *Func_GetHandlerProperty2)(UINT32 formatIndex, PROPID propID, PROPVARIANT *value);
}
Func_CreateObject createObject;
Func_GetNumberOfFormats getNumberOfFormats;
Func_GetHandlerProperty2 getHandlerProperty2;

class MemOutStream : public ISequentialOutStream, public CMyUnknownImp {
public:
	UINT64 m_iPos;
	char* m_pBuf;
	UINT64 m_nBufSize;

	void Init(char* pBuf, UINT64 nBufSize) {
		m_iPos = 0;
		m_pBuf = pBuf;
		m_nBufSize = nBufSize;
	}

	MY_UNKNOWN_IMP
	STDMETHOD(Write)(const void *data, UINT32 size, UINT32 *processedSize) {
		memcpy(m_pBuf + m_iPos, data, size);
		m_iPos += size;
		return S_OK;
	}
};

class OpenCallbackImp : public IArchiveOpenCallback, public CMyUnknownImp {
public:
	MY_UNKNOWN_IMP
	STDMETHOD(SetTotal)(const UINT64 *files, const UINT64 *bytes) { return S_OK; }
	STDMETHOD(SetCompleted)(const UINT64 *files, const UINT64 *bytes) { return S_OK; }
};

class extractCallbackImp : public IArchiveExtractCallback, public CMyUnknownImp {
public:
	char* m_pBuf;
	UINT64 m_nBufSize;

	void Init(char* pBuf, UINT64 nBufSize) {
		m_pBuf = pBuf;
		m_nBufSize = nBufSize;
	}

	MY_UNKNOWN_IMP
	STDMETHOD(SetTotal)(UINT64 size) { return S_OK; }
	STDMETHOD(SetCompleted)(const UINT64 *completeValue) { return S_OK; }

	STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode) {
		MemOutStream* pRealStream = new MemOutStream;
		pRealStream->AddRef();
		pRealStream->Init(m_pBuf, m_nBufSize);
		*outStream = pRealStream;
		return S_OK;
	}

	STDMETHOD(PrepareOperation)(Int32 askExtractMode) { return S_OK; }
	STDMETHOD(SetOperationResult)(Int32 resultEOperationResult) { return S_OK; }
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

	STDMETHOD(Read)(void *data, UINT32 size, UINT32 *processedSize) {
		DWORD resultSize = 0;
		BOOL res = ReadFile(hFile, data, size, &resultSize, NULL);
			*processedSize = resultSize;
		return res ? S_OK : E_FAIL;
	}

	STDMETHOD(Seek)(INT64 offset, UINT32 seekOrigin, UINT64 *newPosition) {
		LARGE_INTEGER liDistanceToMove, liNewFilePointer;
		liDistanceToMove.QuadPart = offset;
		BOOL res = SetFilePointerEx(hFile, liDistanceToMove, &liNewFilePointer, static_cast<DWORD>(seekOrigin));
		if (newPosition)
			*newPosition = liNewFilePointer.QuadPart;
		return res ? S_OK : E_FAIL;
	}
};

const UINT64 scanSize = 1 << 23;
const UINT32 fnameLen = 1 << 8;
const UINT32 entrySize = 16;

struct arcItem {
	BOOL isDir;
	UINT32 size;
	wchar_t *path;
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
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
			getHandlerProperty2(i, NArchive::NHandlerPropID::kClassID, &prop);
			codecs.push_back(*(prop.puuid));
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
			getHandlerProperty2(i, NArchive::NHandlerPropID::kExtension, &prop);
			exts.push_back(prop.bstrVal);
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
			getHandlerProperty2(i, NArchive::NHandlerPropID::kName, &prop);
			types.push_back(prop.bstrVal);
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

	UINT32 openAndGetFileCount(wchar_t *input) {
		CMyComPtr<IInStream> file;
		OpenCallbackImp *openCallbackSpec = new OpenCallbackImp;
		CMyComPtr<IArchiveOpenCallback> openCallback = openCallbackSpec;
		for (UINT32 i = 0; i < numf; i++) {
			FileStreamImp *fileSpec = new FileStreamImp(input);
			file = fileSpec;
			createObject(&codecs[i], &IID_IInArchive, (void **)&archive);
			if (archive->Open(file, &scanSize, openCallback) == S_OK) {
				UINT32 fileCount;
				archive->GetNumberOfItems(&fileCount);
				return fileCount;
				break;
			} else {
				archive->Close();
			}
		}
		return 0;
	}

	void close() {
		archive->Close();
	}

	arcItem getFileInfo(UINT32 index) {
		arcItem file;
		VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		archive->GetProperty(index, kpidIsDir, &prop);
		file.isDir = prop.boolVal;
		if (file.isDir) {
			file.size = 0;
		} else {
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
			archive->GetProperty(index, kpidSize, &prop);
			file.size = prop.ulVal;
		}
		VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		archive->GetProperty(index, kpidPath, &prop);
		file.path = prop.bstrVal;
		return file;
	}

	void extractToBuf(char* buf, UINT32 index, UINT64 size) {
		extractCallbackImp *extractCallbackSpec = new extractCallbackImp;
		CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);
		extractCallbackSpec->Init(buf, size);
		UINT32 fullIndex[1] = { index };
		archive->Extract(fullIndex, 1, false, extractCallback);
	}
}
