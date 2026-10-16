#!/system/bin/sh

printf "==================================\n"
printf "  Il2CppDumper - AntiGravity Exss  \n"
printf "==================================\n"
printf "\n"

PACKAGE_NAME=$(getprop persist.il2cppdumper.package)

if [ -z "$PACKAGE_NAME" ]; then
    setprop persist.il2cppdumper.package "com.example.game"
    PACKAGE_NAME="com.example.game"
    printf "No package set. Default applied.\n"
    printf "\n"
fi

printf "Target package : %s\n" "$PACKAGE_NAME"
printf "Full process   : %s:UnityKillsMe\n" "$PACKAGE_NAME"
printf "\n"
printf "To change the target game, run:\n"
printf "  setprop persist.il2cppdumper.package com.new.game\n"
printf "\n"
printf "Or use the WebUI from your root manager.\n"
printf "\n"
printf "Powered by AntiGravity Exss\n"
