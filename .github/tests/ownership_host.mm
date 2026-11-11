#include "4DPluginAPI.h"
#include "PrivateTypes.h"
#include "EntryPoints.h"
#include "ARRAY_BOOLEAN.h"
#include "ownership_windows.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <type_traits>

static void require(bool ok, const char *message) {
    if (!ok) { fprintf(stderr, "%s\n", message); std::exit(1); }
}
static void denied() { require(false, "unexpected platform operation"); }
@implementation OwnershipURL
- (id)initWithString:(NSString *)value { denied(); return nil; }
- (NSString *)absoluteString { denied(); return nil; }
@end
CFURLRef OwnershipCreateURL(CFAllocatorRef, CFStringRef, CFURLPathStyle, Boolean) {
    denied(); return nullptr;
}
CFStringRef OwnershipCopyPath(CFURLRef, CFURLPathStyle) { denied(); return nullptr; }

static std::set<PA_Unichar *> strings;
static std::map<PA_Picture, int> pictures;
static std::map<PA_Handle, size_t> handles;
static bool nullEmpty, commandFailure, handleFailure;
static bool pictureCreationFailure;
static CUTF16String fixture;
static unsigned commandCount;

static PA_Unistring makeString(const CUTF16String &text) {
    PA_Unistring value = {};
    if (text.empty() && nullEmpty) return value;
    value.fLength = text.size();
    value.fString = new PA_Unichar[text.size() + 1];
    std::copy(text.begin(), text.end(), value.fString);
    value.fString[text.size()] = 0;
    strings.insert(value.fString);
    return value;
}
static void disposeString(PA_Unistring value) {
    if (!value.fString) return;
    require(strings.erase(value.fString) == 1, "duplicate/unowned string disposal");
    delete[] value.fString;
}
static PA_Picture makePicture() {
    auto p = new char;
    pictures[p] = 1;
    return p;
}
static void releasePicture(PA_Picture p) {
    if (!p) return;
    auto found = pictures.find(p);
    require(found != pictures.end(), "unowned picture disposal");
    if (--found->second == 0) { pictures.erase(found); delete static_cast<char *>(p); }
}
static void clear(PA_Variable *v) {
    switch (v->fType) {
    case eVK_Unistring: disposeString(v->uValue.fString); break;
    case eVK_Picture: releasePicture(v->uValue.fPicture); break;
    case eVK_ArrayUnicode: {
        auto h = v->uValue.fArray.fData;
        if (h) {
            auto values = reinterpret_cast<PA_Unistring *>(*h);
            for (long i=0; i<=v->uValue.fArray.fNbElements; ++i) disposeString(values[i]);
            PA_DisposeHandle(h);
        }
        break;
    }
    case eVK_ArrayLongint: PA_DisposeHandle(v->uValue.fArray.fData); break;
    case eVK_Undefined: case eVK_Longint: break;
    default: require(false, "unexpected clear kind");
    }
    v->fType = eVK_Undefined;
}
static void engine(short selector, EngineBlock *eb) {
    switch (selector) {
    case EX_CREATE_UNISTRING:
        eb->fUniString1 = makeString(CUTF16String(reinterpret_cast<PA_Unichar *>(eb->fHandle)));
        break;
    case EX_SET_UNISTRING: {
        CUTF16String copy(reinterpret_cast<PA_Unichar *>(eb->fHandle));
        disposeString(eb->fUniString1);
        eb->fUniString1 = makeString(copy);
        break;
    }
    case EX_DISPOSE_UNISTRING: disposeString(eb->fUniString1); break;
    case EX_CREATE_PICTURE:
        eb->fPicture=pictureCreationFailure ? reinterpret_cast<void *>(99) : makePicture();
        eb->fError=pictureCreationFailure ? eER_IncompatibleTypes : eER_NoErr;
        break;
    case EX_DISPOSE_PICTURE: releasePicture(eb->fPicture); break;
    case EX_DUPLICATE_PICTURE:
        require(eb->fParam1 == 0, "unexpected deep picture duplication");
        if (eb->fPicture) {
            require(pictures.count(eb->fPicture), "retain dead picture");
            ++pictures[eb->fPicture];
        }
        break;
    case EX_GET_PICTURE_DATA:
        require(pictures.count(eb->fPicture), "query dead picture");
        // Empty synthetic pictures have no representations. Failed string is unspecified.
        eb->fError=eER_IncompatibleTypes;
        eb->fUniString1={};
        break;
    case EX_CLEAR_VARIABLE: clear(static_cast<PA_Variable *>(eb->fPtr1)); break;
    case EX_HANDLE_MANAGER: {
        eb->fError=0;
        auto h=eb->fHandle;
        switch (eb->fParam1) {
        case 1:
            if (handleFailure) { eb->fHandle=nullptr; eb->fError=eER_IncompatibleTypes; break; }
            h=new char *;
            *h=new char[eb->fParam2]();
            handles[h]=eb->fParam2; eb->fHandle=h; break;
        case 2:
            require(handles.erase(h)==1, "dispose unowned handle");
            delete[] *h; delete h; break;
        case 3: {
            require(handles.count(h), "resize unowned handle");
            size_t size=eb->fParam2;
            char *data=new char[size]();
            memcpy(data, *h, std::min(size, handles[h]));
            delete[] *h; *h=data; handles[h]=size; break;
        }
        case 4: require(handles.count(h), "size unowned handle"); eb->fParam2=handles[h]; break;
        case 5: require(handles.count(h), "lock unowned handle"); eb->fParam3=(sLONG_PTR)*h; break;
        case 6: require(handles.count(h), "unlock unowned handle"); break;
        default: require(false, "unexpected handle operation");
        }
        break;
    }
    case EX_EXECUTE_COMMAND_BY_ID: {
        ++commandCount;
        auto args=static_cast<PA_Variable **>(eb->fPtr1);
        eb->fError=commandFailure ? eER_IncompatibleTypes : eER_NoErr;
        // These are procedures; their successful result has no owner.
        static_cast<PA_Variable *>(eb->fPtr2)->fType=eVK_Undefined;
        if (commandFailure) break;
        switch (eb->fParam1) {
        case 678:
            require(eb->fParam2==2, "path argument count");
            require(CUTF16String(args[0]->uValue.fString.fString ?
                    args[0]->uValue.fString.fString : reinterpret_cast<const PA_Unichar *>(u""),
                    args[0]->uValue.fString.fLength)==fixture, "path content/length");
            clear(args[1]); PA_SetPictureVariable(args[1], makePicture()); break;
        case 988: require(eb->fParam2==2, "gray argument count"); break;
        case 457:
            require(eb->fParam2==3, "size argument count");
            PA_SetLongintVariable(args[1], 13); PA_SetLongintVariable(args[2], 17); break;
        case 679:
            require(eb->fParam2==4, "thumbnail argument count");
            clear(args[1]); PA_SetPictureVariable(args[1], makePicture()); break;
        default: require(false, "unexpected engine command");
        }
        break;
    }
    default: require(false, "unexpected engine selector");
    }
}

