#include <android_native_app_glue.h> // for android_main and all native app glue code
#include <android/log.h> // for __android_log_print

#include <memory.h> // for memset
#include <stdlib.h> // for malloc and free

// vulkan related header files
#define VK_USE_PLATFORM_ANDROID_KHR
#include<vulkan/vulkan.h>

// glm related macros & header files
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // clip space depth range is [0, 1]
#include "../../../../../../glm/glm.hpp"
#include "../../../../../../glm/gtc/matrix_transform.hpp"

#define _ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))

enum {
    AMK_ATTRIBUTE_POSITION = 0,
    AMK_ATTRIBUTE_COLOR = 1,
};

// global variables
typedef struct {
    android_app *app;
    bool bActive;
} Engine;

ANativeWindow *androidNativeWindow = NULL;
AAssetManager *androidAssetManager = NULL;

// event flags
bool bTouchDown = false;
bool bDragging = false;
bool bLongPressDetected = false;
bool bDoubleTapDectected = false;
bool bPendingSingleTap = false;
long touchStartTime = 0;
long pendigSingleTapTime = 0;
float touchStartX = 0.0f;
float touchStartY = 0.0f;
float lastTapX = 0.0f;
float lastTapY = 0.0f;

bool bDone = false;
const char* gpszAppName = "Vulkan Android NDK - Multi-Colored Triangle";
int winWidth = 0;
int winHeight = 0;

// instance extension related variables
uint32_t enabledInstanceExtensionCount = 0; 
// VK_KHR_SURFACE_EXTENSION_NAME & VK_KHR_ANDROID_SURFACE_EXTENSION_NAME & VK_EXT_DEBUG_REPORT_EXTENSION_NAME
const char *enabledInstanceExtensionNames_array[3]; 
// vulkan instance
VkInstance vkInstance = VK_NULL_HANDLE;

//vulkan presentation surface object
VkSurfaceKHR vkSurfaceKHR = VK_NULL_HANDLE;

// vulkan physical device related variables
VkPhysicalDevice vkPhysicalDevice_selected = VK_NULL_HANDLE;
uint32_t graphicsQueueFamilyIndex_selected = UINT32_MAX;
VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;

//
uint32_t physicalDeviceCount = 0;
VkPhysicalDevice *vkPhysicalDevice_array = NULL;

// Device Extension related variables
uint32_t enabledDeviceExtensionCount = 0;
const char *enabledDeviceExtensionNames_array[1]; // VK_KHR_SWAPCHAIN_EXTENSION_NAME

// Vulkan Device
VkDevice vkDevice = VK_NULL_HANDLE;

// Device Queue
VkQueue vkQueue = VK_NULL_HANDLE;

// Surface Format & Surcae ColorSpace
VkFormat vkFormat_color = VK_FORMAT_UNDEFINED;
VkColorSpaceKHR vkColorSpaceKHR = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

// Presentation Mode
VkPresentModeKHR vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;

// Swapchain
VkSwapchainKHR vkSwapchainKHR = VK_NULL_HANDLE;
VkExtent2D vkExtent2D_swapchain;

// Swapchain Images & Image Views
uint32_t swapchainImageCount = UINT32_MAX;
VkImage *swapchainImage_array = NULL;
VkImageView *swapchainImageView_array = NULL;

// Command Pool
VkCommandPool vkCommandPool = VK_NULL_HANDLE;

// Command Buffer
VkCommandBuffer *vkCommandBuffer_array;

// Render Pass
VkRenderPass vkRenderPass = VK_NULL_HANDLE;

// Frame Buffer
VkFramebuffer *vkFramebuffer_array = NULL;

// Fences & Semaphore
VkSemaphore vkSemaphore_backbuffer = VK_NULL_HANDLE;
VkSemaphore vkSemaphore_rendercomplete = VK_NULL_HANDLE;
VkFence *vkFence_array = NULL;

// Build Command Buffers
VkClearColorValue vkClearColorValue;

// Render Variables
bool bInitialized = false;
uint32_t currentImageIndex = UINT32_MAX;

// Validation Layer
bool bValidation = true;
uint32_t enabledValidationLayerCount = 0;
const char *enabledValidationLayerNames_array[1]; //VK_LAYER_KHRONOS_validation
VkDebugReportCallbackEXT vkDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT_fnptr = NULL;

// Vertex Buffer
typedef struct {
    VkBuffer vkBuffer;
    VkDeviceMemory vkDeviceMemory;
} VertexData;

// Position
VertexData vertexData_position;

// Color
VertexData vertexData_color;

// Uniform Related Declarations
struct MyUniformData {
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;
};

typedef struct {
    VkBuffer vkBuffer;
    VkDeviceMemory vkDeviceMemory;
} UniformData;

UniformData uniformData;

// Shader Variables
VkShaderModule vkShaderModule_vertex = VK_NULL_HANDLE;
VkShaderModule vkShaderModule_fragment = VK_NULL_HANDLE;

// Descriptor Set Layout
VkDescriptorSetLayout vkDescriptorSetLayout = VK_NULL_HANDLE;

// Pipeline Layout
VkPipelineLayout vkPipelineLayout = VK_NULL_HANDLE;

// Descriptor Pool
VkDescriptorPool vkDescriptorPool = VK_NULL_HANDLE;

// Descriptor Set
VkDescriptorSet vkDescriptorSet = VK_NULL_HANDLE;

// Pipeline
VkViewport vkViewport;
VkRect2D vkRect2D_scissor;
VkPipeline vkPipeline = VK_NULL_HANDLE;

void engine_handle_cmd(struct android_app*, int32_t);
int32_t engine_handle_input(struct android_app*, AInputEvent*);

void android_main(struct android_app* state) {
    
    // Code

    // function declarations
    VkResult display(void);
    void uninitialize(Engine*);
    void update(void);

    // Variables
    JavaVM *vm = state->activity->vm;
    JNIEnv *env = NULL;
    VkResult vkResult = VK_SUCCESS;

    // attach the current thread to the Java VM
    vm->AttachCurrentThread(&env, NULL);

    // get Running Activity's object
    jobject jObject = state->activity->clazz;
    jclass activityClass = env->GetObjectClass(jObject);
    jclass windowClass = env->FindClass("android/view/Window");
    jclass viewClass = env->FindClass("android/view/View");

    // get window method
    jmethodID getWindowMethod = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    jobject windowObject = env->CallObjectMethod(jObject, getWindowMethod);

    jmethodID getDecorViewMethod = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
    jobject decorViewObject = env->CallObjectMethod(windowObject, getDecorViewMethod);

    // get eight view class static fields for setting system UI visibility
    const int flag_SYSTEM_UI_FLAG_IMMERSIVE = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_IMMERSIVE", "I"));
    const int flag_SYSTEM_UI_FLAG_LAYOUT_STABLE = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_STABLE", "I"));
    const int flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I"));
    const int flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I"));
    const int flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I"));
    const int flag_SYSTEM_UI_FLAG_FULLSCREEN = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I"));
    const int flag_SYSTEM_UI_FLAG_LOW_PROFILE = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LOW_PROFILE", "I"));
    const int flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY = env->GetStaticIntField(viewClass, env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I"));

    // set system UI visibility to hide navigation bar and status bar and make the app full screen and immersive
    int systemUiVisibilityFlags = flag_SYSTEM_UI_FLAG_IMMERSIVE
                                | flag_SYSTEM_UI_FLAG_LAYOUT_STABLE
                                | flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION // These two layout flag are deprecated in latest version so i am facing the brief flash of home screen issue
                                | flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN // to prevent that issue, i'd need to paint something!
                                | flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                | flag_SYSTEM_UI_FLAG_FULLSCREEN
                                | flag_SYSTEM_UI_FLAG_LOW_PROFILE
                                | flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

    jmethodID setSystemUiVisibilityMethod = env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");
    env->CallVoidMethod(decorViewObject, setSystemUiVisibilityMethod, systemUiVisibilityFlags);

    // change to landscape mode
    jclass activityInfoClass = env->FindClass("android/content/pm/ActivityInfo");
    const int flag_SCREEN_ORIENTATION_LANDSCAPE = env->GetStaticIntField(activityInfoClass, env->GetStaticFieldID(activityInfoClass, "SCREEN_ORIENTATION_LANDSCAPE", "I"));
    jmethodID setRequestedOrientationMethod = env->GetMethodID(activityClass, "setRequestedOrientation", "(I)V");
    env->CallVoidMethod(jObject, setRequestedOrientationMethod, flag_SCREEN_ORIENTATION_LANDSCAPE);

    // detach the current thread from the Java VM
    vm->DetachCurrentThread();

    Engine engine;
    memset((void*)&engine, 0, sizeof(Engine));

    //initialize the app's state
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd; // callback for all app commands [ register fn to os ]
    state->onInputEvent = engine_handle_input; // callback for all input events [ register fn to os ]

    engine.app = state;

    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "android_main() started!");

    // loop waiting for stuff to do.
    // Here we will block on the read of events until we have something to do.  
    while (1) {
        int identifier;
        struct android_poll_source *source = NULL;

        while ((identifier = ALooper_pollOnce(engine.bActive ? 0 : -1, NULL, NULL,
                                        (void**)&source)) >= 0) {
            // Process system event.
            if (source != NULL) {
                source->process(state, source);
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:",
                                    "Exiting android_main() from destroy request");
                return;
            }
        }

        //handle longpress
        if(bTouchDown == true && bDragging == false && bLongPressDetected == false && bDoubleTapDectected == false)  {
            struct timespec ts;
            memset(&ts, 0, sizeof(struct timespec));
            clock_gettime(CLOCK_MONOTONIC, &ts); // use system time but use monotoni clock the clock which doesnt decay
            long now = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000); // convert to milliseconds

            if(now - touchStartTime > 500) { // long press is detected after 5000 milliseconds
                bLongPressDetected = true;
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Long Press Detected!");
            }
        }

        // handle single tap
        if(bPendingSingleTap == true && bDoubleTapDectected == false) {
            struct timespec ts;
            memset(&ts, 0, sizeof(struct timespec));
            clock_gettime(CLOCK_MONOTONIC, &ts); // use system time but use monotoni clock the clock which doesnt decay
            long now = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000); // convert to milliseconds

            if(now - pendigSingleTapTime > 300) { // single tap is detected after 300 milliseconds
                bPendingSingleTap = false;
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Single Tap Detected!");
            }
        }

        // Game Loop
        if(engine.bActive == true && bInitialized == true) {
            // display
            vkResult = display();
            if(vkResult != VK_FALSE && vkResult != VK_SUCCESS && vkResult != VK_ERROR_OUT_OF_DATE_KHR && vkResult != VK_SUBOPTIMAL_KHR) {
                __android_log_print(ANDROID_LOG_ERROR, "AMK-Ndk:", "Failed to present the swapchain image!");
            }
            // update
            update();
        }
    }

    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Exiting android_main()!");
    uninitialize(&engine);
    exit(0);
}


// callback for handling app commands
void engine_handle_cmd(struct android_app* app, int32_t cmd) {

    // Code

    // function declarations
    VkResult initialize(void);
    void uninitialize(Engine*);

    // Variables
    Engine *engine = (Engine*)app->userData;
    VkResult vkResult = VK_SUCCESS;

    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            // here we save the state of app when app is paused or stopped
            engine->bActive = false;    
        break;

        case APP_CMD_INIT_WINDOW:
            // here we initialize the window when app is resumed or started
            if (engine->app->window != NULL) {
                androidNativeWindow = engine->app->window;
                androidAssetManager = engine->app->activity->assetManager;
                
                winWidth = ANativeWindow_getWidth(androidNativeWindow);
                winHeight = ANativeWindow_getHeight(androidNativeWindow);

                if(bInitialized == false) {
                    vkResult = initialize();
                    if(vkResult != VK_SUCCESS) {
                        __android_log_print(ANDROID_LOG_ERROR, "AMK-Ndk:", "Failed to initialize Vulkan!");
                    } else {
                        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Initialization Successful!");
                    }
                }
                engine->bActive = true;
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Window Created!");
            } else {
                androidNativeWindow = NULL;
            }
        break;

        case APP_CMD_TERM_WINDOW:
            bDone = true;
            uninitialize(engine);
            // here we terminate the window when app is paused or stopped
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Window Destroyed!");
        break;

        case APP_CMD_GAINED_FOCUS:
            // here we gain focus when app is resumed or started
            engine->bActive = true;
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Window has Gained Focus!");
        break;

        case APP_CMD_LOST_FOCUS:
            // here we lost focus when app is paused or stopped
            engine->bActive = false;
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Window has Lost Focus!");
        break;
    }
}

// callback for handling input events
int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    // Code

    Engine *engine = (Engine*)app->userData;

    int32_t eventType = AInputEvent_getType(event);

    switch(eventType) {
        case AINPUT_EVENT_TYPE_MOTION: 
        {
            int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;

            switch(action) {
                case AMOTION_EVENT_ACTION_DOWN: 
                {
                    touchStartX = AMotionEvent_getX(event, 0);
                    touchStartY = AMotionEvent_getY(event, 0);

                    struct timespec ts;
                    memset(&ts, 0, sizeof(struct timespec));
                    clock_gettime(CLOCK_MONOTONIC, &ts); // use system time but use monotoni clock the clock which doesnt decay
                    touchStartTime = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000); // convert to milliseconds

                    bTouchDown = true;
                    bDragging = false;
                    bLongPressDetected = false;
                    bDoubleTapDectected = false;

                    // check whether the down action is for second tap of double tap or not
                    if(bPendingSingleTap == true) {
                        long timeSinceLastTap = touchStartTime - pendigSingleTapTime;
                        float dx = touchStartX - lastTapX;
                        float dy = touchStartY - lastTapY;
                        float distance = sqrtf(dx * dx + dy * dy);

                        if(timeSinceLastTap < 300.0f && distance < 100.0f) { // double tap is detected if second tap is within 300 milliseconds and within 100 pixels distance from first tap
                            bDoubleTapDectected = true;
                            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Double Tap Detected!");
                        } else {
                            // this is a new touch for a single tap
                            
                        }

                        bPendingSingleTap = false; // reset pending single tap flag
                    }
                }
                break;

                case AMOTION_EVENT_ACTION_MOVE: 
                {
                    float currentX = AMotionEvent_getX(event, 0);
                    float currentY = AMotionEvent_getY(event, 0);
                    float dx = currentX - touchStartX;
                    float dy = currentY - touchStartY;
                    float distance = sqrtf(dx * dx + dy * dy);

                    if(distance > 50.0f) { // consider it as dragging if finger moved more than 50 pixels from initial touch point
                        bDragging = true;
                    }
                }
                break;

                case AMOTION_EVENT_ACTION_UP: 
                {
                    struct timespec ts;
                    memset(&ts, 0, sizeof(struct timespec));
                    clock_gettime(CLOCK_MONOTONIC, &ts); // use system time but use monotoni clock the clock which doesnt decay
                    long now = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000); // convert to milliseconds
                    long duration = now - touchStartTime;

                    float endX = AMotionEvent_getX(event, 0);
                    float endY = AMotionEvent_getY(event, 0);

                    float dx = endX - touchStartX;
                    float dy = endY - touchStartY;
                    float distance = sqrtf(dx * dx + dy * dy);

                    bTouchDown = false;

                    if(bDoubleTapDectected == true) {
                        // this up belongs to second tap of double tap which we have already handled in down action
                    }
                    else if(bLongPressDetected == true) {
                        // this up belongs to long press which we have already handled in move action
                    }
                    else if(bDragging == true && distance > 150.0f) {
                        // swipe is detected
                        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Swipe Detected!");
                        ANativeActivity_finish(engine->app->activity); // close the app when swipe is detected
                    }
                    else if(duration < 300 && distance < 50.0f) { 
                        // consider it as single tap if finger lifted within 300 milliseconds and moved less than 50 pixels from initial touch point
                        bPendingSingleTap = true;
                        pendigSingleTapTime = now;
                        lastTapX = endX;
                        lastTapY = endY;
                    }

                    bDragging = false;
                }
                break;
            }
        }
        break;
    }

    return 0;
}

