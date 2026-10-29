//
// Modified by AntiGravity Exss
//

#ifndef ZYGISK_IL2CPPDUMPER_GAME_H
#define ZYGISK_IL2CPPDUMPER_GAME_H

#include <cstring>
#include <cstdio>
#include "log.h"

#include <cctype>
#include <sys/system_properties.h>

inline bool UsesUnityKillsMe(const char *pkg) {
    const char *known[] = {
        "com.mobile.legends",
        "com.mobile.legends.usa",
        "com.fyrefly.f21",
        // tambahkan varian lain yang sudah terbukti di sini
    };
    for (const char *k : known) {
        if (strcmp(pkg, k) == 0) return true;
    }
    return false;
}

inline const char *GetGamePackageName() {
    static char package_name[256] = {0};
    static bool initialized = false;

    if (!initialized) {
        char base_package[256] = {0};

        __system_property_get("persist.il2cppdumper.package", base_package);

        if (base_package[0] == '\0') {
            __system_property_get("debug.il2cppdumper.package", base_package);
        }

        if (base_package[0] == '\0') {
            snprintf(base_package, sizeof(base_package), "com.example.game");
        }

        if (strchr(base_package, ':') != nullptr) {
            snprintf(package_name, sizeof(package_name), "%s", base_package);
        }
        else if (UsesUnityKillsMe(base_package)) {
            snprintf(package_name, sizeof(package_name), "%s:UnityKillsMe", base_package);
        }
        else {
            snprintf(package_name, sizeof(package_name), "%s", base_package);
        }

        initialized = true;
    }
    return package_name;
}

#define GamePackageName GetGamePackageName()

#endif //ZYGISK_IL2CPPDUMPER_GAME_H
