//
// Created by Perfare on 2020/7/4.
// Modified by AntiGravity Exss
//

#include "hack.h"
#include "il2cpp_dump.h"
#include "log.h"
#include "xdl.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <jni.h>
#include <thread>
#include <sys/mman.h>
#include <linux/unistd.h>
#include <array>

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        auto package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        auto app_data_dir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        preSpecialize(package_name, app_data_dir);
        env->ReleaseStringUTFChars(args->nice_name, package_name);
        env->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (enable_hack) {
            std::thread hack_thread(hack_prepare, game_data_dir, data, length);
            hack_thread.detach();
        }
    }

private:
    Api *api{};
    JNIEnv *env{};
    bool enable_hack{};
    char *game_data_dir{};
    void *data{};
    size_t length{};

    void preSpecialize(const char *package_name, const char *app_data_dir) {
        // Baca nama paket dasar dari property (diisi via WebUI)
        char base_package[256] = {0};
        __system_property_get("persist.il2cppdumper.package", base_package);

        if (base_package[0] == '\0') {
            __system_property_get("debug.il2cppdumper.package", base_package);
        }

        if (base_package[0] == '\0') {
            snprintf(base_package, sizeof(base_package), "com.mobile.legends");
        }

        // Bangun nama proses Unity
        char unity_package[512] = {0};
        snprintf(unity_package, sizeof(unity_package), "%s:UnityKillsMe", base_package);

        // Cek apakah package yang sedang di-specialize cocok
        bool match = false;
        if (strcmp(package_name, base_package) == 0) {
            match = true;
        } else if (strcmp(package_name, unity_package) == 0) {
            match = true;
        } else if (strchr(base_package, ':') != nullptr &&
                   strcmp(package_name, base_package) == 0) {
            match = true;
        }

        if (match) {
            LOGI("detect game: %s", package_name);
            enable_hack = true;
            game_data_dir = new char[strlen(app_data_dir) + 1];
            strcpy(game_data_dir, app_data_dir);

#if defined(__i386__)
            auto path = "zygisk/armeabi-v7a.so";
#endif
#if defined(__x86_64__)
            auto path = "zygisk/arm64-v8a.so";
#endif
#if defined(__i386__) || defined(__x86_64__)
            int dirfd = api->getModuleDir();
            int fd = openat(dirfd, path, O_RDONLY);
            if (fd != -1) {
                struct stat sb{};
                if (fstat(fd, &sb) == 0 && sb.st_size > 0) {
                    length = sb.st_size;
                    data = mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0);
                    if (data == MAP_FAILED) {
                        LOGW("Unable to mmap arm file");
                        data = nullptr;
                        length = 0;
                    }
                } else {
                    LOGW("Unable to stat arm file");
                }
                close(fd);
            } else {
                LOGW("Unable to open arm file");
            }
            if (data == nullptr) {
                enable_hack = false;
            }
#endif
        } else {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }
};

REGISTER_ZYGISK_MODULE(MyModule)

void hack_start(const char *game_data_dir) {
    LOGI("Il2CppDumper by AntiGravity Exss");

    const char *target_libs[] = {
        "libcsharp.so",
        "liblogic.so",
        "libil2cpp.so"
    };

    bool load = false;

    for (int attempt = 0; attempt < 20; attempt++) {
        for (const char *lib_name : target_libs) {
            void *handle = xdl_open(lib_name, 0);
            if (handle) {
                LOGI("%s found, starting dump...", lib_name);
                load = true;
                il2cpp_api_init(handle);
                il2cpp_dump(game_data_dir);
                break;
            }
        }
        if (load) break;
        sleep(1);
    }

    if (!load) {
        LOGI("No target library found in thread %d", gettid());
    }
}