VkResult initialize(void) {

    // function declarations
    VkResult createVulkanInstance(void);
    VkResult getSupportedSurface(void);
    VkResult getPhysicalDevice(void);
    VkResult printVKInfo(void);
    VkResult createVulkanDevice(void);
    void getDeviceQueue(void);
    VkResult createSwapchain(VkBool32);
    VkResult createSwapchainImagesAndImageViews(void);
    VkResult createCommandPool(void);
    VkResult createCommandBuffers(void);
    VkResult createVertexBuffer(void);
    VkResult createUniformBuffer(void);
    VkResult createShaders(void);
    VkResult createDescriptorSetLayout(void);
    VkResult createPipelineLayout(void);
    VkResult createDescriptorPool(void);
    VkResult createDescriptorSet(void);
    VkResult createRenderPass(void);
    VkResult createPipeline(void);
    VkResult createFramebuffers(void);
    VkResult createSemaphores(void);
    VkResult createFences(void);
    VkResult buildCommandBuffers(void);


    // varibales
    VkResult vkResult = VK_SUCCESS;

    // code
    vkResult = createVulkanInstance();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createVulkanInstance() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createVulkanInstance() Successful!.\n\n");
    }

    // create vulkan presentation surface
    vkResult = getSupportedSurface();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): getSupportedSurface() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): getSupportedSurface() Successful!.\n\n");
    }

    // Get Physical Device, enumerate and select it's queue family index
    vkResult = getPhysicalDevice();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): getPhysicalDevice() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): getPhysicalDevice() Successful!.\n\n");
    }

    // Print Vulkan Info
    vkResult = printVKInfo();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): printVKInfo() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): printVKInfo() Successful!.\n\n");
    }

    vkResult = createVulkanDevice();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createVulkanDevice() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createVulkanDevice() Successful!.\n\n");
    }

    // Device Queue
    getDeviceQueue();

    // Swapchain
    vkResult = createSwapchain(VK_FALSE);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createSwapchain() Failed!.\n");
        return (VK_ERROR_INITIALIZATION_FAILED);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createSwapchain() Successful!.\n\n");
    }

    // Swapchain Images & Image Views
    vkResult = createSwapchainImagesAndImageViews();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createSwapchainImagesAndImageViews() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createSwapchainImagesAndImageViews() Successful!.\n\n");
    }

    // Command Pool
    vkResult = createCommandPool();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createCommandPool() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createCommandPool() Successful!.\n\n");
    }

    // Command Buffer
    vkResult = createCommandBuffers();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createCommandBuffers() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createCommandBuffers() Successful!.\n\n");
    }

    // Create Vertex Buffer
    vkResult = createVertexBuffer();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createVertexBuffer() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createVertexBuffer() Successful!.\n\n");
    }

    // Create Uniform Buffer
    vkResult = createUniformBuffer();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createUniformBuffer() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createUniformBuffer() Successful!.\n\n");
    }


    // Create Shaders
    vkResult = createShaders();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createShaders() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createShaders() Successful!.\n\n");
    }

    // Create Descriptor Set Layout
    vkResult = createDescriptorSetLayout();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createDescriptorSetLayout() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createDescriptorSetLayout() Successful!.\n\n");
    }

    // Create Pipeline Layout
    vkResult = createPipelineLayout();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createPipelineLayout() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createPipelineLayout() Successful!.\n\n");
    }

    // Create Descriptor Pool
    vkResult = createDescriptorPool();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createDescriptorPool() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createDescriptorPool() Successful!.\n\n");
    }

    // Create Descriptor Set
    vkResult = createDescriptorSet();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createDescriptorSet() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createDescriptorSet() Successful!.\n\n");
    }

    // Render Pass
    vkResult = createRenderPass();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createRenderPass() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createRenderPass() Successful!.\n\n");
    }

    // Pipeline
    vkResult = createPipeline();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createPipeline() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createPipeline() Successful!.\n\n");
    }

    // Framebuffers
    vkResult = createFramebuffers();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createFramebuffers() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createFramebuffers() Successful!.\n\n");
    }

    // Create Semaphores
    vkResult = createSemaphores();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createSemaphores() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createSemaphores() Successful!.\n\n");
    }

    // Create Fences
    vkResult = createFences();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createFences() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): createFences() Successful!.\n\n");
    }

    // initialize clear color values
    memset((void*)&vkClearColorValue, 0, sizeof(VkClearColorValue));
    vkClearColorValue.float32[0] = 0.0f;
    vkClearColorValue.float32[1] = 0.0f;
    vkClearColorValue.float32[2] = 0.0f;
    vkClearColorValue.float32[3] = 1.0f; // analogous to glClearColor

    // Build Command Buffers
    vkResult = buildCommandBuffers();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): buildCommandBuffers() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): buildCommandBuffers() Successful!.\n\n");
    }

    // Initialization is completed!
    bInitialized = true;
    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "initialize(): Initialization Successful!.\n");

    return (vkResult);
}

VkResult resize(int width, int height) {
    // Function declarations
    VkResult createSwapchain(VkBool32);
    VkResult createSwapchainImagesAndImageViews(void);
    VkResult createCommandBuffers(void);
    VkResult createPipelineLayout(void);
    VkResult createPipeline(void);
    VkResult createFramebuffers(void);
    VkResult createRenderPass(void);
    VkResult buildCommandBuffers(void);


    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Code
    if(height <= 0)
        height = 1;

    // If control comes here before initialization is done, then return false
    if(bInitialized == false) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): initialization is not completed or failed\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    }

    // As recreation of swapchain is required, we are going to repeat many steps of initialization again
    // hence set bInitialized to false
    bInitialized = false; // this will prevent display() function to execute before resize() is done

    // Set Global Width & Height
    winWidth = width;
    winHeight = height;

    // Wait til vkDevice is idle
    if(vkDevice) {
        vkDeviceWaitIdle(vkDevice); // this basically waits on til all the operations are done using the device and then this function call returns
    }

    // Check if vkSwapchainKHR is NULL, if it is NULL then we cannot proceed
    if(vkSwapchainKHR == VK_NULL_HANDLE) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): vkSwapchainKHR is NULL cannot proceed!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    }

    // Destroy Frame Buffers
    if(vkFramebuffer_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            vkDestroyFramebuffer(vkDevice, vkFramebuffer_array[i], NULL);
            vkFramebuffer_array[i] = VK_NULL_HANDLE;
        }
    }

    if(vkFramebuffer_array) {
        free(vkFramebuffer_array);
        vkFramebuffer_array = NULL;
    }

    // Destroy  Command Buffers
    if(vkCommandBuffer_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkCommandBuffer_array[i]);
            vkCommandBuffer_array[i] = VK_NULL_HANDLE;
        }
    }

    if(vkCommandBuffer_array) {
        free(vkCommandBuffer_array);
        vkCommandBuffer_array = NULL;
    }

    // Destroy Pipeline
    if(vkPipeline) {
        vkDestroyPipeline(vkDevice, vkPipeline, NULL);
        vkPipeline = VK_NULL_HANDLE;
    }

    // Destroy Pipeline Layout
    if(vkPipelineLayout) {
        vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, NULL);
        vkPipelineLayout = VK_NULL_HANDLE;
    }

    // Destroy Render Pass
    if(vkRenderPass) {
        vkDestroyRenderPass(vkDevice, vkRenderPass, NULL);
        vkRenderPass = VK_NULL_HANDLE;
    }

    if(swapchainImageView_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            if(swapchainImageView_array[i]) {
                vkDestroyImageView(vkDevice, swapchainImageView_array[i], NULL);
                swapchainImageView_array[i] = VK_NULL_HANDLE;
            }
        }
    }

    if(swapchainImageView_array) {
        free(swapchainImageView_array);
        swapchainImageView_array = NULL;
    }

    // Destroy vulkan Images
    // VALIDATION USE CASE 4: uncomment the given block to see the error
    /* if(swapchainImage_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            if(swapchainImage_array[i]) {
                vkDestroyImage(vkDevice, swapchainImage_array[i], NULL);
                swapchainImage_array[i] = VK_NULL_HANDLE;
            }
        }
    } */

    if(swapchainImage_array) {
        free(swapchainImage_array);
        swapchainImage_array = NULL;
    }

    // Destroy Swapchain
    if(vkSwapchainKHR) {
        vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, NULL);
        vkSwapchainKHR = VK_NULL_HANDLE;
    }

    // RECREATE FOR RESIZE
    // Create Swapchain
    vkResult = createSwapchain(VK_TRUE);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): createSwapchain() Failed!.\n");
        return (VK_ERROR_INITIALIZATION_FAILED);
    }

    // Create Swapchain Images & Image Views
    vkResult = createSwapchainImagesAndImageViews();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): createSwapchainImagesAndImageViews() Failed!.\n");
        return (vkResult);
    }

    // Create Render Pass
    vkResult = createRenderPass();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): createRenderPass() Failed!.\n");
        return (vkResult);
    }

    // Create Pipeline Layout
    vkResult = createPipelineLayout();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): createPipelineLayout() Failed!.\n");
        return (vkResult);
    }

    // Create Pipeline
    vkResult = createPipeline();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): createPipeline() Failed!.\n");
        return (vkResult);
    }

    // Create Framebuffers
    vkResult = createFramebuffers();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): createFramebuffers() Failed!.\n");
        return (vkResult);
    }
    
    // Create Command Buffer
    vkResult = createCommandBuffers();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): createCommandBuffers() Failed!.\n");
        return (vkResult);
    }

    // Build Command Buffers
    vkResult = buildCommandBuffers();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "resize(): buildCommandBuffers() Failed!.\n");
        return (vkResult);
    }

    // add extra new line for better readability
    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "\n\n");

    bInitialized = true;

    return (vkResult);
}

VkResult display(void) {

    // Function declarations
    VkResult resize(int, int);
    VkResult updateUniformBuffer(void);

    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Code
    //if control comes here before initialization is done, then return false
    if(bInitialized == false) {
        vkResult = (VkResult)VK_FALSE;
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): bInitialized is false!.\n");
        return (vkResult);
    }

    // Acquire index of next swapchain image
    vkResult = vkAcquireNextImageKHR(
        vkDevice, 
        vkSwapchainKHR,
        UINT64_MAX, // timeout in nanoseconds
        vkSemaphore_backbuffer,
        VK_NULL_HANDLE,
        &currentImageIndex
    );

    if(vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR) {
        if(vkResult == VK_ERROR_OUT_OF_DATE_KHR) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): vkAcquireNextImageKHR() Failed! Swapchain is out of date.\n");
            // Resize the swapchain
            vkResult = resize(winWidth, winHeight);
            if(vkResult != VK_SUCCESS) {
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): resize() Failed!.\n");
                return (vkResult);
            }
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): vkAcquireNextImageKHR() Failed!.\n");
            return (vkResult);
        }
    }

    // Use Fence to allow host to wait for complition of execution of prev command buffer
    vkResult = vkWaitForFences(vkDevice, 1, &vkFence_array[currentImageIndex], VK_TRUE, UINT64_MAX);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): vkWaitForFences() Failed!.\n");
        return (vkResult);
    }

    // Now ready the facnces for execution of next command buffer
    vkResult = vkResetFences(vkDevice, 1, &vkFence_array[currentImageIndex]);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): vkResetFences() Failed!.\n");
        return (vkResult);
    }

    // One of the memeber of vkSubmitInfo structure requires array of pipeline stages, we haveonly one have of 
    // complition of color attachment, so we need to create array of size 1
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // declare, memset & initialize vkSubmitInfo structure
    VkSubmitInfo vkSubmitInfo;
    memset((void*)&vkSubmitInfo, 0, sizeof(VkSubmitInfo));

    vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkSubmitInfo.pNext = NULL;
    vkSubmitInfo.pWaitDstStageMask = &waitDstStageMask;
    vkSubmitInfo.waitSemaphoreCount = 1;
    vkSubmitInfo.pWaitSemaphores = &vkSemaphore_backbuffer;
    vkSubmitInfo.commandBufferCount = 1;
    vkSubmitInfo.pCommandBuffers = &vkCommandBuffer_array[currentImageIndex];
    vkSubmitInfo.signalSemaphoreCount = 1;
    vkSubmitInfo.pSignalSemaphores = &vkSemaphore_rendercomplete;

    // Now submit command buffer to queue for execution
    vkResult = vkQueueSubmit(vkQueue, 1, &vkSubmitInfo, vkFence_array[currentImageIndex]);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): vkQueueSubmit() Failed!.\n");
        return (vkResult);
    }

    // We are going to present rendered image after declaring & initializing vkPresentInfoKHR structure
    VkPresentInfoKHR vkPresentInfoKHR;
    memset((void*)&vkPresentInfoKHR, 0, sizeof(VkPresentInfoKHR));

    vkPresentInfoKHR.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    vkPresentInfoKHR.pNext = NULL;
    vkPresentInfoKHR.waitSemaphoreCount = 1;
    vkPresentInfoKHR.pWaitSemaphores = &vkSemaphore_rendercomplete;
    vkPresentInfoKHR.swapchainCount = 1;
    vkPresentInfoKHR.pSwapchains = &vkSwapchainKHR;
    vkPresentInfoKHR.pImageIndices = &currentImageIndex;
    vkPresentInfoKHR.pResults = NULL; // this is optional, so we are not using it

    // Present the queue!
    vkResult = vkQueuePresentKHR(vkQueue, &vkPresentInfoKHR);
    if(vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR) {
        if(vkResult == VK_ERROR_OUT_OF_DATE_KHR) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): vkQueuePresentKHR() Failed! Swapchain is out of date.\n");
            // Resize the swapchain
            vkResult = resize(winWidth, winHeight);
            if(vkResult != VK_SUCCESS) {
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): resize() Failed!.\n");
                return (vkResult);
            }
        }
        else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): vkQueuePresentKHR() Failed!.\n");
            return (vkResult);
        }
    }

    // Update Uniform Buffer
    vkResult = updateUniformBuffer();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "display(): updateUniformBuffer() Failed!.\n");
        return (vkResult);
    }

    vkDeviceWaitIdle(vkDevice); // VALIDATION USE CASE 1: Comment this line to see the error

    return (vkResult);
}

