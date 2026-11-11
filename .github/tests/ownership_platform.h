#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include <cstring>

// Keep path helpers linked without permitting filesystem/URL operations.
@interface OwnershipURL : NSObject
- (id)initWithString:(NSString *)value;
- (NSString *)absoluteString;
@end
CFURLRef OwnershipCreateURL(CFAllocatorRef, CFStringRef, CFURLPathStyle, Boolean);
CFStringRef OwnershipCopyPath(CFURLRef, CFURLPathStyle);
#define NSURL OwnershipURL
#define CFURLCreateWithFileSystemPath OwnershipCreateURL
#define CFURLCopyFileSystemPath OwnershipCopyPath