static DWORD winError;
static bool serviceSuccess;
static unsigned serviceCalls;
size_t OwnershipWideLength(const wchar_t *s) {
    auto p=reinterpret_cast<const PA_Unichar *>(s);
    size_t n=0; while (p[n]) ++n; return n;
}
SC_HANDLE OpenSCManager(LPCTSTR a, LPCTSTR, DWORD) {
    require(!a, "remote SCM requested"); ++serviceCalls;
    winError=5; return serviceSuccess ? reinterpret_cast<void *>(1) : nullptr;
}
SC_HANDLE OpenService(SC_HANDLE h, LPCTSTR name, DWORD) {
    require(h==reinterpret_cast<void *>(1) && name, "synthetic service input");
    return reinterpret_cast<void *>(2);
}
BOOL CloseServiceHandle(SC_HANDLE h) {
    require(h==reinterpret_cast<void *>(1) || h==reinterpret_cast<void *>(2), "bad SCM handle");
    return TRUE;
}
DWORD GetLastError() { return winError; }
BOOL QueryServiceConfig(SC_HANDLE h, LPQUERY_SERVICE_CONFIG config, DWORD size, DWORD *needed) {
    require(h==reinterpret_cast<void *>(2), "bad synthetic query");
    *needed=sizeof(QUERY_SERVICE_CONFIG);
    if (!config) { winError=ERROR_INSUFFICIENT_BUFFER; return FALSE; }
    require(size==*needed, "query size");
    config->dwServiceType=16; config->dwStartType=2;
    config->lpBinaryPathName=config->lpServiceStartName=config->lpDisplayName=
        reinterpret_cast<wchar_t *>(const_cast<PA_Unichar *>(fixture.c_str()));
    return TRUE;
}
BOOL EnumServicesStatus(SC_HANDLE h, DWORD, DWORD, LPENUM_SERVICE_STATUS data,
                        DWORD size, DWORD *needed, DWORD *count, DWORD *) {
    require(h==reinterpret_cast<void *>(1), "bad synthetic enum");
    *needed=2*sizeof(ENUM_SERVICE_STATUS);
    if (!data) { winError=ERROR_INSUFFICIENT_BUFFER; return FALSE; }
    require(size==*needed, "enum size");
    *count=2;
    for (unsigned i=0;i<2;++i) {
        data[i].lpServiceName=reinterpret_cast<wchar_t *>(const_cast<PA_Unichar *>(fixture.c_str()));
        data[i].ServiceStatus.dwCurrentState=4;
    }
    return TRUE;
}
#define DENY_BOOL(name, signature) BOOL name signature { denied(); return FALSE; }
DENY_BOOL(ControlService, (SC_HANDLE, DWORD, SERVICE_STATUS *))
DENY_BOOL(ChangeServiceConfig, (SC_HANDLE, DWORD, DWORD, DWORD, LPCTSTR, LPCTSTR,
                               DWORD *, LPCTSTR, LPCTSTR, LPCTSTR, LPCTSTR))