void uninitialize(Engine* engine) {

    bInitialized = false;

    if(androidNativeWindow) {
        ANativeWindow_release(androidNativeWindow);
        androidNativeWindow = NULL;
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): ANativeWindow Released!.\n");
    }

    if(engine->bActive) {
        memset((void *)engine, 0, sizeof(Engine));
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): Engine Released!.\n");
    }

    // wait til vkDevice is idle
    if(vkDevice) {
        vkDeviceWaitIdle(vkDevice); // this basically waits on til all the operations are done using the device and then this function call returns
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "\nuninitialize(): vkDeviceWaitIdle is done!\n");
    }

    // Destroy Fence
    // VALIDATION USE CASE 3: Comment this line to see the error
    if(vkFence_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            vkDestroyFence(vkDevice, vkFence_array[i], NULL);
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyFence() Succeed for {%d}!.\n", i);
            vkFence_array[i] = VK_NULL_HANDLE;
        }
    }

    if(vkFence_array) {
        free(vkFence_array);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): freed vkFence_array!.\n");
        vkFence_array = NULL;
    }

    // Destroy Semaphore
    if(vkSemaphore_rendercomplete) {
        vkDestroySemaphore(vkDevice, vkSemaphore_rendercomplete, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroySemaphore() for Render Complete Succeed!\n");
        vkSemaphore_rendercomplete = VK_NULL_HANDLE;
    }

    if(vkSemaphore_backbuffer) {
        vkDestroySemaphore(vkDevice, vkSemaphore_backbuffer, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroySemaphore() for Back Buffer Succeed!\n");
        vkSemaphore_backbuffer = VK_NULL_HANDLE;
    }

    // Destroy Frame Buffers
    if(vkFramebuffer_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            vkDestroyFramebuffer(vkDevice, vkFramebuffer_array[i], NULL);
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyFramebuffer() Succeed for {%d}!.\n", i);
            vkFramebuffer_array[i] = VK_NULL_HANDLE;
        }
    }

    if(vkFramebuffer_array) {
        free(vkFramebuffer_array);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): freed vkFramebuffer_array!.\n");
        vkFramebuffer_array = NULL;
    }

    // Destroy Pipeline
    if(vkPipeline) {
        vkDestroyPipeline(vkDevice, vkPipeline, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyPipeline() Succeed!\n");
        vkPipeline = VK_NULL_HANDLE;
    }

    // Destroy Render Pass
    if(vkRenderPass) {
        vkDestroyRenderPass(vkDevice, vkRenderPass, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyRenderPass() Succeed!\n");
        vkRenderPass = VK_NULL_HANDLE;
    }

    // Destroy Descriptor Pool
    // When descriptor pool is destroyed, all the descriptor sets created from it are destroyed internally
    // so we  don't need to destroy descriptor set explicitly 
    if(vkDescriptorPool) {
        vkDestroyDescriptorPool(vkDevice, vkDescriptorPool, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDescriptorPool & vkDescriptorSet Destroy Succeed!\n");
        vkDescriptorPool = VK_NULL_HANDLE;
    }

    // Destroy Pipeline Layout
    if(vkPipelineLayout) {
        vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyPipelineLayout() Succeed!\n");
        vkPipelineLayout = VK_NULL_HANDLE;
    }

    // Destroy Descriptor Set Layout
    if(vkDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(vkDevice, vkDescriptorSetLayout, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyDescriptorSetLayout() Succeed!\n");
        vkDescriptorSetLayout = VK_NULL_HANDLE;
    }

    // Destroy Shader
    if(vkShaderModule_fragment) {
        vkDestroyShaderModule(vkDevice, vkShaderModule_fragment, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyShaderModule() Succeed for Fragment Shader!\n");
        vkShaderModule_fragment = VK_NULL_HANDLE;
    }

    if(vkShaderModule_vertex) {
        vkDestroyShaderModule(vkDevice, vkShaderModule_vertex, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyShaderModule() Succeed for Vertex Shader!\n");
        vkShaderModule_vertex = VK_NULL_HANDLE;
    }

    // Destroy Uniform Buffer
    if(uniformData.vkDeviceMemory) {
        vkFreeMemory(vkDevice, uniformData.vkDeviceMemory, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkFreeMemory() Succeed for Uniform Buffer!\n");
        uniformData.vkDeviceMemory = VK_NULL_HANDLE;
    }

    if(uniformData.vkBuffer) {
        vkDestroyBuffer(vkDevice, uniformData.vkBuffer, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyBuffer() Succeed for Uniform Buffer!\n");
        uniformData.vkBuffer = VK_NULL_HANDLE;
    }

    // Destroy Vertex Buffer Color
    if(vertexData_color.vkDeviceMemory) {
        vkFreeMemory(vkDevice, vertexData_color.vkDeviceMemory, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkFreeMemory() Succeed for Vertex Buffer for Color!\n");
        vertexData_color.vkDeviceMemory = VK_NULL_HANDLE;
    }

    if(vertexData_color.vkBuffer) {
        vkDestroyBuffer(vkDevice, vertexData_color.vkBuffer, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyBuffer() Succeed for Vertex Buffer for Color!\n");
        vertexData_color.vkBuffer = VK_NULL_HANDLE;
    }

    // Destroy Vertex Buffer
    if(vertexData_position.vkDeviceMemory) {
        vkFreeMemory(vkDevice, vertexData_position.vkDeviceMemory, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkFreeMemory() Succeed for Vertex Buffer!\n");
        vertexData_position.vkDeviceMemory = VK_NULL_HANDLE;
    }

    if(vertexData_position.vkBuffer) {
        vkDestroyBuffer(vkDevice, vertexData_position.vkBuffer, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyBuffer() Succeed for Vertex Buffer!\n");
        vertexData_position.vkBuffer = VK_NULL_HANDLE;
    }

    // Destroy  Command Buffers
    if(vkCommandBuffer_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            vkFreeCommandBuffers(vkDevice, vkCommandPool, 1, &vkCommandBuffer_array[i]);
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkFreeCommandBuffers() Succeed for {%d}\n", i);
            vkCommandBuffer_array[i] = VK_NULL_HANDLE;
        }
    }

    if(vkCommandBuffer_array) {
        free(vkCommandBuffer_array);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): freed vkCommandBuffer_array!.\n");
        vkCommandBuffer_array = NULL;
    }

    // Destroy the command pool
    if(vkCommandPool) {
        vkDestroyCommandPool(vkDevice, vkCommandPool, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyCommandPool Successful!.\n");
        vkCommandPool = VK_NULL_HANDLE;
    }

    // Destroy Vulkan Swapchain Image Views
    if(swapchainImageView_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            if(swapchainImageView_array[i]) {
                vkDestroyImageView(vkDevice, swapchainImageView_array[i], NULL);
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyImageView() Succeed for {%d}\n", i);
                swapchainImageView_array[i] = VK_NULL_HANDLE;
            }
        }
    }

    if(swapchainImageView_array) {
        free(swapchainImageView_array);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): freed swapchainImageView_array!.\n");
        swapchainImageView_array = NULL;
    }

    // Destroy vulkan Images
    // VALIDATION USE CASE 4: uncomment the given block to see the error
    /* if(swapchainImage_array) {
        for(uint32_t i = 0; i < swapchainImageCount; i++) {
            if(swapchainImage_array[i]) {
                vkDestroyImage(vkDevice, swapchainImage_array[i], NULL);
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyImage() Succeed for {%d}\n", i);
                fflush(fptr);
                swapchainImage_array[i] = VK_NULL_HANDLE;
            }
        }
    } */

    if(swapchainImage_array) {
        free(swapchainImage_array);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): freed swapchainImage_array!.\n");
        swapchainImage_array = NULL;
    }


    // Destroy Vulkan Swapchain
    if(vkSwapchainKHR) {
        vkDestroySwapchainKHR(vkDevice, vkSwapchainKHR, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroySwapchainKHR() Succeed!\n");
        vkSwapchainKHR = VK_NULL_HANDLE;
    }
    
    // No need to destroy device queue

    // Destroy Vulkan Device
    if(vkDevice) {
        vkDestroyDevice(vkDevice, NULL);
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "uninitialize(): vkDestroyDevice() Succeed!\n");
        vkDevice = VK_NULL_HANDLE;
    }
    
    //No need to destroy selected physical device!

    // destroy surface
    if(vkSurfaceKHR) {
        vkDestroySurfaceKHR(vkInstance, vkSurfaceKHR, NULL);
        vkSurfaceKHR = VK_NULL_HANDLE;
		__android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:","uninitialize(): vkDestroySurfaceKHR() Succeed\n");
    }

    if(vkDebugReportCallbackEXT && vkDestroyDebugReportCallbackEXT_fnptr) {
        vkDestroyDebugReportCallbackEXT_fnptr(vkInstance, vkDebugReportCallbackEXT, NULL);
        vkDebugReportCallbackEXT = VK_NULL_HANDLE;
        vkDestroyDebugReportCallbackEXT_fnptr = NULL;
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:","uninitialize(): vkDestroyDebugReportCallbackEXT_fnptr() Succeed\n");
    }

    // destroy vkInstance
    if(vkInstance) {
        vkDestroyInstance(vkInstance, NULL);
        vkInstance = VK_NULL_HANDLE;
		__android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:","uninitialize(): vkDestroyInstance() Succeed\n");
    }
}

void update(void) {

}

//! //////////////////////////////////////// Definations of vulkan Related Functions ///////////////////////////////////////////////

VkResult createVulkanInstance (void) {
    // function declarations
    VkResult fillInstanceExtensionNames(void);
    VkResult fillValidationLayerNames(void);
    VkResult createValidationCallbackFunction(void);

    // varibales
    VkResult vkResult = VK_SUCCESS;

    // code
    vkResult = fillInstanceExtensionNames();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): fillInstanceExtensionNames() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): fillInstanceExtensionNames() Successful!.\n\n");
    }

    if(bValidation == true) {
        //fill validation layers names
        vkResult = fillValidationLayerNames();
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): fillValidationLayerNames() Failed!.\n");
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): fillValidationLayerNames() Successful!.\n");
        }
    }

    // step 2:
    VkApplicationInfo vkApplicationInfo;
    memset((void*)&vkApplicationInfo, 0, sizeof(VkApplicationInfo));

    vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkApplicationInfo.pNext = NULL;
    vkApplicationInfo.pApplicationName = gpszAppName;
    vkApplicationInfo.applicationVersion = 1;
    vkApplicationInfo.pEngineName = gpszAppName;
    vkApplicationInfo. engineVersion = 1;
    vkApplicationInfo.apiVersion = VK_API_VERSION_1_3;  // target Vulkan 1.3 for broader device support

    // Step 3: initialize struct VkInstanceCreateInfo
    VkInstanceCreateInfo vkInstanceCreateInfo;
    memset((void*)&vkInstanceCreateInfo, 0, sizeof(VkInstanceCreateInfo));

    vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInstanceCreateInfo.pNext = NULL;
    vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
    vkInstanceCreateInfo.enabledExtensionCount = enabledInstanceExtensionCount;
    vkInstanceCreateInfo.ppEnabledExtensionNames = enabledInstanceExtensionNames_array;

    // if validation layer is enabled/valid then fill data else keep it null
    if(bValidation == true) {
        vkInstanceCreateInfo.enabledLayerCount = enabledValidationLayerCount;
        vkInstanceCreateInfo.ppEnabledLayerNames = enabledValidationLayerNames_array;
    } else {
        vkInstanceCreateInfo.enabledLayerCount = 0;
        vkInstanceCreateInfo.ppEnabledLayerNames = NULL;
    }

    // Step 4: Create instance using vkCreateInstance
    vkResult = vkCreateInstance(&vkInstanceCreateInfo, NULL, &vkInstance);
    if(vkResult == VK_ERROR_INCOMPATIBLE_DRIVER) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): vkCreateInstance() Failed Due to Incompatible Driver (%d)!.\n", vkResult);
        return (vkResult);
    } else if(vkResult == VK_ERROR_EXTENSION_NOT_PRESENT) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): vkCreateInstance() Failed Due to Extention Not Present (%d)!.\n", vkResult);
        return (vkResult);
    } else if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): vkCreateInstance() Failed Due to Unknown Reason (%d)!.\n", vkResult);
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): vkCreateInstance() Successful!.\n\n");
    }

    // Step 5: Create Validation Layer Callback Function [ Do this for validation callbaaks ]
    if(bValidation == true) {
        vkResult = createValidationCallbackFunction();
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): createValidationCallbackFunction() Failed!.\n");
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanInstance(): createValidationCallbackFunction() Successful!.\n\n");
        }
    }

    return (vkResult);

}

VkResult fillInstanceExtensionNames (void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // Step 1: Find how many instance extension are supported by this vulkan driver & keep it in local variable
    uint32_t instanceExtensionCount = 0;

    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, NULL);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): vkEnumerateInstanceExtensionProperties() First Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): vkEnumerateInstanceExtensionProperties() First Call Successful!.\n");
    }

    // step 2: Allocate & fill struct vk Extenstions array correspoinding to above acount
    VkExtensionProperties *vkExtensionProperties_array = NULL;
    vkExtensionProperties_array = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * instanceExtensionCount);
    vkResult = vkEnumerateInstanceExtensionProperties(NULL, &instanceExtensionCount, vkExtensionProperties_array);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): vkEnumerateInstanceExtensionProperties() Second Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): vkEnumerateInstanceExtensionProperties() Second Call Successful!.\n");
    }

    // Step 3: fill all supoorted extensions names in array of char pointers
    char **instanceExtensionNames_array = NULL;
    instanceExtensionNames_array = (char**)malloc(sizeof(char*) * instanceExtensionCount);
    for(uint32_t i = 0; i < instanceExtensionCount; i++) {
        instanceExtensionNames_array[i] = (char*)malloc(sizeof(char) * strlen(vkExtensionProperties_array[i].extensionName) + 1);
        memcpy(
            instanceExtensionNames_array[i], 
            vkExtensionProperties_array[i].extensionName, 
            strlen(vkExtensionProperties_array[i].extensionName) + 1
        );
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): Vulkan Extension Name = %s \n", instanceExtensionNames_array[i]);
    }

    // step 4:
    free(vkExtensionProperties_array);

    // step 5
    VkBool32 surfaceExtensionFound = VK_FALSE;
    VkBool32 androidvulkanSurfaceExtensionFound = VK_FALSE;
    VkBool32 debugReportExtensionFound = VK_FALSE;
    for(uint32_t i = 0; i < instanceExtensionCount; i++) {
        if(strcmp(instanceExtensionNames_array[i], VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
            surfaceExtensionFound = VK_TRUE;
            enabledInstanceExtensionNames_array[enabledInstanceExtensionCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
        }
        if(strcmp(instanceExtensionNames_array[i], VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) ==  0) {
            androidvulkanSurfaceExtensionFound = VK_TRUE;
            enabledInstanceExtensionNames_array[enabledInstanceExtensionCount++] = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
        }
        if(strcmp(instanceExtensionNames_array[i], VK_EXT_DEBUG_REPORT_EXTENSION_NAME) ==  0) {
            debugReportExtensionFound = VK_TRUE;
            if(bValidation == true) {
                enabledInstanceExtensionNames_array[enabledInstanceExtensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            } else {
                // array will not have entry of VK_EXT_DEBUG_REPORT_EXTENSION_NAME
            }
        }
    }

    // step 6
    for(uint32_t i = 0; i < instanceExtensionCount; i++) {
        free(instanceExtensionNames_array[i]);
    }
    free(instanceExtensionNames_array);

    // step 7:
    if(surfaceExtensionFound == VK_FALSE) {
        vkResult = VK_ERROR_INITIALIZATION_FAILED; // return hardcoded failure
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME Not Found!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): VK_KHR_SURFACE_EXTENSION_NAME Found!.\n");
    }

    if(androidvulkanSurfaceExtensionFound == VK_FALSE) {
        vkResult = VK_ERROR_INITIALIZATION_FAILED; // return hardcoded failure
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME Not Found!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): VK_KHR_WIN32_SURFACE_EXTENSION_NAME Found!.\n");
    }

    if(debugReportExtensionFound == VK_FALSE) {
        if(bValidation == true) {
            vkResult = VK_ERROR_INITIALIZATION_FAILED; // return hardcoded failure
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): Validation is ON but VK_EXT_DEBUG_REPORT_EXTENSION_NAME Not Supported!.\n");
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): Validation is OFF and VK_EXT_DEBUG_REPORT_EXTENSION_NAME Not Supported!.\n");
        }
    } else {
        if(bValidation == true) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): Validation is ON but VK_EXT_DEBUG_REPORT_EXTENSION_NAME is Supported!.\n");
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): Validation is OFF and VK_EXT_DEBUG_REPORT_EXTENSION_NAME is Supported!.\n");
        }
    }

    // step 8: print all the supported extensions
    for(uint32_t i = 0; i < enabledInstanceExtensionCount; i++) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillInstanceExtensionNames(): Enabled Vulkan Instance Extension Name = %s \n", enabledInstanceExtensionNames_array[i]);
    }

    return vkResult;
}

