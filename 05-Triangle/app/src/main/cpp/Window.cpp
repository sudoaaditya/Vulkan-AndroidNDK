#include <android_native_app_glue.h> // for android_main and all native app glue code
#include <android/log.h> // for __android_log_print

#include <memory.h> // for memset
#include <math.h> // for sqrtf

typedef struct {
    android_app *app;
    bool bActive;
} Engine;

ANativeWindow *androidNativeWindow = NULL;

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

// Global Callback functions
void engine_handle_cmd(struct android_app*, int32_t);
int32_t engine_handle_input(struct android_app*, AInputEvent*);

void android_main(struct android_app* state) {
    
    // Code

    // Variables
    JavaVM *vm = state->activity->vm;
    JNIEnv *env = NULL;

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
    }
}


// callback for handling app commands
void engine_handle_cmd(struct android_app* app, int32_t cmd) {

    // Code
    Engine *engine = (Engine*)app->userData;

    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            // here we save the state of app when app is paused or stopped
            engine->bActive = false;    
        break;

        case APP_CMD_INIT_WINDOW:
            // here we initialize the window when app is resumed or started
            if (engine->app->window != NULL) {
                androidNativeWindow = engine->app->window;
                // Draw something to prevent the brief flash of home screen issue when using deprecated layout flags
                // Draw background color with pixel by pixel coloring by CPU
                ANativeWindow_Buffer buffer;
                uint32_t *pixels = NULL;
                uint32_t color;
                int x, y;
                // set the buffer geometry and format
                ANativeWindow_setBuffersGeometry(androidNativeWindow, 0, 0, WINDOW_FORMAT_RGBA_8888);
                if(ANativeWindow_lock(androidNativeWindow, &buffer, NULL) == 0) {
                    pixels = (uint32_t*)buffer.bits;
                    color = 0xFF000000; // [ color set is in ABGR format ]
                    for (y = 0; y < buffer.height; y++) {
                        for (x = 0; x < buffer.width; x++) {
                            pixels[y * buffer.stride + x] = color;
                        }
                    }
                    ANativeWindow_unlockAndPost(androidNativeWindow);
                }
                engine->bActive = true;
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Window Created!");
            } else {
                androidNativeWindow = NULL;
            }
        break;

        case APP_CMD_TERM_WINDOW:
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

