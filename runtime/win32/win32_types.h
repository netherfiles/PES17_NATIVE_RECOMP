#pragma once
// ============================================================================
// win32_types.h - Tipos/constantes Win32 em C++ puro (sem <windows.h>)
// Alvo: Android ARM64 (NDK 27, C++20). O "guest" é x86 de 32 bits.
//
// Convenções:
//  * Tipos escalares seguem o layout Win32 de 32 bits (DWORD=4, LONG=4, BOOL=4).
//  * Structs que o guest lê/escreve diretamente na memória usam campos de ponteiro
//    de 32 bits (GPTR) para manter o layout x86 idêntico.
//  * Parâmetros de função (HANDLE, LPVOID...) são ponteiros do host. Como a arena
//    do guest é mapeada em endereços < 4GB com identidade, a conversão é um cast.
// ============================================================================

#include <cstdint>
#include <cstddef>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#endif

// ----------------------------------------------------------------------------
// Convenções de chamada / exportação (no-op em ARM64)
// ----------------------------------------------------------------------------
#define WINAPI
#define CALLBACK
#define APIENTRY
#define STDMETHODCALLTYPE
#define CDECL
#define WIN32_API extern "C" __attribute__((visibility("default")))

#ifndef NULL
#define NULL nullptr
#endif
#define TRUE  1
#define FALSE 0
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu

// ----------------------------------------------------------------------------
// Tipos escalares
// ----------------------------------------------------------------------------
typedef void            VOID;
typedef char            CHAR;
typedef uint16_t        WCHAR;
typedef uint8_t         BYTE;
typedef uint16_t        WORD;
typedef uint32_t        DWORD;
typedef int32_t         LONG;
typedef uint32_t        ULONG;
typedef int32_t         BOOL;
typedef int32_t         INT;
typedef uint32_t        UINT;
typedef int16_t         SHORT;
typedef uint16_t        USHORT;
typedef float           FLOAT;
typedef int64_t         LONGLONG;
typedef uint64_t        ULONGLONG;
typedef uint64_t        DWORD64;
typedef uint32_t        GPTR;          // ponteiro de 32 bits do guest (para structs com layout x86)

typedef size_t          SIZE_T;        // lado host: 64 bits
typedef intptr_t        INT_PTR;
typedef uintptr_t       UINT_PTR;
typedef uintptr_t       ULONG_PTR;
typedef uintptr_t       DWORD_PTR;
typedef intptr_t        LONG_PTR;

typedef BYTE*           LPBYTE;
typedef DWORD*          LPDWORD;
typedef LONG*           PLONG;
typedef BOOL*           LPBOOL;
typedef void*           PVOID;
typedef void*           LPVOID;
typedef const void*     LPCVOID;
typedef char*           LPSTR;
typedef const char*     LPCSTR;
typedef WCHAR*          LPWSTR;
typedef const WCHAR*    LPCWSTR;

typedef int32_t         HRESULT;
typedef DWORD           LCID;
typedef WORD            ATOM;
typedef UINT_PTR        WPARAM;
typedef LONG_PTR        LPARAM;
typedef LONG_PTR        LRESULT;

// ----------------------------------------------------------------------------
// Handles (void*; o VALOR é sempre um ID < 4GB, nunca um ponteiro do host)
// ----------------------------------------------------------------------------
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* HWND;
typedef void* HDC;
typedef void* HMENU;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HBRUSH;
typedef void* HMONITOR;
typedef void* HGLOBAL;
typedef void* HLOCAL;
typedef HANDLE* PHANDLE;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

typedef intptr_t (*FARPROC)(void);
typedef FARPROC  PROC;

// ----------------------------------------------------------------------------
// Structs
// ----------------------------------------------------------------------------
struct POINT { LONG x; LONG y; };
struct SIZE  { LONG cx; LONG cy; };
struct RECT  { LONG left; LONG top; LONG right; LONG bottom; };
typedef POINT* LPPOINT;
typedef RECT*  LPRECT;

struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };

union LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    LONGLONG QuadPart;
};
typedef LARGE_INTEGER* PLARGE_INTEGER;

union ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; };
    ULONGLONG QuadPart;
};

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
};
typedef GUID  IID;
typedef GUID  CLSID;