VkResult fillValidationLayerNames(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // code
    // step 1: Find how many validation layers are supported by this vulkan driver & keep it in local variable
    uint32_t validationLayerCount = 0;

    vkResult = vkEnumerateInstanceLayerProperties(&validationLayerCount, NULL);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): vkEnumerateInstanceLayerProperties() First Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): vkEnumerateInstanceLayerProperties() First Call Successful!.\n");
    }

    // step 2: Allocate & fill struct vk Validation Layers array correspoinding to above acount
    VkLayerProperties *vkLayerProperties_array = NULL;
    vkLayerProperties_array = (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * validationLayerCount);
    vkResult = vkEnumerateInstanceLayerProperties(&validationLayerCount, vkLayerProperties_array);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): vkEnumerateInstanceLayerProperties() Second Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): vkEnumerateInstanceLayerProperties() Second Call Successful!.\n");
    }

    // step 3: fill all supoorted layers names in array of char pointers
    char **validationLayerNames_array = NULL;
    validationLayerNames_array = (char**)malloc(sizeof(char*) * validationLayerCount);
    for(uint32_t i = 0; i < validationLayerCount; i++) {
        validationLayerNames_array[i] = (char*)malloc(sizeof(char) * strlen(vkLayerProperties_array[i].layerName) + 1);
        memcpy(
            validationLayerNames_array[i], 
            vkLayerProperties_array[i].layerName, 
            strlen(vkLayerProperties_array[i].layerName) + 1
        );
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): Vulkan Validation Layer Name = %s \n", validationLayerNames_array[i]);
    }

    // step 4: free vkLayerProperties_array
    free(vkLayerProperties_array);

    // step 5: check if validation layer is supported or not
    VkBool32 validationLayerFound = VK_FALSE;
    for(uint32_t i = 0; i < validationLayerCount; i++) {
        if(strcmp(validationLayerNames_array[i], "VK_LAYER_KHRONOS_validation") == 0) {
            validationLayerFound = VK_TRUE;
            enabledValidationLayerNames_array[enabledValidationLayerCount++] = "VK_LAYER_KHRONOS_validation";
        }
    }

    // step 6
    for(uint32_t i = 0; i < validationLayerCount; i++) {
        free(validationLayerNames_array[i]);
    }
    free(validationLayerNames_array);

    // step 7
    if(validationLayerFound == VK_FALSE) {
        vkResult = VK_ERROR_INITIALIZATION_FAILED; // return hardcoded failure
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): VK_LAYER_KHRONOS_validation Not Supported!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): VK_LAYER_KHRONOS_validation Supported!.\n");
    }

    // step 8: print all the supported layers
    for(uint32_t i = 0; i < enabledValidationLayerCount; i++) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillValidationLayerNames(): Enabled Vulkan Validation Layer Name = %s \n", enabledValidationLayerNames_array[i]);
    }

    return (vkResult);
}

VkResult createValidationCallbackFunction(void) {
    // function declaratons
    VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
        VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT,
        uint64_t, size_t, int32_t, const char*, const char*,
        void*
    );

    // variables
    VkResult vkResult = VK_SUCCESS;
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT_fnptr = NULL;

    // code
    // get the required function pointers
    vkCreateDebugReportCallbackEXT_fnptr = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(vkInstance, "vkCreateDebugReportCallbackEXT");
    if(vkCreateDebugReportCallbackEXT_fnptr == NULL) {
        vkResult = VK_ERROR_INITIALIZATION_FAILED; // return hardcoded failure
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createValidationCallbackFunction(): vkGetInstanceProcAddr() for vkCreateDebugReportCallbackEXT Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createValidationCallbackFunction(): vkGetInstanceProcAddr() for vkCreateDebugReportCallbackEXT Successful!.\n");
    }

    vkDestroyDebugReportCallbackEXT_fnptr = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(vkInstance, "vkDestroyDebugReportCallbackEXT");
    if(vkCreateDebugReportCallbackEXT_fnptr == NULL) {
        vkResult = VK_ERROR_INITIALIZATION_FAILED; // return hardcoded failure
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createValidationCallbackFunction(): vkGetInstanceProcAddr() for vkDestroyDebugReportCallbackEXT Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createValidationCallbackFunction(): vkGetInstanceProcAddr() for vkDestroyDebugReportCallbackEXT Successful!.\n");
    }

    // fill struct VkDebugReportCallbackCreateInfoEXT to get vulkan debug report callback object
    VkDebugReportCallbackCreateInfoEXT vkDebugReportCallbackCreateInfoEXT;
    memset((void*)&vkDebugReportCallbackCreateInfoEXT, 0, sizeof(VkDebugReportCallbackCreateInfoEXT));

    vkDebugReportCallbackCreateInfoEXT.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    vkDebugReportCallbackCreateInfoEXT.pNext = NULL;
    vkDebugReportCallbackCreateInfoEXT.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    vkDebugReportCallbackCreateInfoEXT.pfnCallback = debugReportCallback;
    vkDebugReportCallbackCreateInfoEXT.pUserData = NULL;

    vkResult = vkCreateDebugReportCallbackEXT_fnptr(vkInstance, &vkDebugReportCallbackCreateInfoEXT, NULL, &vkDebugReportCallbackEXT);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createValidationCallbackFunction(): vkCreateDebugReportCallbackEXT_fnptr() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createValidationCallbackFunction(): vkCreateDebugReportCallbackEXT_fnptr() Successful!.\n");
    }

    return (vkResult);
}

// get supported surface
VkResult getSupportedSurface(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    //code
    VkAndroidSurfaceCreateInfoKHR vkAndroidSurfaceCreateInfoKHR;
    memset((void*)&vkAndroidSurfaceCreateInfoKHR, 0, sizeof(VkAndroidSurfaceCreateInfoKHR));

    vkAndroidSurfaceCreateInfoKHR.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    vkAndroidSurfaceCreateInfoKHR.pNext = NULL;
    vkAndroidSurfaceCreateInfoKHR.flags = 0;
    vkAndroidSurfaceCreateInfoKHR.window = androidNativeWindow;

    vkResult = vkCreateAndroidSurfaceKHR(
        vkInstance, 
        &vkAndroidSurfaceCreateInfoKHR,
        NULL,
        &vkSurfaceKHR
    );

    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getSupportedSurface(): vkCreateAndroidSurfaceKHR() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getSupportedSurface(): vkCreateAndroidSurfaceKHR() Successful!.\n");
    }

    return vkResult;
}

VkResult getPhysicalDevice() {
    // variables
    VkResult vkResult = VK_SUCCESS;

    //code
    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, NULL);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): vkEnumeratePhysicalDevices() First Call Failed!.\n");
        return (vkResult);
    } else if(physicalDeviceCount == 0) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): vkEnumeratePhysicalDevices() Resulted in Zero Physical Devices!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): vkEnumeratePhysicalDevices() First Call Successful!.\n");
    }

    vkPhysicalDevice_array = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);

    vkResult = vkEnumeratePhysicalDevices(vkInstance, &physicalDeviceCount, vkPhysicalDevice_array);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): vkEnumeratePhysicalDevices() Second Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): vkEnumeratePhysicalDevices() Second Call Successful!.\n");
    }

    VkBool32 bFound = VK_FALSE;

    for(uint32_t i = 0; i < physicalDeviceCount; i++) {
        uint32_t queueCount = UINT32_MAX;

        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &queueCount, NULL);
        VkQueueFamilyProperties *vkQueueFamilyProperties_array = NULL;
        vkQueueFamilyProperties_array = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_array[i], &queueCount, vkQueueFamilyProperties_array);

        VkBool32 *isQueueSurfaceSupported_array = NULL;
        isQueueSurfaceSupported_array = (VkBool32*)malloc(sizeof(VkBool32) * queueCount);

        for(uint32_t j = 0; j < queueCount; j++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(
                vkPhysicalDevice_array[i],
                j,
                vkSurfaceKHR,
                &isQueueSurfaceSupported_array[j]
            );
        }

        for(uint32_t j = 0; j < queueCount; j++) {
            if(vkQueueFamilyProperties_array[j].queueFlags & VK_QUEUE_GRAPHICS_BIT
                && isQueueSurfaceSupported_array[j] == VK_TRUE) {
                vkPhysicalDevice_selected = vkPhysicalDevice_array[i];
                graphicsQueueFamilyIndex_selected = j;
                bFound = VK_TRUE;
                break;
            }
        }

        if(isQueueSurfaceSupported_array) {
            free(isQueueSurfaceSupported_array);
            isQueueSurfaceSupported_array = NULL;
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): freed isQueueSurfaceSupported_array!.\n");
        }
        
        if(vkQueueFamilyProperties_array) {
            free(vkQueueFamilyProperties_array);
            vkQueueFamilyProperties_array = NULL;
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): freed vkQueueFamilyProperties_array!.\n");
        }

        if(bFound == VK_TRUE) {
            break;
        }
    }


    if(bFound == VK_TRUE) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): Successful to get required graphics enabled physical device!.\n");
    } else {
        if(vkPhysicalDevice_array) {
            free(vkPhysicalDevice_array);
            vkPhysicalDevice_array = NULL;
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): freed vkPhysicalDevice_array!.\n");
        }
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): Failed to get required graphics enabled physical device!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    }

    memset((void*)&vkPhysicalDeviceMemoryProperties, 0, sizeof(VkPhysicalDeviceMemoryProperties));

    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice_selected, &vkPhysicalDeviceMemoryProperties);

    VkPhysicalDeviceFeatures vkPhysicalDeviceFeatures;
    memset((void*)&vkPhysicalDeviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));

    vkGetPhysicalDeviceFeatures(vkPhysicalDevice_selected, &vkPhysicalDeviceFeatures);

    if(vkPhysicalDeviceFeatures.tessellationShader == VK_TRUE) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): Selected Physical Device Supports Tessellation Shader!.\n");
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): Selected Physical Device Does Not Supports Tessellation Shader!.\n");
    }

    if(vkPhysicalDeviceFeatures.geometryShader == VK_TRUE) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): Selected Physical Device Supports Geometry Shader!.\n");
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDevice(): Selected Physical Device Does Not Supports Geometry Shader!.\n");
    }

    return (vkResult);
}

VkResult printVKInfo (void) {
    // varibales
    VkResult vkResult = VK_SUCCESS;

    // code
    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "printVKInfo(): Printing Vulkan Info: \n\n");

    for(uint32_t i = 0; i < physicalDeviceCount; i++) {

        VkPhysicalDeviceProperties vkPhysicalDeviceProperties;
        memset((void*)&vkPhysicalDeviceProperties, 0, sizeof(VkPhysicalDeviceProperties));

        vkGetPhysicalDeviceProperties(vkPhysicalDevice_array[i], &vkPhysicalDeviceProperties);

        uint32_t majorVersion = VK_API_VERSION_MAJOR(vkPhysicalDeviceProperties.apiVersion);
        uint32_t minorVersion = VK_API_VERSION_MINOR(vkPhysicalDeviceProperties.apiVersion);
        uint32_t patchVersion = VK_API_VERSION_PATCH(vkPhysicalDeviceProperties.apiVersion);

        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Physical Device [%d] Properties: \n", i);
        // API Version
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "API Version: %d.%d.%d\n", majorVersion, minorVersion, patchVersion);
        //Device Name
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device Name: %s\n", vkPhysicalDeviceProperties.deviceName);
        
        switch(vkPhysicalDeviceProperties.deviceType) {
    
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device Type: Integrated GPU (iGPU)\n");
                break;

            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device Type: Discrete GPU (dGPU)\n");
                break;

            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device Type: Virtual GPU (vGPU)\n");
                break;

            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device Type: GPU\n");
                break;

            case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device Type: Nor iGPU, dGPU, vGPU, or CPU, Something Other\n");
                break;

            default:
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device Type: Unknown\n");
                break;
        }

        // Vendor ID
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Vendor ID: 0x%04x\n", vkPhysicalDeviceProperties.vendorID);

        // Device ID
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Device ID: 0x%04x\n", vkPhysicalDeviceProperties.deviceID);

        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "\n");
    }

    if(vkPhysicalDevice_array) {
        free(vkPhysicalDevice_array);
        vkPhysicalDevice_array = NULL;
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "printVKInfo(): freed vkPhysicalDevice_array!.\n");
    }

    return (vkResult);
}

VkResult fillDeviceExtensionNames (void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // Step 1: Find how many devices extension are supported by this vulkan driver & keep it in local variable
    uint32_t devicesExtensionCount = 0;

    vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected, NULL, &devicesExtensionCount, NULL);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): vkEnumerateDeviceExtensionProperties() First Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): vkEnumerateDeviceExtensionProperties() First Call Successful!.\n");
    }

    // step 2: Allocate & fill struct vk Extenstions array correspoinding to above count
    VkExtensionProperties *vkExtensionProperties_array = NULL;
    vkExtensionProperties_array = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * devicesExtensionCount);
    vkResult = vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_selected, NULL, &devicesExtensionCount, vkExtensionProperties_array);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): vkEnumerateDeviceExtensionProperties() Second Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): vkEnumerateDeviceExtensionProperties() Second Call Successful!.\n");
    }

    // Step 3: 
    char **deviceExtensionNames_array = NULL;
    deviceExtensionNames_array = (char**)malloc(sizeof(char*) * devicesExtensionCount);
    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): Vulkan Device Extension Count = %d \n", devicesExtensionCount);
    for(uint32_t i = 0; i < devicesExtensionCount; i++) {
        deviceExtensionNames_array[i] = (char*)malloc(sizeof(char) * strlen(vkExtensionProperties_array[i].extensionName) + 1);
        memcpy(
            deviceExtensionNames_array[i], 
            vkExtensionProperties_array[i].extensionName, 
            strlen(vkExtensionProperties_array[i].extensionName) + 1
        );
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): Vulkan Device Extension Name = %s \n", deviceExtensionNames_array[i]);
    }

    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "\n");


    // step 4:
    free(vkExtensionProperties_array);

    // step 5
    VkBool32 vulkanSwapchainExtensionFound = VK_FALSE;
    for(uint32_t i = 0; i < devicesExtensionCount; i++) {
        if(strcmp(deviceExtensionNames_array[i], VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            vulkanSwapchainExtensionFound = VK_TRUE;
            enabledDeviceExtensionNames_array[enabledDeviceExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        }
    }

    // step 6
    for(uint32_t i = 0; i < devicesExtensionCount; i++) {
        free(deviceExtensionNames_array[i]);
    }
    free(deviceExtensionNames_array);

    // step 7:
    if(vulkanSwapchainExtensionFound == VK_FALSE) {
        vkResult = VK_ERROR_INITIALIZATION_FAILED; // return hardcoded failure
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): VK_KHR_SWAPCHAIN_EXTENSION_NAME Not Found!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): VK_KHR_SWAPCHAIN_EXTENSION_NAME Found!.\n");
    }

    // step 8:
    for(uint32_t i = 0; i < enabledDeviceExtensionCount; i++) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "fillDeviceExtensionNames(): Enabled Vulkan Device Extension Name = %s \n", enabledDeviceExtensionNames_array[i]);
    }

    return vkResult;
}

