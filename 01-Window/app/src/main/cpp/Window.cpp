#include <android_native_app_glue.h> // for android_main and all native app glue code
#include <android/log.h> // for __android_log_print

#include <memory.h> // for memset

typedef struct {
    android_app *app;
    bool bActive;
} Engine;

ANativeWindow *androidNativeWindow = NULL;

// Global Callback functions
void engine_handle_cmd(struct android_app*, int32_t);
int32_t engine_handle_input(struct android_app*, AInputEvent*);

void android_main(struct android_app* state) {
    
    // Code
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
                androidNativeWindow = engine->app->window;;
                __android_log_print(ANDROID_LOG_INFO, "AMK-Ndk:", "Window Created!");
            } else {
                androidNativeWindow = NULL;
            }
            engine->bActive = true;
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
    return 0;
}