SC_HANDLE CreateService(SC_HANDLE, LPCTSTR, LPCTSTR, DWORD, DWORD, DWORD, DWORD,
                         LPCTSTR, LPCTSTR, DWORD *, LPCTSTR, LPCTSTR, LPCTSTR) {
    denied(); return nullptr;
}
DENY_BOOL(DeleteService, (SC_HANDLE))
DENY_BOOL(QueryServiceStatus, (SC_HANDLE, SERVICE_STATUS *))
DENY_BOOL(StartService, (SC_HANDLE, DWORD, const wchar_t **))
DENY_BOOL(QueryServiceObjectSecurity, (SC_HANDLE, DWORD, void *, DWORD, DWORD *))
DENY_BOOL(GetSecurityDescriptorDacl, (PSECURITY_DESCRIPTOR, BOOL *, PACL *, BOOL *))
void BuildExplicitAccessWithName(EXPLICIT_ACCESS *, LPCTSTR, DWORD, DWORD, DWORD) { denied(); }
DWORD SetEntriesInAcl(DWORD, EXPLICIT_ACCESS *, PACL, PACL *) { denied(); return 1; }
DENY_BOOL(InitializeSecurityDescriptor, (SECURITY_DESCRIPTOR *, DWORD))
DENY_BOOL(SetSecurityDescriptorDacl, (SECURITY_DESCRIPTOR *, BOOL, PACL, BOOL))
DENY_BOOL(SetServiceObjectSecurity, (SC_HANDLE, DWORD, SECURITY_DESCRIPTOR *))
HLOCAL LocalFree(HLOCAL) { denied(); return nullptr; }