VkResult createVulkanDevice () {
    
    // function definations
    VkResult fillDeviceExtensionNames(void);

    //variables
    VkResult vkResult = VK_SUCCESS;

    vkResult = fillDeviceExtensionNames();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanDevice(): fillDeviceExtensionNames() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanDevice(): fillDeviceExtensionNames() Successful!.\n");
    }

    // !NEWLY ADDED CODE : intialize VkDeviceQueueCreateInfo
    float queuePriorities[] = { 1.0f };
    VkDeviceQueueCreateInfo vkDeviceQueueCreateInfo;
    memset((void*)&vkDeviceQueueCreateInfo, 0, sizeof(VkDeviceQueueCreateInfo));

    vkDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    vkDeviceQueueCreateInfo.pNext = 0;
    vkDeviceQueueCreateInfo.flags = 0;
    vkDeviceQueueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex_selected;
    vkDeviceQueueCreateInfo.queueCount = 1;
    vkDeviceQueueCreateInfo.pQueuePriorities = queuePriorities;

    // initialize VkDeviceCreateInfo structure
    VkDeviceCreateInfo vkDeviceCreateInfo;
    memset((void*)&vkDeviceCreateInfo, 0, sizeof(VkDeviceCreateInfo));

    vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkDeviceCreateInfo.pNext = NULL;
    vkDeviceCreateInfo.flags = 0;
    vkDeviceCreateInfo.enabledExtensionCount = enabledDeviceExtensionCount;
    vkDeviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensionNames_array;
    vkDeviceCreateInfo.enabledLayerCount = 0; // these are deprecated in current version
    vkDeviceCreateInfo.ppEnabledLayerNames = NULL; // these are deprecated in current version
    vkDeviceCreateInfo.pEnabledFeatures = NULL;
    // !NEWLY ADDED CODE : set VkDeviceQueueCreateInfo
    vkDeviceCreateInfo.queueCreateInfoCount = 1;
    vkDeviceCreateInfo.pQueueCreateInfos = &vkDeviceQueueCreateInfo;

    vkResult = vkCreateDevice(
        vkPhysicalDevice_selected,
        &vkDeviceCreateInfo,
        NULL, &vkDevice
    );

    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanDevice(): vkCreateDevice() Failed!. (%d)\n", vkResult);
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVulkanDevice(): vkCreateDevice() Successful!.\n");
    }

    return(vkResult);
}

void getDeviceQueue (void) {

    vkGetDeviceQueue(
        vkDevice,
        graphicsQueueFamilyIndex_selected,
        0, &vkQueue
    );

    if(vkQueue == VK_NULL_HANDLE) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getDeviceQueue(): vkGetDeviceQueue() Failed!.\n");
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getDeviceQueue(): vkGetDeviceQueue() Successful!.\n\n");
    }

}

VkResult getPhysicalDeviceSurfaceFormatAndColorSpace (void) {
    
    //variables
    VkResult vkResult = VK_SUCCESS;
    uint32_t formatCount = 0;

    // code

    // get the count of supported color formats
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        vkPhysicalDevice_selected, 
        vkSurfaceKHR, &formatCount,
        NULL
    );

    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfaceFormatAndColorSpace(): vkGetPhysicalDeviceSurfaceFormatsKHR() frist Call Failed!.\n");
    } else if(formatCount == 0) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfaceFormatAndColorSpace(): vkGetPhysicalDeviceSurfaceFormatsKHR() Failed: 0 supported formats found!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return(vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfaceFormatAndColorSpace(): vkGetPhysicalDeviceSurfaceFormatsKHR() first Call Successful!. [Found %d Formats]\n", formatCount);
    }

    VkSurfaceFormatKHR *vkSurfaceFormatKHR_array = (VkSurfaceFormatKHR*)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    
    // fill the allocated array with supported formats
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        vkPhysicalDevice_selected, 
        vkSurfaceKHR, &formatCount,
        vkSurfaceFormatKHR_array
    );
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfaceFormatAndColorSpace(): vkGetPhysicalDeviceSurfaceFormatsKHR() Second Call Failed!.\n");
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfaceFormatAndColorSpace(): vkGetPhysicalDeviceSurfaceFormatsKHR() Second Call Successful!.\n");
    }

    // Decide the surface color format first!
    if(formatCount == 1 && vkSurfaceFormatKHR_array[0].format == VK_FORMAT_UNDEFINED) {
        vkFormat_color = VK_FORMAT_B8G8R8G8_422_UNORM;
    } else {
        vkFormat_color = vkSurfaceFormatKHR_array[0].format;
    }

    // Decide the Color Space
    vkColorSpaceKHR = vkSurfaceFormatKHR_array[0].colorSpace;

    if(vkSurfaceFormatKHR_array) {
        free(vkSurfaceFormatKHR_array);
        vkSurfaceFormatKHR_array = NULL;
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfaceFormatAndColorSpace(): vkSurfaceFormatKHR_array freed.\n");
    }

    return (vkResult);
}

VkResult getPhysicalDeviceSurfacePresentMode(void) {

    // Variables
    VkResult vkResult = VK_SUCCESS;
    uint32_t modeCount = 0;

    //code
    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice_selected, 
        vkSurfaceKHR, &modeCount,
        NULL
    );
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): vkGetPhysicalDeviceSurfacePresentModesKHR() frist Call Failed!.\n");
    } else if(modeCount == 0) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): vkGetPhysicalDeviceSurfacePresentModesKHR() Failed: 0 supported modes found!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return(vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): vkGetPhysicalDeviceSurfacePresentModesKHR() first Call Successful!. [Found %d Present Modes]\n", modeCount);
    }

    VkPresentModeKHR *vkPresentModeKHR_array = (VkPresentModeKHR*)malloc(modeCount * sizeof(VkPresentModeKHR));

    // fill the allocated array with supported present modes
    vkResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        vkPhysicalDevice_selected, 
        vkSurfaceKHR, &modeCount,
        vkPresentModeKHR_array
    );
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): vkGetPhysicalDeviceSurfacePresentModesKHR() Second Call Failed!.\n");
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): vkGetPhysicalDeviceSurfacePresentModesKHR() Second Call Successful!.\n");
    }

    for(uint32_t i = 0 ;  i < modeCount; i++) {
        if(vkPresentModeKHR_array[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            vkPresentModeKHR = vkPresentModeKHR_array[i];
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): VK_PRESENT_MODE_MAILBOX_KHR Present Mode found!.\n");
            break;
        }
    }

    if(vkPresentModeKHR != VK_PRESENT_MODE_MAILBOX_KHR) {
        // since we don't have mailbox as supported format let's settle for FIFO then!
        vkPresentModeKHR = VK_PRESENT_MODE_FIFO_KHR;
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): Present Mode set to VK_PRESENT_MODE_FIFO_KHR!.\n");
    }

    if(vkPresentModeKHR_array) {
        free(vkPresentModeKHR_array);
        vkPresentModeKHR_array = NULL;
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "getPhysicalDeviceSurfacePresentMode(): vkPresentModeKHR_array freed.\n");
    }

    return (vkResult);
}

VkResult createSwapchain (VkBool32 vSync) {

    // Functions
    VkResult getPhysicalDeviceSurfaceFormatAndColorSpace(void);
    VkResult getPhysicalDeviceSurfacePresentMode(void);

    // Variables
    VkResult vkResult = VK_SUCCESS;
    
    // Code
    // Surface Color & Color Space
    vkResult = getPhysicalDeviceSurfaceFormatAndColorSpace();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): getPhysicalDeviceSurfaceFormatAndColorSpace() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): getPhysicalDeviceSurfaceFormatAndColorSpace() Successful!.\n");
    }

    // Get Physical Device Surface Capabilities
    VkSurfaceCapabilitiesKHR vkSurfaceCapabilitiesKHR;
    memset((void*)&vkSurfaceCapabilitiesKHR, 0, sizeof(VkSurfaceCapabilitiesKHR));

    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_selected, vkSurfaceKHR, &vkSurfaceCapabilitiesKHR);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): vkGetPhysicalDeviceSurfaceCapabilitiesKHR() Successful!.\n");
    }

    // Decide Image Count of Swapchain using minImageCount & maxImageCount from vkSurfaceCapabilitiesKHR
    uint32_t testingNumberOfSwapchainImages = vkSurfaceCapabilitiesKHR.minImageCount + 1;
    uint32_t desiredNumberOfSwapchainImages = 0;

    if(vkSurfaceCapabilitiesKHR.maxImageCount > 0 && vkSurfaceCapabilitiesKHR.maxImageCount < testingNumberOfSwapchainImages) {
        desiredNumberOfSwapchainImages = vkSurfaceCapabilitiesKHR.maxImageCount;
    } else {
        desiredNumberOfSwapchainImages = vkSurfaceCapabilitiesKHR.minImageCount;
    }

    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:",
        "createSwapchain(): desiredNumberOfSwapchainImages is : %d, [Min: %d, Max: %d]\n", 
        desiredNumberOfSwapchainImages,  vkSurfaceCapabilitiesKHR.minImageCount,  
        vkSurfaceCapabilitiesKHR.maxImageCount
    );

    // Decide Size of Swapchain Image using currentExtent Size & window Size
    memset((void*)&vkExtent2D_swapchain, 0, sizeof(VkExtent2D));

    if(vkSurfaceCapabilitiesKHR.currentExtent.width != UINT32_MAX) {
        vkExtent2D_swapchain.width = vkSurfaceCapabilitiesKHR.currentExtent.width;
        vkExtent2D_swapchain.height = vkSurfaceCapabilitiesKHR.currentExtent.height;

        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", 
            "createSwapchain(): Swapchain Image Width : %d X Height : %d\n", 
            vkExtent2D_swapchain.width, vkExtent2D_swapchain.height
        );
    } else {
        // if surface size is already defined then swapchain image size must match with it!
        VkExtent2D vkExtent2D;
        memset((void*)&vkExtent2D, 0, sizeof(VkExtent2D));

        vkExtent2D.width = (uint32_t)winWidth;
        vkExtent2D.height = (uint32_t)winHeight;

        vkExtent2D_swapchain.width = glm::max(vkSurfaceCapabilitiesKHR.minImageExtent.width, glm::min(vkSurfaceCapabilitiesKHR.maxImageExtent.width, vkExtent2D.width));
        vkExtent2D_swapchain.height = glm::max(vkSurfaceCapabilitiesKHR.minImageExtent.height, glm::min(vkSurfaceCapabilitiesKHR.maxImageExtent.height, vkExtent2D.height));

        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:",
            "createSwapchain(): Swapchain Image (Derived from best of minImageExtent, maxImageExtent & Window Size) Width  : %d X Height : %d\n", 
            vkExtent2D_swapchain.width, vkExtent2D_swapchain.height
        );
    }

    // Set Swapchain Image Usage Flag
    VkImageUsageFlags vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    // Whether to Consider Pre-Transform/Flipping or Not
    VkSurfaceTransformFlagBitsKHR vkSurfaceTransformFlagBitsKHR;

    if(vkSurfaceCapabilitiesKHR.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        vkSurfaceTransformFlagBitsKHR = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        vkSurfaceTransformFlagBitsKHR = vkSurfaceCapabilitiesKHR.currentTransform;
    }

    // Physical Device Presentation Mode
    vkResult = getPhysicalDeviceSurfacePresentMode();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): getPhysicalDeviceSurfacePresentMode() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): getPhysicalDeviceSurfacePresentMode() Successful!.\n");
    }

    // Initalize VkSwapchainCreateInfoKHR
    VkSwapchainCreateInfoKHR vkSwapchainCreateInfoKHR;
    memset((void*)&vkSwapchainCreateInfoKHR, 0, sizeof(VkSwapchainCreateInfoKHR));

    vkSwapchainCreateInfoKHR.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vkSwapchainCreateInfoKHR.pNext = NULL;
    vkSwapchainCreateInfoKHR.flags = 0;
    vkSwapchainCreateInfoKHR.surface = vkSurfaceKHR;
    vkSwapchainCreateInfoKHR.minImageCount = desiredNumberOfSwapchainImages;
    vkSwapchainCreateInfoKHR.imageFormat = vkFormat_color;
    vkSwapchainCreateInfoKHR.imageColorSpace = vkColorSpaceKHR;
    vkSwapchainCreateInfoKHR.imageExtent.width = vkExtent2D_swapchain.width;
    vkSwapchainCreateInfoKHR.imageExtent.height = vkExtent2D_swapchain.height;
    vkSwapchainCreateInfoKHR.imageUsage = vkImageUsageFlags;
    vkSwapchainCreateInfoKHR.preTransform = vkSurfaceTransformFlagBitsKHR;
    vkSwapchainCreateInfoKHR.imageArrayLayers = 1;
    vkSwapchainCreateInfoKHR.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkSwapchainCreateInfoKHR.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    vkSwapchainCreateInfoKHR.presentMode = vkPresentModeKHR;
    vkSwapchainCreateInfoKHR.clipped = VK_TRUE;

    vkResult = vkCreateSwapchainKHR(vkDevice, &vkSwapchainCreateInfoKHR, NULL, &vkSwapchainKHR);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): vkCreateSwapchainKHR() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchain(): vkCreateSwapchainKHR() Successful!.\n");
    }

    return (vkResult);
}

