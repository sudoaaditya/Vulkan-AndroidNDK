#include <android_native_app_glue.h>
#include <android/log.h>

void android_main(struct android_app* app) {
    // code
    __android_log_print(ANDROID_LOG_INFO, "AMK:LOG", "Hello, Vulkan-AndroidNDK!");
}