std::string GetLibDir(JavaVM *vms) {
    JNIEnv *env = nullptr;
    vms->AttachCurrentThread(&env, nullptr);
    jclass activity_thread_clz = env->FindClass("android/app/ActivityThread");
    if (activity_thread_clz != nullptr) {
        jmethodID currentApplicationId = env->GetStaticMethodID(activity_thread_clz,
                                                                "currentApplication",
                                                                "()Landroid/app/Application;");
        if (currentApplicationId) {
            jobject application = env->CallStaticObjectMethod(activity_thread_clz,
                                                              currentApplicationId);
            if (application == nullptr) {
                LOGE("currentApplication returned null");
                return {};
            }
            jclass application_clazz = env->GetObjectClass(application);
            if (application_clazz) {
                jmethodID get_application_info = env->GetMethodID(application_clazz,
                                                                  "getApplicationInfo",
                                                                  "()Landroid/content/pm/ApplicationInfo;");
                if (get_application_info) {
                    jobject application_info = env->CallObjectMethod(application,
                                                                     get_application_info);
                    jfieldID native_library_dir_id = env->GetFieldID(
                            env->GetObjectClass(application_info), "nativeLibraryDir",
                            "Ljava/lang/String;");
                    if (native_library_dir_id) {
                        auto native_library_dir_jstring = (jstring) env->GetObjectField(
                                application_info, native_library_dir_id);
                        auto path = env->GetStringUTFChars(native_library_dir_jstring, nullptr);
                        LOGI("lib dir %s", path);
                        std::string lib_dir(path);
                        env->ReleaseStringUTFChars(native_library_dir_jstring, path);
                        return lib_dir;
                    } else {
                        LOGE("nativeLibraryDir not found");
                    }
                } else {
                    LOGE("getApplicationInfo not found");
                }
            } else {
                LOGE("application class not found");
            }
        } else {
            LOGE("currentApplication not found");
        }
    } else {
        LOGE("ActivityThread not found");
    }
    return {};
}

static std::string GetNativeBridgeLibrary() {
    auto value = std::array<char, PROP_VALUE_MAX>();
    __system_property_get("ro.dalvik.vm.native.bridge", value.data());
    return {value.data()};
}

struct NativeBridgeCallbacks {
    uint32_t version;
    void *initialize;

    void *(*loadLibrary)(const char *libpath, int flag);

    void *(*getTrampoline)(void *handle, const char *name, const char *shorty, uint32_t len);

    void *isSupported;
    void *getAppEnv;
    void *isCompatibleWith;
    void *getSignalHandler;
    void *unloadLibrary;
    void *getError;
    void *isPathSupported;
    void *initAnonymousNamespace;
    void *createNamespace;
    void *linkNamespaces;

    void *(*loadLibraryExt)(const char *libpath, int flag, void *ns);
};

static JavaVM *WaitForRuntime(void *libart) {
    auto JNI_GetCreatedJavaVMs = (jint (*)(JavaVM **, jsize, jsize *)) dlsym(
            libart, "JNI_GetCreatedJavaVMs");
    LOGI("JNI_GetCreatedJavaVMs %p", JNI_GetCreatedJavaVMs);
    if (!JNI_GetCreatedJavaVMs) {
        LOGE("dlsym JNI_GetCreatedJavaVMs failed");
        return nullptr;
    }
    for (int i = 0; i < 300; i++) {
        JavaVM *vms_buf[1];
        jsize num_vms = 0;
        if (JNI_GetCreatedJavaVMs(vms_buf, 1, &num_vms) == JNI_OK && num_vms > 0) {
            JavaVM *vms = vms_buf[0];
            JNIEnv *env = nullptr;
            if (vms->AttachCurrentThread(&env, nullptr) == JNI_OK && env != nullptr) {
                bool ready = false;
                jclass activity_thread_clz = env->FindClass("android/app/ActivityThread");
                if (activity_thread_clz != nullptr) {
                    jmethodID currentApplicationId = env->GetStaticMethodID(
                            activity_thread_clz, "currentApplication",
                            "()Landroid/app/Application;");
                    if (currentApplicationId) {
                        jobject application = env->CallStaticObjectMethod(
                                activity_thread_clz, currentApplicationId);
                        if (application != nullptr) {
                            ready = true;
                            env->DeleteLocalRef(application);
                        }
                    }
                    env->DeleteLocalRef(activity_thread_clz);
                }
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                }
                if (ready) {
                    return vms;
                }
            }
        }
        usleep(100 * 1000);
    }
    LOGE("timed out waiting for runtime/application");
    return nullptr;
}