VkResult createSwapchainImagesAndImageViews(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // code
    // Step 1: Get Swapchain Image Count
    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, NULL);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchainImagesAndImageViews(): vkGetSwapchainImagesKHR() First Call Failed!.\n");
        return (vkResult);
    } else if( swapchainImageCount == 0) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchainImagesAndImageViews(): vkGetSwapchainImagesKHR() Failed: 0 Swapchain Images found!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchainImagesAndImageViews(): vkGetSwapchainImagesKHR() Successful!. : Swapchain Image Count : [%d]\n", swapchainImageCount);
    }

    // Step 2: Allocate Swapchain Image Array
    swapchainImage_array = (VkImage*)malloc(sizeof(VkImage) * swapchainImageCount);

    // Step 3: Fill Swapchain Image Array
    vkResult = vkGetSwapchainImagesKHR(vkDevice, vkSwapchainKHR, &swapchainImageCount, swapchainImage_array);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchainImagesAndImageViews(): vkGetSwapchainImagesKHR() Second Call Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchainImagesAndImageViews(): vkGetSwapchainImagesKHR() Second Call Successful!.\n");
    }

    // Setp 4: Allocate Swapchain Image Views Array
    swapchainImageView_array = (VkImageView*)malloc(sizeof(VkImageView) * swapchainImageCount);

    // Step 5: vkCreateImageView for each Swapchain Image
    VkImageViewCreateInfo vkImageViewCreateInfo;
    memset((void*)&vkImageViewCreateInfo, 0, sizeof(VkImageViewCreateInfo));
    
    vkImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vkImageViewCreateInfo.pNext = NULL;
    vkImageViewCreateInfo.flags = 0;
    vkImageViewCreateInfo.format = vkFormat_color;
    vkImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    vkImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    vkImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    vkImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    vkImageViewCreateInfo.subresourceRange.layerCount = 1;
    vkImageViewCreateInfo.subresourceRange.levelCount = 1;
    vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    // Step 6: Fill Imafe view Array  using above struct
    for(uint32_t i = 0; i < swapchainImageCount; i++) {
        vkImageViewCreateInfo.image = swapchainImage_array[i];
        vkResult = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, NULL, &swapchainImageView_array[i]);
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchainImagesAndImageViews(): vkCreateImageView() Failed at {%d}!.\n", i);
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSwapchainImagesAndImageViews(): vkCreateImageView() Successful for {%d}!.\n", i);
        }
    }


    return (vkResult);
}

VkResult createCommandPool(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    VkCommandPoolCreateInfo vkCommandPoolCreateInfo;
    memset((void*)&vkCommandPoolCreateInfo, 0, sizeof(vkCommandPoolCreateInfo));

    vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    vkCommandPoolCreateInfo.pNext = NULL;
    vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCommandPoolCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex_selected;

    vkResult = vkCreateCommandPool(vkDevice, &vkCommandPoolCreateInfo, NULL, &vkCommandPool);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createCommandPool(): vkCreateCommandPool() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createCommandPool(): vkCreateCommandPool() Successful!.\n");
    }

    return (vkResult);
}

VkResult createCommandBuffers(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // Step 1: Init and Allocate VkCommandBufferAllocateInfo
    VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo;
    memset((void*)&vkCommandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));

    vkCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    vkCommandBufferAllocateInfo.pNext = NULL;
    vkCommandBufferAllocateInfo.commandPool = vkCommandPool;
    vkCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkCommandBufferAllocateInfo.commandBufferCount = 1;

    // Step 2: Allocate Command Buffer Array to the size of swapchainImageCount
    vkCommandBuffer_array = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * swapchainImageCount);

    // Step 3: Allocat eeach command buffer in loop with allocateInfo struct
    for(uint32_t i = 0; i < swapchainImageCount; i++) {
        vkResult = vkAllocateCommandBuffers(vkDevice, &vkCommandBufferAllocateInfo, &vkCommandBuffer_array[i]);
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createCommandBuffers(): vkCreatvkAllocateCommandBufferseImageView() Failed at {%d}!.\n", i);
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createCommandBuffers(): vkAllocateCommandBuffers() Successful for {%d}!.\n", i);
        }
    }

    return (vkResult);
}


VkResult createVertexBuffer(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // Step 1
    float triangle_position[] = {
        0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
    };

    float triangle_color[] = {
        1.0f, 0.0f, 0.0f, // Red
        0.0f, 1.0f, 0.0f, // Green
        0.0f, 0.0f, 1.0f  // Blue
    };

    // Step 2
    memset((void*)&vertexData_position, 0, sizeof(VertexData));

    // Step 3
    VkBufferCreateInfo vkBufferCreateInfo;
    memset((void*)&vkBufferCreateInfo, 0, sizeof(VkBufferCreateInfo));

    vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkBufferCreateInfo.pNext = NULL;
    vkBufferCreateInfo.flags = 0; // No flags, Valid Flags are used in scattered buffer
    vkBufferCreateInfo.size = sizeof(triangle_position);
    vkBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    
    // Setp 4
    vkResult = vkCreateBuffer(vkDevice, &vkBufferCreateInfo, NULL, &vertexData_position.vkBuffer);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkCreateBuffer() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkCreateBuffer() Successful!.\n");
    }

    // Step 5
    VkMemoryRequirements vkMemoryRequirements;
    memset((void*)&vkMemoryRequirements, 0, sizeof(VkMemoryRequirements));

    vkGetBufferMemoryRequirements(vkDevice, vertexData_position.vkBuffer, &vkMemoryRequirements);

    // Step 6
    VkMemoryAllocateInfo vkMemoryAllocateInfo;
    memset((void*)&vkMemoryAllocateInfo, 0, sizeof(VkMemoryAllocateInfo));

    vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo.pNext = NULL;
    vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
    vkMemoryAllocateInfo.memoryTypeIndex = 0; // this will be set in next step

    // Step A 
    for(uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
        // Step B
        if((vkMemoryRequirements.memoryTypeBits & 1) == 1) {
            // Step C
            if(vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                // Step D
                vkMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        // Step E
        vkMemoryRequirements.memoryTypeBits >>= 1;
    }

    //Setp 9
    vkResult = vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, NULL, &vertexData_position.vkDeviceMemory);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkAllocateMemory() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkAllocateMemory() Successful!.\n");
    }

    // Step 10
    vkResult = vkBindBufferMemory(vkDevice, vertexData_position.vkBuffer, vertexData_position.vkDeviceMemory, 0);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkBindBufferMemory() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkBindBufferMemory() Successful!.\n");
    }

    // Step 11
    void *data = NULL;

    vkResult = vkMapMemory(
        vkDevice,
        vertexData_position.vkDeviceMemory,
        0,
        vkMemoryAllocateInfo.allocationSize,
        0,
        &data
    );

    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkMapMemory() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkMapMemory() Successful!.\n");
    }

    // Step 12
    memcpy(data, triangle_position, sizeof(triangle_position));

    // Step 13
    vkUnmapMemory(vkDevice, vertexData_position.vkDeviceMemory);

    // VertexData for Triangle Color
    memset((void*)&vertexData_color, 0, sizeof(VertexData));

    // Step 3
    memset((void*)&vkBufferCreateInfo, 0, sizeof(VkBufferCreateInfo));

    vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkBufferCreateInfo.pNext = NULL;
    vkBufferCreateInfo.flags = 0; // No flags, Valid Flags are used in scattered buffer
    vkBufferCreateInfo.size = sizeof(triangle_color);
    vkBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    
    // Setp 4
    vkResult = vkCreateBuffer(vkDevice, &vkBufferCreateInfo, NULL, &vertexData_color.vkBuffer);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkCreateBuffer() Failed for Color!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkCreateBuffer() Successful for Color!.\n");
    }

    // Step 5
    memset((void*)&vkMemoryRequirements, 0, sizeof(VkMemoryRequirements));

    vkGetBufferMemoryRequirements(vkDevice, vertexData_color.vkBuffer, &vkMemoryRequirements);

    // Step 6
    memset((void*)&vkMemoryAllocateInfo, 0, sizeof(VkMemoryAllocateInfo));

    vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo.pNext = NULL;
    vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
    vkMemoryAllocateInfo.memoryTypeIndex = 0; // this will be set in next step

    // Step A 
    for(uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
        // Step B
        if((vkMemoryRequirements.memoryTypeBits & 1) == 1) {
            // Step C
            if(vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                // Step D
                vkMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        // Step E
        vkMemoryRequirements.memoryTypeBits >>= 1;
    }

    //Setp 9
    vkResult = vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, NULL, &vertexData_color.vkDeviceMemory);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkAllocateMemory() Failed for Color!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkAllocateMemory() Successful for Color!.\n");
    }

    // Step 10
    vkResult = vkBindBufferMemory(vkDevice, vertexData_color.vkBuffer, vertexData_color.vkDeviceMemory, 0);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkBindBufferMemory() Failed for Color!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkBindBufferMemory() Successful for Color!.\n");
    }

    // Step 11
    data = NULL;

    vkResult = vkMapMemory(
        vkDevice,
        vertexData_color.vkDeviceMemory,
        0,
        vkMemoryAllocateInfo.allocationSize,
        0,
        &data
    );

    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkMapMemory() Failed for Color!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createVertexBuffer(): vkMapMemory() Successful for Color!.\n");
    }

    // Step 12
    memcpy(data, triangle_color, sizeof(triangle_color));

    // Step 13
    vkUnmapMemory(vkDevice, vertexData_color.vkDeviceMemory);

    return(vkResult);
}


VkResult createUniformBuffer (void) {
    // functions
    VkResult updateUniformBuffer(void);

    // variables
    VkResult vkResult = VK_SUCCESS;

    memset((void*)&uniformData, 0, sizeof(UniformData));

    // Step 3
    VkBufferCreateInfo vkBufferCreateInfo;
    memset((void*)&vkBufferCreateInfo, 0, sizeof(VkBufferCreateInfo));

    vkBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vkBufferCreateInfo.pNext = NULL;
    vkBufferCreateInfo.flags = 0; // No flags, Valid Flags are used in scattered buffer
    vkBufferCreateInfo.size = sizeof(struct MyUniformData);
    vkBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    
    // Setp 4
    vkResult = vkCreateBuffer(vkDevice, &vkBufferCreateInfo, NULL, &uniformData.vkBuffer);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): vkCreateBuffer() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): vkCreateBuffer() Successful!.\n");
    }

    // Step 5
    VkMemoryRequirements vkMemoryRequirements;
    memset((void*)&vkMemoryRequirements, 0, sizeof(VkMemoryRequirements));

    vkGetBufferMemoryRequirements(vkDevice, uniformData.vkBuffer, &vkMemoryRequirements);

    // Step 6
    VkMemoryAllocateInfo vkMemoryAllocateInfo;
    memset((void*)&vkMemoryAllocateInfo, 0, sizeof(VkMemoryAllocateInfo));

    vkMemoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vkMemoryAllocateInfo.pNext = NULL;
    vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
    vkMemoryAllocateInfo.memoryTypeIndex = 0; // this will be set in next step

    // Step A 
    for(uint32_t i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
        // Step B
        if((vkMemoryRequirements.memoryTypeBits & 1) == 1) {
            // Step C
            if(vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                // Step D
                vkMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
        // Step E
        vkMemoryRequirements.memoryTypeBits >>= 1;
    }

    //Setp 9
    vkResult = vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, NULL, &uniformData.vkDeviceMemory);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): vkAllocateMemory() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): vkAllocateMemory() Successful!.\n");
    }

    // Step 10
    vkResult = vkBindBufferMemory(vkDevice, uniformData.vkBuffer, uniformData.vkDeviceMemory, 0);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): vkBindBufferMemory() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): vkBindBufferMemory() Successful!.\n");
    }

    // call updateUniformBuffer() to fill the uniform buffer with data
    vkResult = updateUniformBuffer();
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): updateUniformBuffer() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createUniformBuffer(): updateUniformBuffer() Successful!.\n");
    }

    return (vkResult);
}

VkResult updateUniformBuffer(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    struct MyUniformData myUniformData;
    memset((void*)&myUniformData, 0, sizeof(struct MyUniformData));

    myUniformData.modelMatrix = glm::mat4(1.0f);
    myUniformData.modelMatrix = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(0.0f, 0.0f, -3.0f)
    );

    myUniformData.viewMatrix = glm::mat4(1.0f);
    myUniformData.projectionMatrix = glm::mat4(1.0f);

    glm::mat4 perspectiveProjectionMatrix = glm::mat4(1.0f);

    perspectiveProjectionMatrix = glm::perspective(
        glm::radians(45.0f),
        (float)winWidth / (float)winHeight,
        0.1f,
        100.0f
    );

    perspectiveProjectionMatrix[1][1] *= -1.0f; // Invert Y axis for Vulkan

    myUniformData.projectionMatrix = perspectiveProjectionMatrix;

    void *data = NULL;

    vkResult = vkMapMemory(
        vkDevice,
        uniformData.vkDeviceMemory,
        0,
        sizeof(struct MyUniformData),
        0,
        &data
    );
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "updateUniformBuffer(): vkMapMemory() Failed!.\n");
        return (vkResult);
    }

    memcpy(data, &myUniformData, sizeof(struct MyUniformData));

    vkUnmapMemory(vkDevice, uniformData.vkDeviceMemory);

    // Free Data / Set it to NULL
    data = NULL;

    return (vkResult);
}

VkResult createShaders(void) {

    // variables
    VkResult vkResult = VK_SUCCESS;

    // for vertex shader
    const char* szFileName = "shader.vert.spv";
    size_t fileSize = 0;
    AAsset *asset = NULL;

    asset = AAssetManager_open(androidAssetManager, szFileName, AASSET_MODE_BUFFER);
    if(asset == NULL) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAssetManager_open() failed to open Vertex Shader spir-v file!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAssetManager_open() succeed to open Vertex Shader spir-v file!.\n");
    }

    fileSize = AAsset_getLength(asset);
    if(fileSize == 0) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAsset_getLength() gave file size 0.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    } 

    char *shaderData = (char*)malloc(fileSize * sizeof(char));

    int retVal = AAsset_read(asset, shaderData, fileSize);
    if(retVal != fileSize) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAsset_read() failed to read Vertex Shader file!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAsset_read() succeed to read Vertex Shader file!.\n");
    }
    
    AAsset_close(asset);
    asset = NULL;

    VkShaderModuleCreateInfo vkShaderModuleCreateInfo;
    memset((void*)&vkShaderModuleCreateInfo, 0, sizeof(VkShaderModuleCreateInfo));

    vkShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vkShaderModuleCreateInfo.pNext = NULL;
    vkShaderModuleCreateInfo.flags = 0;
    vkShaderModuleCreateInfo.codeSize = fileSize;
    vkShaderModuleCreateInfo.pCode = (uint32_t*)shaderData;

    vkResult = vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, NULL, &vkShaderModule_vertex);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): vkCreateShaderModule() for Vertex Shader Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): vkCreateShaderModule() for Vertex Shader Successful!.\n");
    }

    if(shaderData) {
        free(shaderData);
        shaderData = NULL;
    }
    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): Vertex Shader Module Created Successful!.\n");

    // for fragment shader
    szFileName = "shader.frag.spv";
    fileSize = 0;

    asset = AAssetManager_open(androidAssetManager, szFileName, AASSET_MODE_BUFFER);
    if(asset == NULL) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAssetManager_open() failed to open Fragment Shader spir-v file!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAssetManager_open() succeed to open Fragment Shader spir-v file!.\n");
    }

    fileSize = AAsset_getLength(asset);
    if(fileSize == 0) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAsset_getLength() gave file size 0.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    }

    shaderData = (char*)malloc(fileSize * sizeof(char));

    retVal = AAsset_read(asset, shaderData, fileSize);
    if(retVal != fileSize) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAsset_read() failed to read Fragment Shader file!.\n");
        vkResult = VK_ERROR_INITIALIZATION_FAILED;
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): AAsset_read() succeed to read Fragment Shader file!.\n");
    }
    AAsset_close(asset);
    asset = NULL;

    memset((void*)&vkShaderModuleCreateInfo, 0, sizeof(VkShaderModuleCreateInfo));

    vkShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vkShaderModuleCreateInfo.pNext = NULL;
    vkShaderModuleCreateInfo.flags = 0;
    vkShaderModuleCreateInfo.codeSize = fileSize;
    vkShaderModuleCreateInfo.pCode = (uint32_t*)shaderData;

    vkResult = vkCreateShaderModule(vkDevice, &vkShaderModuleCreateInfo, NULL, &vkShaderModule_fragment);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): vkCreateShaderModule() for Fragment Shader Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): vkCreateShaderModule() for Fragment Shader Successful!.\n");
    }

    if(shaderData) {
        free(shaderData);
        shaderData = NULL;
    }
    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createShaders(): Fragment Shader Module Created Successful!.\n");

    return (vkResult);
}