// Layout x86 (32 bits)
struct SECURITY_ATTRIBUTES {
    DWORD nLength;
    GPTR  lpSecurityDescriptor;
    BOOL  bInheritHandle;
};
typedef SECURITY_ATTRIBUTES* LPSECURITY_ATTRIBUTES;

// Layout x86 (32 bits)
struct OVERLAPPED {
    DWORD Internal;
    DWORD InternalHigh;
    DWORD Offset;
    DWORD OffsetHigh;
    GPTR  hEvent;
};
typedef OVERLAPPED* LPOVERLAPPED;

static_assert(sizeof(BYTE)  == 1, "BYTE");
static_assert(sizeof(WORD)  == 2, "WORD");
static_assert(sizeof(DWORD) == 4, "DWORD");
static_assert(sizeof(LONG)  == 4, "LONG");
static_assert(sizeof(BOOL)  == 4, "BOOL");
static_assert(sizeof(POINT) == 8, "POINT");
static_assert(sizeof(RECT)  == 16, "RECT");
static_assert(sizeof(LARGE_INTEGER) == 8, "LARGE_INTEGER");
static_assert(sizeof(GUID) == 16, "GUID");
static_assert(sizeof(SECURITY_ATTRIBUTES) == 12, "SECURITY_ATTRIBUTES (x86)");
static_assert(sizeof(OVERLAPPED) == 20, "OVERLAPPED (x86)");

// ----------------------------------------------------------------------------
// Macros utilitárias
// ----------------------------------------------------------------------------
#define LOWORD(l)      ((WORD)((DWORD_PTR)(l) & 0xFFFF))
#define HIWORD(l)      ((WORD)(((DWORD_PTR)(l) >> 16) & 0xFFFF))
#define LOBYTE(w)      ((BYTE)((DWORD_PTR)(w) & 0xFF))
#define HIBYTE(w)      ((BYTE)(((DWORD_PTR)(w) >> 8) & 0xFF))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))

#define SUCCEEDED(hr)  (((HRESULT)(hr)) >= 0)
#define FAILED(hr)     (((HRESULT)(hr)) < 0)

// ----------------------------------------------------------------------------
// HRESULT
// ----------------------------------------------------------------------------
#define S_OK            ((HRESULT)0)
#define S_FALSE         ((HRESULT)1)
#define E_NOTIMPL       ((HRESULT)0x80004001L)
#define E_NOINTERFACE   ((HRESULT)0x80004002L)
#define E_POINTER       ((HRESULT)0x80004003L)
#define E_ABORT         ((HRESULT)0x80004004L)
#define E_FAIL          ((HRESULT)0x80004005L)
#define E_OUTOFMEMORY   ((HRESULT)0x8007000EL)
#define E_INVALIDARG    ((HRESULT)0x80070057L)

// ----------------------------------------------------------------------------
// Códigos de erro (GetLastError)
// ----------------------------------------------------------------------------
#define ERROR_SUCCESS               0u
#define ERROR_FILE_NOT_FOUND        2u
#define ERROR_PATH_NOT_FOUND        3u
#define ERROR_TOO_MANY_OPEN_FILES   4u
#define ERROR_ACCESS_DENIED         5u
#define ERROR_INVALID_HANDLE        6u
#define ERROR_NOT_ENOUGH_MEMORY     8u
#define ERROR_INVALID_DATA          13u
#define ERROR_OUTOFMEMORY           14u
#define ERROR_HANDLE_EOF            38u
#define ERROR_FILE_EXISTS           80u
#define ERROR_INVALID_PARAMETER     87u
#define ERROR_MOD_NOT_FOUND         126u
#define ERROR_PROC_NOT_FOUND        127u
#define ERROR_ALREADY_EXISTS        183u
#define ERROR_INVALID_ADDRESS       487u

// ----------------------------------------------------------------------------
// Memória virtual
// ----------------------------------------------------------------------------
#define MEM_COMMIT       0x00001000u
#define MEM_RESERVE      0x00002000u
#define MEM_DECOMMIT     0x00004000u
#define MEM_RELEASE      0x00008000u
#define MEM_FREE         0x00010000u
#define MEM_RESET        0x00080000u
#define MEM_TOP_DOWN     0x00100000u

