// Import-only fixture: the reported vkGetPhysicalDeviceFeatures2 export is absent.
extern "C" __declspec(dllexport) void* vkGetInstanceProcAddr(void*, const char*) {
    return nullptr;
}