VkResult createDescriptorSetLayout(void) {
    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Initialize Descriptor Set Binding
    VkDescriptorSetLayoutBinding vkDescriptorSetLayoutBinding;
    memset((void*)&vkDescriptorSetLayoutBinding, 0, sizeof(VkDescriptorSetLayoutBinding));

    vkDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vkDescriptorSetLayoutBinding.binding = 0; // this 0 is  the binding index, we will use this index in shader
    vkDescriptorSetLayoutBinding.descriptorCount = 1; 
    vkDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // this binding will be used in vertex shader
    vkDescriptorSetLayoutBinding.pImmutableSamplers = NULL; // we don't have any immutable samplers for now

    //Create Descriptor Set Layout Create Info
    VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo;
    memset((void*)&vkDescriptorSetLayoutCreateInfo, 0, sizeof(VkDescriptorSetLayoutCreateInfo));

    vkDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    vkDescriptorSetLayoutCreateInfo.pNext = NULL;
    vkDescriptorSetLayoutCreateInfo.flags = 0;
    vkDescriptorSetLayoutCreateInfo.bindingCount = 1; // we will atleast have one binding
    vkDescriptorSetLayoutCreateInfo.pBindings = &vkDescriptorSetLayoutBinding; // we will atleast have one binding
    
    // Create Descriptor Set Layout
    vkResult = vkCreateDescriptorSetLayout(vkDevice, &vkDescriptorSetLayoutCreateInfo, NULL, &vkDescriptorSetLayout);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createDescriptorSetLayout(): vkCreateDescriptorSetLayout() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createDescriptorSetLayout(): vkCreateDescriptorSetLayout() Successful!.\n");
    }

    return (vkResult);
}

VkResult createPipelineLayout(void) {
    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Create Pipeline Layout Create Info
    VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo;
    memset((void*)&vkPipelineLayoutCreateInfo, 0, sizeof(VkPipelineLayoutCreateInfo));

    vkPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkPipelineLayoutCreateInfo.pNext = NULL;
    vkPipelineLayoutCreateInfo.flags = 0;
    vkPipelineLayoutCreateInfo.setLayoutCount = 1; // we have only one descriptor set layout
    vkPipelineLayoutCreateInfo.pSetLayouts = &vkDescriptorSetLayout;
    vkPipelineLayoutCreateInfo.pushConstantRangeCount = 0; // no push constant range for now
    vkPipelineLayoutCreateInfo.pPushConstantRanges = NULL;

    // Create Pipeline Layout
    vkResult = vkCreatePipelineLayout(vkDevice, &vkPipelineLayoutCreateInfo, NULL, &vkPipelineLayout);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createPipelineLayout(): vkCreatePipelineLayout() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createPipelineLayout(): vkCreatePipelineLayout() Successful!.\n");
    }

    return (vkResult);
}

VkResult createDescriptorPool(void) {
    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Create Descriptor Pool Create Info
    VkDescriptorPoolSize vkDescriptorPoolSize;
    memset((void*)&vkDescriptorPoolSize, 0, sizeof(VkDescriptorPoolSize));

    vkDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vkDescriptorPoolSize.descriptorCount = 1; // we have only one uniform buffer

    VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo;
    memset((void*)&vkDescriptorPoolCreateInfo, 0, sizeof(VkDescriptorPoolCreateInfo));

    vkDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    vkDescriptorPoolCreateInfo.pNext = NULL;
    vkDescriptorPoolCreateInfo.flags = 0;
    vkDescriptorPoolCreateInfo.maxSets = 1; // we have only one descriptor set
    vkDescriptorPoolCreateInfo.poolSizeCount = 1; // we have only one pool size
    vkDescriptorPoolCreateInfo.pPoolSizes = &vkDescriptorPoolSize;

    // Create Descriptor Pool
    vkResult = vkCreateDescriptorPool(vkDevice, &vkDescriptorPoolCreateInfo, NULL, &vkDescriptorPool);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createDescriptorPool(): vkCreateDescriptorPool() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createDescriptorPool(): vkCreateDescriptorPool() Successful!.\n");
    }

    return (vkResult);
}

VkResult createDescriptorSet(void) {
    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Create Descriptor Set Allocate Info
    VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo;
    memset((void*)&vkDescriptorSetAllocateInfo, 0, sizeof(VkDescriptorSetAllocateInfo));

    vkDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    vkDescriptorSetAllocateInfo.pNext = NULL;
    vkDescriptorSetAllocateInfo.descriptorPool = vkDescriptorPool;
    vkDescriptorSetAllocateInfo.descriptorSetCount = 1; // we have only one descriptor set
    vkDescriptorSetAllocateInfo.pSetLayouts = &vkDescriptorSetLayout; // we have only one descriptor set layout

    // Allocate Descriptor Set
    vkResult = vkAllocateDescriptorSets(vkDevice, &vkDescriptorSetAllocateInfo, &vkDescriptorSet);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createDescriptorSet(): vkAllocateDescriptorSets() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createDescriptorSet(): vkAllocateDescriptorSets() Successful!.\n");
    }

    // Describe whether we want buffer or image as uniform
    // we want buffer as uniform
    VkDescriptorBufferInfo vkDescriptorBufferInfo;
    memset((void*)&vkDescriptorBufferInfo, 0, sizeof(VkDescriptorBufferInfo));

    vkDescriptorBufferInfo.buffer = uniformData.vkBuffer; // this is the buffer we want to use as uniform
    vkDescriptorBufferInfo.offset = 0; // offset is 0
    vkDescriptorBufferInfo.range = sizeof(struct MyUniformData); // range is size of uniform

    // Now update the descriptor set with the buffer directly to the shader
    // we will write to the shader
    VkWriteDescriptorSet vkWriteDescriptorSet;
    memset((void*)&vkWriteDescriptorSet, 0, sizeof(VkWriteDescriptorSet));

    vkWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vkWriteDescriptorSet.pNext = NULL;
    vkWriteDescriptorSet.dstSet = vkDescriptorSet; // this is the descriptor set we want to update
    vkWriteDescriptorSet.dstArrayElement = 0; // we have only one descriptor set, so array element is 0
    vkWriteDescriptorSet.descriptorCount = 1; // we are only gonna write one descriptor set
    vkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; 
    vkWriteDescriptorSet.pBufferInfo = &vkDescriptorBufferInfo;
    vkWriteDescriptorSet.pImageInfo = NULL; // we'll use this during texture
    vkWriteDescriptorSet.pTexelBufferView = NULL; // using for tiling of texture but we're not using it now
    vkWriteDescriptorSet.dstBinding = 0; // this is the binding index we used in descriptor set layout & shader

    // Update Descriptor Set
    vkUpdateDescriptorSets(vkDevice, 1, &vkWriteDescriptorSet, 0, NULL); 
    // we have only one descriptor set to update, so count is 1
    // last two parameters are for copy descriptor sets, which are used while copying

    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createDescriptorSet(): vkUpdateDescriptorSets() Successful!.\n");

    return (vkResult);
}