#define PAGE_NOACCESS           0x01u
#define PAGE_READONLY           0x02u
#define PAGE_READWRITE          0x04u
#define PAGE_WRITECOPY          0x08u
#define PAGE_EXECUTE            0x10u
#define PAGE_EXECUTE_READ       0x20u
#define PAGE_EXECUTE_READWRITE  0x40u
#define PAGE_EXECUTE_WRITECOPY  0x80u

// ----------------------------------------------------------------------------
// Arquivos (usados por win32_file_stubs.cpp)
// ----------------------------------------------------------------------------
#define GENERIC_READ     0x80000000u
#define GENERIC_WRITE    0x40000000u
#define GENERIC_EXECUTE  0x20000000u
#define GENERIC_ALL      0x10000000u

#define FILE_SHARE_READ    0x00000001u
#define FILE_SHARE_WRITE   0x00000002u
#define FILE_SHARE_DELETE  0x00000004u

#define CREATE_NEW         1u
#define CREATE_ALWAYS      2u
#define OPEN_EXISTING      3u
#define OPEN_ALWAYS        4u
#define TRUNCATE_EXISTING  5u

#define FILE_ATTRIBUTE_READONLY   0x00000001u
#define FILE_ATTRIBUTE_DIRECTORY  0x00000010u
#define FILE_ATTRIBUTE_NORMAL     0x00000080u
#define FILE_FLAG_OVERLAPPED      0x40000000u
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000u
#define FILE_FLAG_RANDOM_ACCESS   0x10000000u

#define FILE_BEGIN    0u
#define FILE_CURRENT  1u
#define FILE_END      2u

#define INVALID_SET_FILE_POINTER 0xFFFFFFFFu
#define INVALID_FILE_SIZE        0xFFFFFFFFu
#define INVALID_FILE_ATTRIBUTES  0xFFFFFFFFu

// ============================================================================
// API interna da camada de compatibilidade
// ============================================================================

// Log (logcat no Android, stderr fora dele)
extern "C" void Win32_Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// --- Registro de exports --------------------------------------------------
// Registra uma função host sob "dll!name" e devolve o endereço-thunk de 32 bits
// que o guest enxerga. Seguro de chamar durante a inicialização estática.
extern "C" uint32_t Win32_RegisterExport(const char* dll, const char* name, void* hostFn);

// Resolve um import em tempo de recompilação/bind da IAT. Retorna 0 se não existir.
extern "C" uint32_t Win32_ResolveImport(const char* dll, const char* name);

// Dispatcher de chamadas indiretas: converte endereço-thunk -> função host.
// Retorna nullptr se 'guestAddr' não for um thunk.
extern "C" void* Win32_ResolveThunk(uint32_t guestAddr);
extern "C" bool  Win32_IsThunkAddress(uint32_t guestAddr);

// Registro automático em outros .cpp:  WIN32_EXPORT("kernel32.dll", CloseHandle);
struct Win32ExportRegistrar {
    Win32ExportRegistrar(const char* dll, const char* name, void* fn) { Win32_RegisterExport(dll, name, fn); }
};
#define WIN32_EXPORT(dll, fn) \
    static Win32ExportRegistrar win32_registrar_##fn(dll, #fn, reinterpret_cast<void*>(&fn))

// Conversões guest<->host (arena idêntica, < 4GB)
static inline uint32_t Win32_ToGuest(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
static inline void*    Win32_ToHost(uint32_t g)     { return reinterpret_cast<void*>(static_cast<uintptr_t>(g)); }

// --- Protótipos kernel32 (win32_stubs.cpp) --------------------------------
WIN32_API DWORD   WINAPI GetLastError(void);
WIN32_API void    WINAPI SetLastError(DWORD dwErrCode);

WIN32_API LPVOID  WINAPI VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
WIN32_API BOOL    WINAPI VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
WIN32_API BOOL    WINAPI VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, LPDWORD lpflOldProtect);

WIN32_API DWORD     WINAPI GetTickCount(void);
WIN32_API ULONGLONG WINAPI GetTickCount64(void);
WIN32_API void      WINAPI Sleep(DWORD dwMilliseconds);
WIN32_API BOOL      WINAPI QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
WIN32_API BOOL      WINAPI QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);

WIN32_API HMODULE WINAPI LoadLibraryA(LPCSTR lpLibFileName);
WIN32_API BOOL    WINAPI FreeLibrary(HMODULE hLibModule);
WIN32_API HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);
WIN32_API FARPROC WINAPI GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