static void balanced() {
    require(strings.empty(), "unbalanced string allocations");
    require(pictures.empty(), "unbalanced picture references");
    require(handles.empty(), "unbalanced handles");
    puts("strings=0 pictures=0 handles=0 objects=n/a collections=n/a");
}
static void pictureCase(const char *name) {
    {
        CPicture p;
        if (!strcmp(name, "path")) {
            CUTF8String text;
            C_TEXT source; source.setUTF16String(fixture.c_str(), fixture.size());
            source.copyUTF8String(&text);
            p.fromFileAtPath(&text);
        } else if (!strcmp(name, "bytes")) {
            const uint8_t bytes[]={1,2,3}; p.setBytes(bytes, sizeof(bytes));
        } else if (!strcmp(name, "creation-failure")) {
            auto original=p.getPicture();
            pictureCreationFailure=true;
            const uint8_t bytes[]={1,2,3}; p.setBytes(bytes,sizeof(bytes));
            require(PA_GetLastError()==eER_IncompatibleTypes, "creation error lost");
            require(p.getPicture()==original, "failed creation replaced owner");
        } else if (!strcmp(name, "gray")) {
            p.convertToGreyScale();
        } else if (!strcmp(name, "size")) {
            unsigned width=0, height=0; p.getSize(&width, &height);
            if (!commandFailure && !handleFailure)
                require(width==13 && height==17, "size output");
        } else if (!strcmp(name, "thumbnail")) {
            auto result=p.createThumbnail(3, 5); PA_DisposePicture(result);
        } else require(false, "unknown picture test");
        require(pictures.count(p.getPicture()), "wrapper lost retained picture");
    }
    balanced();
}
static void arrayCase() {
    PA_Variable array=PA_CreateVariable(eVK_ArrayUnicode);
    BytePtr params[]={reinterpret_cast<BytePtr>(&array)};
    {
        ARRAY_TEXT text; text.setSize(1);
        text.appendUTF16String(fixture.c_str(), fixture.size());
        for (int repeat=0;repeat<3;++repeat) {
            text.toParamAtIndex(params, 1);
            std::vector<CUTF16String> churn(128, fixture);
            auto value=PA_GetStringInArray(array, 1);
            require(value.fLength==(long)fixture.size(), "array length");
            require(!value.fLength || CUTF16String(value.fString,value.fLength)==fixture,
                    "array content after churn");
            ARRAY_TEXT copy; copy.fromParamAtIndex(params, 1);
            auto replacement=makeString(CUTF16String());
            PA_SetStringInArray(array,1,&replacement);
            CUTF16String read; copy.copyUTF16StringAtIndex(&read,1);
            require(read==fixture, "borrowed array copied before mutation");
        }
        PA_ResizeArray(&array,0);
    }
    PA_ClearVariable(&array);
    {
        ARRAY_TEXT text; text.setSize(1);
        text.appendUTF16String(fixture.c_str(), fixture.size());
        array=PA_CreateVariable(eVK_Undefined);
        text.toParamAtIndex(params,1);
        require(array.fType==eVK_ArrayUnicode && array.uValue.fArray.fNbElements==1,
                "undefined array adoption");
        PA_ClearVariable(&array);
    }
    // Rejected destinations retain the caller's owner; index zero is accepted.
    auto value=makeString(fixture);
    PA_Variable wrong=PA_CreateVariable(eVK_Longint);
    PA_SetStringInArray(wrong,0,&value);
    PA_DisposeUnistring(&value);
    array=PA_CreateVariable(eVK_ArrayUnicode);
    value=makeString(fixture);
    PA_SetStringInArray(array, -1, &value);
    PA_SetStringInArray(array, 1, &value);
    PA_Variable noData=array; noData.uValue.fArray.fData=nullptr;
    PA_SetStringInArray(noData,0,&value);
    PA_SetStringInArray(array,0,nullptr);
    PA_SetStringInArray(array,0,&value);
    PA_ClearVariable(&array);
    balanced();
}
static void scalarCase() {
    {
        C_TEXT text; text.setUTF16String(fixture.c_str(),fixture.size());
        auto value=makeString(CUTF16String());
        BytePtr params[]={reinterpret_cast<BytePtr>(&value)};
        text.toParamAtIndex(params,1);
        require(value.fLength==(long)fixture.size(), "scalar output length");
        C_TEXT copy; copy.fromParamAtIndex(params,1);
        PA_DisposeUnistring(&value);
        require(copy.getUTF16Length()==fixture.size(), "scalar independent copy");
        require(CUTF16String(copy.getUTF16StringPtr(),copy.getUTF16Length())==fixture,
                "scalar independent content");
        value=makeString(CUTF16String());
        sLONG_PTR result=reinterpret_cast<sLONG_PTR>(&value);
        copy.setReturn(&result);
        PA_DisposeUnistring(&value);
    }
    balanced();
}
static void dispatcherCase(bool success) {
    serviceSuccess=success;
    for (int selector : {2,8,17}) {
        if (selector==17) {
            PA_Variable arrays[5];
            BytePtr args[5];
            for (int i=0;i<5;++i) {
                arrays[i]=PA_CreateVariable(i<3 ? eVK_ArrayUnicode : eVK_ArrayLongint);
                args[i]=reinterpret_cast<BytePtr>(&arrays[i]);
            }
            sLONG_PTR result=0;
            CommandDispatcher(selector,&result,args);
#ifdef OWNERSHIP_WINDOWS_DISPATCH
            for (int i=0;i<5;++i)
                require(arrays[i].uValue.fArray.fNbElements==(success ? 2 : 0), "dispatcher rows");
            if (success) {
                auto name=PA_GetStringInArray(arrays[0],1);
                require(CUTF16String(name.fString,name.fLength)==fixture,"dispatcher names");
            }
#else
            require(!serviceCalls, "Mac dispatcher called Windows");
#endif
            for (auto &array:arrays) PA_ClearVariable(&array);
        } else {
            auto input=makeString(fixture), output=makeString(CUTF16String());
            BytePtr args[]={reinterpret_cast<BytePtr>(&input),reinterpret_cast<BytePtr>(&output)};
            sLONG_PTR result=0;
            CommandDispatcher(selector,&result,args);
#ifdef OWNERSHIP_WINDOWS_DISPATCH
            require((int)result==(success ? 0 : 5), "dispatcher status");
            require(output.fLength==(success ? (long)fixture.size() : 0), "dispatcher scalar");
#endif
            PA_DisposeUnistring(&input); PA_DisposeUnistring(&output);
        }
    }
    balanced();
}
template<class T, bool copyable=std::is_copy_constructible<T>::value>
struct CopyCase {
    static void run() {
        require(std::is_copy_assignable<T>::value, "inconsistent copy traits");
        T original;
        { T copy(original); }
        // The original destructor still owns the same allocation/reference.
    }
};
template<class T> struct CopyCase<T, false> {
    static void run() {
        static_assert(!std::is_copy_assignable<T>::value, "assignment must also be disabled");
    }
};
static void traitsCase(const char *name) {
#define CHECK_TYPE(type) if (!strcmp(name,#type)) { CopyCase<type>::run(); balanced(); return; }
    CHECK_TYPE(ARRAY_BOOLEAN)
    CHECK_TYPE(ARRAY_INTEGER)
    CHECK_TYPE(ARRAY_LONGINT)
    CHECK_TYPE(ARRAY_REAL)
    CHECK_TYPE(ARRAY_TEXT)
    CHECK_TYPE(C_BLOB)
    CHECK_TYPE(CPicture)
    CHECK_TYPE(C_PICTURE)
    CHECK_TYPE(C_TEXT)
#undef CHECK_TYPE
    require(!strcmp(name,"values"), "unknown traits test");
    static_assert(std::is_copy_constructible<CBytes>::value, "value bytes remain copyable");
    static_assert(std::is_copy_assignable<CBytes>::value, "value bytes remain assignable");
    static_assert(std::is_copy_constructible<C_DATE>::value, "date copy");
    static_assert(std::is_copy_constructible<C_INTEGER>::value, "integer copy");
    static_assert(std::is_copy_constructible<C_LONGINT>::value, "longint copy");
    static_assert(std::is_copy_constructible<C_REAL>::value, "real copy");
    static_assert(std::is_copy_constructible<C_TIME>::value, "time copy");
    balanced();
}
int main(int argc, char **argv) {
    @autoreleasepool {
        require(argc>=4,"arguments");
        gCall4D=engine;
        nullEmpty=!strcmp(argv[3],"null");
        NSString *text=[NSString stringWithUTF8String:argv[2]];
        fixture.resize([text length]);
        if (!fixture.empty()) [text getCharacters:&fixture[0] range:NSMakeRange(0,[text length])];
        if (argc>4) {
            commandFailure=!strcmp(argv[4],"command");
            handleFailure=!strcmp(argv[4],"handle");
        }
        if (!strcmp(argv[1],"traits")) traitsCase(argv[2]);
        else if (!strcmp(argv[1],"arrays")) arrayCase();
        else if (!strcmp(argv[1],"scalar")) scalarCase();
        else if (!strcmp(argv[1],"dispatch")) dispatcherCase(argc>4 && !strcmp(argv[4],"success"));
        else pictureCase(argv[1]);
    }
}