VkResult createRenderPass(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // Code
    //Step 1: Create Attachment Description stcture array
    VkAttachmentDescription vkAttachmentDescription_array[1];
    memset((void*)vkAttachmentDescription_array, 0, sizeof(VkAttachmentDescription) * _ARRAYSIZE(vkAttachmentDescription_array));

    vkAttachmentDescription_array[0].flags = 0;
    vkAttachmentDescription_array[0].format =  vkFormat_color;
    vkAttachmentDescription_array[0].samples = VK_SAMPLE_COUNT_1_BIT;
    vkAttachmentDescription_array[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    vkAttachmentDescription_array[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    vkAttachmentDescription_array[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vkAttachmentDescription_array[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vkAttachmentDescription_array[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkAttachmentDescription_array[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Step 2: Create Attachment Reference Structure
    VkAttachmentReference vkAttachmentReference;
    memset((void*)&vkAttachmentReference, 0, sizeof(VkAttachmentReference));

    vkAttachmentReference.attachment = 0; // From the array of attachment description, refer to 0th index, oth will be color attachment
    vkAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 

    // STep 3: create sub pass description strcture
    VkSubpassDescription vkSubpassDescription;
    memset((void*)&vkSubpassDescription, 0, sizeof(VkSubpassDescription));
    
    vkSubpassDescription.flags = 0;
    vkSubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkSubpassDescription.inputAttachmentCount = 0;
    vkSubpassDescription.pInputAttachments = NULL;
    vkSubpassDescription.colorAttachmentCount = 1; // we have only one color attachment
    vkSubpassDescription.pColorAttachments = &vkAttachmentReference;
    vkSubpassDescription.pResolveAttachments = NULL;
    vkSubpassDescription.pDepthStencilAttachment = NULL;
    vkSubpassDescription.preserveAttachmentCount = 0;
    vkSubpassDescription.pPreserveAttachments = NULL;

    // Step 4: Render Pass Create Info
    VkRenderPassCreateInfo vkRenderPassCreateInfo;
    memset((void*)&vkRenderPassCreateInfo, 0, sizeof(VkRenderPassCreateInfo));

    vkRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    vkRenderPassCreateInfo.flags = 0;
    vkRenderPassCreateInfo.pNext = NULL;
    vkRenderPassCreateInfo.attachmentCount = _ARRAYSIZE(vkAttachmentDescription_array);
    vkRenderPassCreateInfo.pAttachments = vkAttachmentDescription_array;
    vkRenderPassCreateInfo.subpassCount = 1;
    vkRenderPassCreateInfo.pSubpasses = &vkSubpassDescription;
    vkRenderPassCreateInfo.dependencyCount = 0;
    vkRenderPassCreateInfo.pDependencies = NULL;
    

    // Step 5: Create Render Pass
    vkResult = vkCreateRenderPass(
        vkDevice,
        &vkRenderPassCreateInfo,
        NULL,
        &vkRenderPass
    );

    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createRenderPass(): vkCreateRenderPass() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createRenderPass(): vkCreateRenderPass() Successful!.\n");
    }

    return (vkResult);
}

VkResult createPipeline(void) {
    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Vertex Input Binding Description [ Vertex Input State]
    VkVertexInputBindingDescription vkVertexInputBindingDescription_array[2];
    memset((void*)vkVertexInputBindingDescription_array, 0, sizeof(VkVertexInputBindingDescription) * _ARRAYSIZE(vkVertexInputBindingDescription_array));

    vkVertexInputBindingDescription_array[0].binding = AMK_ATTRIBUTE_POSITION; // 0th binding index for position
    vkVertexInputBindingDescription_array[0].stride = sizeof(float) * 3;
    vkVertexInputBindingDescription_array[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vkVertexInputBindingDescription_array[1].binding = AMK_ATTRIBUTE_COLOR; // 1st binding index for color
    vkVertexInputBindingDescription_array[1].stride = sizeof(float) * 3;
    vkVertexInputBindingDescription_array[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vkVertexInputAttributeDescription_array[2];
    memset((void*)vkVertexInputAttributeDescription_array, 0, sizeof(VkVertexInputAttributeDescription) * _ARRAYSIZE(vkVertexInputAttributeDescription_array));

    // Position Attribute
    vkVertexInputAttributeDescription_array[0].binding = AMK_ATTRIBUTE_POSITION;
    vkVertexInputAttributeDescription_array[0].location = AMK_ATTRIBUTE_POSITION;
    vkVertexInputAttributeDescription_array[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vkVertexInputAttributeDescription_array[0].offset = 0;

    // Color Attribute
    vkVertexInputAttributeDescription_array[1].binding = AMK_ATTRIBUTE_COLOR;
    vkVertexInputAttributeDescription_array[1].location = AMK_ATTRIBUTE_COLOR;
    vkVertexInputAttributeDescription_array[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vkVertexInputAttributeDescription_array[1].offset = 0; 
    
    VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo;
    memset((void*)&vkPipelineVertexInputStateCreateInfo, 0, sizeof(VkPipelineVertexInputStateCreateInfo));

    vkPipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vkPipelineVertexInputStateCreateInfo.pNext = NULL;
    vkPipelineVertexInputStateCreateInfo.flags = 0;
    vkPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = _ARRAYSIZE(vkVertexInputBindingDescription_array);
    vkPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vkVertexInputBindingDescription_array;
    vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = _ARRAYSIZE(vkVertexInputAttributeDescription_array);
    vkPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vkVertexInputAttributeDescription_array;

    // Input Assembly State
    VkPipelineInputAssemblyStateCreateInfo vkPipelineInputAssemblyStateCreateInfo;
    memset((void*)&vkPipelineInputAssemblyStateCreateInfo, 0, sizeof(VkPipelineInputAssemblyStateCreateInfo));

    vkPipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    vkPipelineInputAssemblyStateCreateInfo.pNext = NULL;
    vkPipelineInputAssemblyStateCreateInfo.flags = 0;
    vkPipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterization State
    VkPipelineRasterizationStateCreateInfo vkPipelineRasterizationStateCreateInfo;
    memset((void*)&vkPipelineRasterizationStateCreateInfo, 0, sizeof(VkPipelineRasterizationStateCreateInfo));

    vkPipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    vkPipelineRasterizationStateCreateInfo.pNext = NULL;
    vkPipelineRasterizationStateCreateInfo.flags = 0;
    vkPipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    vkPipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    vkPipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    vkPipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

    // Color Blending State
    VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState_array[1];
    memset((void*)vkPipelineColorBlendAttachmentState_array, 0, sizeof(VkPipelineColorBlendAttachmentState) * _ARRAYSIZE(vkPipelineColorBlendAttachmentState_array));

    vkPipelineColorBlendAttachmentState_array[0].blendEnable = VK_FALSE;
    vkPipelineColorBlendAttachmentState_array[0].colorWriteMask = 0xF;

    VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo;
    memset((void*)&vkPipelineColorBlendStateCreateInfo, 0, sizeof(VkPipelineColorBlendStateCreateInfo));

    vkPipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    vkPipelineColorBlendStateCreateInfo.pNext = NULL;
    vkPipelineColorBlendStateCreateInfo.flags = 0;
    vkPipelineColorBlendStateCreateInfo.attachmentCount = _ARRAYSIZE(vkPipelineColorBlendAttachmentState_array);
    vkPipelineColorBlendStateCreateInfo.pAttachments = vkPipelineColorBlendAttachmentState_array;


    // Viewport Scissor State
    VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo;
    memset((void*)&vkPipelineViewportStateCreateInfo, 0, sizeof(VkPipelineViewportStateCreateInfo));

    vkPipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vkPipelineViewportStateCreateInfo.pNext = NULL;
    vkPipelineViewportStateCreateInfo.flags = 0;

    // Set the viewport/s
    vkPipelineViewportStateCreateInfo.viewportCount = 1;

    memset((void*)&vkViewport, 0, sizeof(VkViewport));
    vkViewport.x = 0;
    vkViewport.y = 0;
    vkViewport.width = (float)vkExtent2D_swapchain.width;
    vkViewport.height = (float)vkExtent2D_swapchain.height;
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;

    vkPipelineViewportStateCreateInfo.pViewports = &vkViewport;

    // Set the scissor rect/s
    vkPipelineViewportStateCreateInfo.scissorCount = 1;
    
    memset((void*)&vkRect2D_scissor, 0, sizeof(VkRect2D));
    vkRect2D_scissor.offset.x = 0;
    vkRect2D_scissor.offset.y = 0;
    vkRect2D_scissor.extent.width = vkExtent2D_swapchain.width;
    vkRect2D_scissor.extent.height = vkExtent2D_swapchain.height;

    vkPipelineViewportStateCreateInfo.pScissors = &vkRect2D_scissor;

    // Dynamic State
    // We don't have any dynamic state;

    // Multisample State
    VkPipelineMultisampleStateCreateInfo vkPipelineMultisampleStateCreateInfo;
    memset((void*)&vkPipelineMultisampleStateCreateInfo, 0, sizeof(VkPipelineMultisampleStateCreateInfo));

    vkPipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    vkPipelineMultisampleStateCreateInfo.pNext = NULL;
    vkPipelineMultisampleStateCreateInfo.flags = 0;
    vkPipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;


    // Shader Stage State
    VkPipelineShaderStageCreateInfo vkPipelineShaderStageCreateInfo_array[2];
    memset((void*)vkPipelineShaderStageCreateInfo_array, 0, sizeof(VkPipelineShaderStageCreateInfo) * _ARRAYSIZE(vkPipelineShaderStageCreateInfo_array));

    // Vertex Shader Stage
    vkPipelineShaderStageCreateInfo_array[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vkPipelineShaderStageCreateInfo_array[0].pNext = NULL;
    vkPipelineShaderStageCreateInfo_array[0].flags = 0;
    vkPipelineShaderStageCreateInfo_array[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    vkPipelineShaderStageCreateInfo_array[0].module = vkShaderModule_vertex;
    vkPipelineShaderStageCreateInfo_array[0].pName = "main"; // entry point name
    vkPipelineShaderStageCreateInfo_array[0].pSpecializationInfo = NULL;

    // Fragment Shader Stage
    vkPipelineShaderStageCreateInfo_array[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vkPipelineShaderStageCreateInfo_array[1].pNext = NULL;
    vkPipelineShaderStageCreateInfo_array[1].flags = 0;
    vkPipelineShaderStageCreateInfo_array[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    vkPipelineShaderStageCreateInfo_array[1].module = vkShaderModule_fragment;
    vkPipelineShaderStageCreateInfo_array[1].pName = "main"; // entry point name
    vkPipelineShaderStageCreateInfo_array[1].pSpecializationInfo = NULL;


    // Tessellation State
    // We don't have tessellation shaders so we can skip this state


    // Pipelines are created in a pipeline cache, we will create Pipeline cache object
    VkPipelineCacheCreateInfo vkPipelineCacheCreateInfo;
    memset((void*)&vkPipelineCacheCreateInfo, 0, sizeof(VkPipelineCacheCreateInfo));

    vkPipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkPipelineCacheCreateInfo.pNext = NULL;
    vkPipelineCacheCreateInfo.flags = 0;

    VkPipelineCache vkPipelineCache = VK_NULL_HANDLE;

    vkResult = vkCreatePipelineCache(vkDevice, &vkPipelineCacheCreateInfo, NULL, &vkPipelineCache);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createPipeline(): vkCreatePipelineCache() Failed!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createPipeline(): vkCreatePipelineCache() Successful!.\n");
    }

    // Create Graphics Pipeline
    VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo;
    memset((void*)&vkGraphicsPipelineCreateInfo, 0, sizeof(VkGraphicsPipelineCreateInfo));

    vkGraphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    vkGraphicsPipelineCreateInfo.pNext = NULL;
    vkGraphicsPipelineCreateInfo.flags = 0;
    vkGraphicsPipelineCreateInfo.pVertexInputState = &vkPipelineVertexInputStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pInputAssemblyState = &vkPipelineInputAssemblyStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pRasterizationState = &vkPipelineRasterizationStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pColorBlendState = &vkPipelineColorBlendStateCreateInfo;
    vkGraphicsPipelineCreateInfo.pViewportState = &vkPipelineViewportStateCreateInfo;   
    vkGraphicsPipelineCreateInfo.pDepthStencilState = NULL; // we don't have depth stencil state
    vkGraphicsPipelineCreateInfo.pDynamicState = NULL; // we don't have dynamic state
    vkGraphicsPipelineCreateInfo.pMultisampleState = &vkPipelineMultisampleStateCreateInfo;
    vkGraphicsPipelineCreateInfo.stageCount = _ARRAYSIZE(vkPipelineShaderStageCreateInfo_array);
    vkGraphicsPipelineCreateInfo.pStages = vkPipelineShaderStageCreateInfo_array;
    vkGraphicsPipelineCreateInfo.layout = vkPipelineLayout;
    vkGraphicsPipelineCreateInfo.renderPass = vkRenderPass;
    vkGraphicsPipelineCreateInfo.subpass = 0; // subpass index
    vkGraphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // no base pipeline handle
    vkGraphicsPipelineCreateInfo.basePipelineIndex = 0; // no base pipeline index

    // Create Graphics Pipeline
    vkResult = vkCreateGraphicsPipelines(vkDevice, vkPipelineCache, 1, &vkGraphicsPipelineCreateInfo, NULL, &vkPipeline);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createPipeline(): vkCreateGraphicsPipelines() Failed!.\n");
        // Destroy Pipeline Cache
        vkDestroyPipelineCache(vkDevice, vkPipelineCache, NULL);
        vkPipelineCache = VK_NULL_HANDLE;
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createPipeline(): vkCreateGraphicsPipelines() Successful!.\n");
    }

    // Destroy Pipeline Cache
    vkDestroyPipelineCache(vkDevice, vkPipelineCache, NULL);
    vkPipelineCache = VK_NULL_HANDLE;

    return (vkResult);
}

VkResult createFramebuffers(void) {
    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Step 1: create VkImageView array same as size of attachments
    VkImageView vkImageView_attachments_array[1];
    memset((void*)vkImageView_attachments_array, 0, sizeof(VkImageView) * _ARRAYSIZE(vkImageView_attachments_array));

    // Step 2: Create VkFrameBufferCreateInfo structure
    VkFramebufferCreateInfo vkFrameBufferCreateInfo;
    memset((void*)&vkFrameBufferCreateInfo, 0, sizeof(VkFramebufferCreateInfo));

    vkFrameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    vkFrameBufferCreateInfo.flags = 0;
    vkFrameBufferCreateInfo.pNext = NULL;
    vkFrameBufferCreateInfo.renderPass = vkRenderPass;
    vkFrameBufferCreateInfo.attachmentCount = _ARRAYSIZE(vkImageView_attachments_array);
    vkFrameBufferCreateInfo.pAttachments = vkImageView_attachments_array;
    vkFrameBufferCreateInfo.width = vkExtent2D_swapchain.width;
    vkFrameBufferCreateInfo.height = vkExtent2D_swapchain.height;
    vkFrameBufferCreateInfo.layers = 1; // VALIDATION USE CASE 2: Comment this line to see the error

    // allocate frame buffers array and creat efream buffers in loop with counts of allocated swapchain images
    vkFramebuffer_array = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * swapchainImageCount);
    for(uint32_t i = 0; i < swapchainImageCount; i++) {

        vkImageView_attachments_array[0] = swapchainImageView_array[i];

        vkResult = vkCreateFramebuffer(vkDevice, &vkFrameBufferCreateInfo, NULL, &vkFramebuffer_array[i]);
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createFramebuffers(): vkCreateFramebuffer() Failed at {%d}!.\n", i);
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createFramebuffers(): vkCreateFramebuffer() Successful for {%d}!.\n", i);
        }
    }

    return (vkResult);
}

VkResult createSemaphores(void) {
    // Variables
    VkResult vkResult = VK_SUCCESS;

    // Create Semaphore info
    VkSemaphoreCreateInfo vkSemaphoreCreateInfo;
    memset((void*)&vkSemaphoreCreateInfo, 0, sizeof(VkSemaphoreCreateInfo));

    vkSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkSemaphoreCreateInfo.pNext = NULL;
    vkSemaphoreCreateInfo.flags = 0; // it's reserved must be zero

    // By defualt if no type is specified, binary semaphore is created!

    // create semaphore for backbuffer
    vkResult = vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, NULL, &vkSemaphore_backbuffer);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSemaphores(): vkCreateSemaphore() Failed for Back Buffer Semaphore!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSemaphores(): vkCreateSemaphore() Successful for Back Buffer Semaphore!.\n");
    }

    // create semaphore for render complete
    vkResult = vkCreateSemaphore(vkDevice, &vkSemaphoreCreateInfo, NULL, &vkSemaphore_rendercomplete);
    if(vkResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSemaphores(): vkCreateSemaphore() Failed for Render Complete Semaphore!.\n");
        return (vkResult);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createSemaphores(): vkCreateSemaphore() Successful for Render Complete Semaphore!.\n");
    }

    return (vkResult);
}

VkResult createFences(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    // VkFenceCreateInfo
    VkFenceCreateInfo vkFenceCreateInfo;
    memset((void*)&vkFenceCreateInfo, 0, sizeof(VkFenceCreateInfo));

    vkFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkFenceCreateInfo.pNext = NULL;
    vkFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkFence_array = (VkFence*) malloc(sizeof(VkFence) * swapchainImageCount);


    for(uint32_t i = 0; i < swapchainImageCount; i++) {
        vkResult = vkCreateFence(vkDevice, &vkFenceCreateInfo, NULL, &vkFence_array[i]);
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createFences(): vkCreateFence() Failed at {%d}!.\n", i);
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "createFences(): vkCreateFence() Successful for {%d}!.\n", i);
        }
    }

    return (vkResult);
}

VkResult buildCommandBuffers(void) {
    // variables
    VkResult vkResult = VK_SUCCESS;

    for(uint32_t i = 0; i < swapchainImageCount; i++) {
        // Reset Command Buffers
        vkResult = vkResetCommandBuffer(vkCommandBuffer_array[i], 0); 
        // adding 0 here means sdon't release resources allocated by command pool
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "buildCommandBuffers(): vkResetCommandBuffer() Failed for {%d}!.\n", i);
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "buildCommandBuffers(): vkResetCommandBuffer() Successful for {%d}!.\n", i);
        }

        // set VkCommandBufferBeginInfo
        VkCommandBufferBeginInfo vkCommandBufferBeginInfo;
        memset((void*)&vkCommandBufferBeginInfo, 0, sizeof(VkCommandBufferBeginInfo));

        vkCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkCommandBufferBeginInfo.pNext = NULL;
        vkCommandBufferBeginInfo.flags = 0; 
        // Zero indicates that we'll use primary command buffer and also specifying that we are not
        // using this buffer simultaniously between multiple threads

        vkResult = vkBeginCommandBuffer(vkCommandBuffer_array[i], &vkCommandBufferBeginInfo);
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "buildCommandBuffers(): vkBeginCommandBuffer() Failed for {%d}!.\n", i);
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "buildCommandBuffers(): vkBeginCommandBuffer() Successful for {%d}!.\n", i);
        }

        // Set Clear Values
        VkClearValue vkClearValue_array[1];
        memset((void*)vkClearValue_array, 0, sizeof(VkClearValue) * _ARRAYSIZE(vkClearValue_array));

        vkClearValue_array[0].color = vkClearColorValue;

        // Render pass begin info
        VkRenderPassBeginInfo vkRenderPassBeginInfo;
        memset((void*)&vkRenderPassBeginInfo, 0, sizeof(VkRenderPassBeginInfo));

        vkRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vkRenderPassBeginInfo.pNext = NULL;
        vkRenderPassBeginInfo.renderPass = vkRenderPass;
        vkRenderPassBeginInfo.renderArea.offset.x = 0;
        vkRenderPassBeginInfo.renderArea.offset.y = 0;
        vkRenderPassBeginInfo.renderArea.extent.width = vkExtent2D_swapchain.width;
        vkRenderPassBeginInfo.renderArea.extent.height = vkExtent2D_swapchain.height;
        vkRenderPassBeginInfo.clearValueCount = _ARRAYSIZE(vkClearValue_array);
        vkRenderPassBeginInfo.pClearValues = vkClearValue_array;
        vkRenderPassBeginInfo.framebuffer = vkFramebuffer_array[i];

        // begin render pass
        vkCmdBeginRenderPass(vkCommandBuffer_array[i], &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        // content of this pass are subpass and part of primary command buffers so inline

        // Bind with the pipeline
        vkCmdBindPipeline(vkCommandBuffer_array[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

        // Bind Descriptor Set
        vkCmdBindDescriptorSets(
            vkCommandBuffer_array[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vkPipelineLayout,
            0, 1,
            &vkDescriptorSet, // this is the descriptor set we want to bind
            0, NULL
        );

        // Bind with the vertex buffer
        VkDeviceSize vkDeviceSize_offset_position_array[1];
        memset((void*)vkDeviceSize_offset_position_array, 0, sizeof(VkDeviceSize) * _ARRAYSIZE(vkDeviceSize_offset_position_array));

        vkCmdBindVertexBuffers(
            vkCommandBuffer_array[i], 
            AMK_ATTRIBUTE_POSITION, 1,
            &vertexData_position.vkBuffer,
            vkDeviceSize_offset_position_array
        );

        // Color Buffer Binding
        VkDeviceSize vkDeviceSize_offset_color_array[1];
        memset((void*)vkDeviceSize_offset_color_array, 0, sizeof(VkDeviceSize) * _ARRAYSIZE(vkDeviceSize_offset_color_array));

        vkCmdBindVertexBuffers(
            vkCommandBuffer_array[i], 
            AMK_ATTRIBUTE_COLOR, 1,
            &vertexData_color.vkBuffer,
            vkDeviceSize_offset_color_array
        );

        // Here we should call vulkan drawing functions!
        vkCmdDraw(vkCommandBuffer_array[i], 3, 1, 0, 0);

        // End Render Pass
        vkCmdEndRenderPass(vkCommandBuffer_array[i]);

        // End command buffer recording
        vkResult = vkEndCommandBuffer(vkCommandBuffer_array[i]);
        if(vkResult != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "buildCommandBuffers(): vkEndCommandBuffer() Failed for {%d}!.\n", i);
            return (vkResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "buildCommandBuffers(): vkEndCommandBuffer() Successful for {%d}!.\n", i);
        }
    }

    return (vkResult);
}

// Always Keep this function at the end of this file
VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT vkDebugReportFlagsEXIT, 
    VkDebugReportObjectTypeEXT vkDebugReportObjectTypeEXIT, 
    uint64_t object, 
    size_t location, 
    int32_t messageCode, 
    const char* pLayerPrefix, 
    const char* pMessage, 
    void* pUserData
) {
    __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "AMK_VALIDATION: debugReportCallback() :  %s (%d) = %s\n", pLayerPrefix, messageCode, pMessage);
    return VK_FALSE; // return false to ignore this message
}