bool NativeBridgeLoad(const char *game_data_dir, int api_level, void *data, size_t length) {
    auto libart = dlopen("libart.so", RTLD_NOW);
    if (!libart) {
        LOGE("dlopen libart.so failed");
        return false;
    }
    JavaVM *vms = WaitForRuntime(libart);
    if (!vms) {
        LOGE("runtime/application not ready");
        return false;
    }

    auto lib_dir = GetLibDir(vms);
    if (lib_dir.empty()) {
        LOGE("GetLibDir error");
        return false;
    }
    if (lib_dir.find("/lib/x86") != std::string::npos) {
        LOGI("no need NativeBridge");
        munmap(data, length);
        return false;
    }

    auto nb = dlopen("libhoudini.so", RTLD_NOW);
    if (!nb) {
        auto native_bridge = GetNativeBridgeLibrary();
        LOGI("native bridge: %s", native_bridge.data());
        nb = dlopen(native_bridge.data(), RTLD_NOW);
    }
    if (nb) {
        LOGI("nb %p", nb);
        auto callbacks = (NativeBridgeCallbacks *) dlsym(nb, "NativeBridgeItf");
        if (callbacks) {
            LOGI("NativeBridgeLoadLibrary %p", callbacks->loadLibrary);
            LOGI("NativeBridgeLoadLibraryExt %p", callbacks->loadLibraryExt);
            LOGI("NativeBridgeGetTrampoline %p", callbacks->getTrampoline);

            int fd = (int) syscall(__NR_memfd_create, "anon", MFD_CLOEXEC);
            if (fd == -1) {
                LOGE("memfd_create failed");
                munmap(data, length);
                return false;
            }
            if (ftruncate(fd, (off_t) length) == -1) {
                LOGE("ftruncate failed");
                close(fd);
                munmap(data, length);
                return false;
            }
            void *mem = mmap(nullptr, length, PROT_WRITE, MAP_SHARED, fd, 0);
            if (mem == MAP_FAILED) {
                LOGE("mmap memfd failed");
                close(fd);
                munmap(data, length);
                return false;
            }
            memcpy(mem, data, length);
            munmap(mem, length);
            munmap(data, length);
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "/proc/self/fd/%d", fd);
            LOGI("arm path %s", path);

            void *arm_handle;
            if (api_level >= 26) {
                arm_handle = callbacks->loadLibraryExt(path, RTLD_NOW, (void *) 3);
            } else {
                arm_handle = callbacks->loadLibrary(path, RTLD_NOW);
            }
            if (arm_handle) {
                LOGI("arm handle %p", arm_handle);
                auto init = (void (*)(JavaVM *, void *)) callbacks->getTrampoline(arm_handle,
                                                                                  "JNI_OnLoad",
                                                                                  nullptr, 0);
                LOGI("JNI_OnLoad %p", init);
                init(vms, (void *) game_data_dir);
                return true;
            }
            close(fd);
        }
    }
    return false;
}

void hack_prepare(const char *game_data_dir, void *data, size_t length) {
    LOGI("hack thread: %d", gettid());
    int api_level = android_get_device_api_level();
    LOGI("api level: %d", api_level);

#if defined(__i386__) || defined(__x86_64__)
    if (!NativeBridgeLoad(game_data_dir, api_level, data, length)) {
#endif
        hack_start(game_data_dir);
#if defined(__i386__) || defined(__x86_64__)
    }
#endif
}

#if defined(__arm__) || defined(__aarch64__)

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    auto game_data_dir = (const char *) reserved;
    std::thread hack_thread(hack_start, game_data_dir);
    hack_thread.detach();
    return JNI_VERSION_1_6;
}

#endif
