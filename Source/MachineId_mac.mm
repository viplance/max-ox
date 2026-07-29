#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <string>

std::string getIOPlatformUUID()
{
    std::string uuid;

    io_service_t service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching("IOPlatformExpertDevice"));

    if (service)
    {
        CFStringRef uuidRef = static_cast<CFStringRef>(
            IORegistryEntryCreateCFProperty(
                service,
                CFSTR(kIOPlatformUUIDKey),
                kCFAllocatorDefault, 0));

        if (uuidRef)
        {
            char buffer[128] = {};
            if (CFStringGetCString(uuidRef, buffer, sizeof(buffer),
                                   kCFStringEncodingUTF8))
                uuid = buffer;

            CFRelease(uuidRef);
        }

        IOObjectRelease(service);
    }

    return uuid;
}
