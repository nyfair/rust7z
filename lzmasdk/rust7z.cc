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
	UINT32 m_iPos;
	char* m_pBuf;
	UINT32 m_nBufSize;

	void Init(char* pBuf, UINT32 nBufSize) {
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
	UINT32 m_nBufSize;

	void Init(char* pBuf, UINT32 nBufSize) {
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

struct arc {
	wchar_t path[fnameLen];
	BOOL isDir;
	UINT32 size;
};

HMODULE dll;
UINT32 numf;
PROPVARIANT prop;
vector<CLSID> codecs;
vector<wstring> exts;
vector<wstring> types;

extern "C" {
	BOOL init7z() {
		dll = LoadLibraryA("7z.dll");
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
}

/*int wmain(int argc, wchar_t *argv[]) {
	init7z();
	CMyComPtr<IInArchive> archive;
	CMyComPtr<IInStream> file;
	OpenCallbackImp *openCallbackSpec = new OpenCallbackImp;
	CMyComPtr<IArchiveOpenCallback> openCallback = openCallbackSpec;
	bool valid = false;
	for (UINT32 i = 0; i < numf; i++) {
		FileStreamImp *fileSpec = new FileStreamImp(L"test.7z");
		file = fileSpec;
		createObject(&codecs[i], &IID_IInArchive, (void **)&archive);
		if (archive->Open(file, &scanSize, openCallback) == S_OK) {
			valid = true;
			break;
		} else {
			archive->Close();
		}
	}
	if (!valid)
		return 1;

	vector<arc> files;
	UINT32 fileCount = 0;
	UINT32 fullSize = 0;
	archive->GetNumberOfItems(&fileCount);
	for (UINT32 i = 0; i < fileCount; i++) {
		arc file;
		VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		archive->GetProperty(i, kpidPath, &prop);
		memcpy(file.path, prop.bstrVal, fnameLen);
		VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
		archive->GetProperty(i, kpidIsDir, &prop);
		file.isDir = prop.boolVal;
		if (file.isDir) {
			file.size = 0;
		} else {
			VariantClear(reinterpret_cast<VARIANTARG*>(&prop));
			archive->GetProperty(i, kpidSize, &prop);
			file.size = prop.ulVal;
			fullSize += file.size;
		}
		files.push_back(file);
	}
	
	extractCallbackImp *extractCallbackSpec = new extractCallbackImp;
	CMyComPtr<IArchiveExtractCallback> extractCallback(extractCallbackSpec);
	char *cache = (char*)LocalAlloc(LMEM_FIXED, fullSize);
	if (cache == NULL) {
		printf("Insufficient Memory");
		archive->Close();
		return 1;
	}
	extractCallbackSpec->Init(cache, fullSize);
	UINT32 index[2] = { 0,1 };
	archive->Extract(index, 2, false, extractCallback);
	archive->Close();

	FILE *output = fopen("test","wb");
	fwrite(cache, fullSize, 1, output);
	return 0;
}*/
