#!/usr/bin/bash
set -e
BUILD_DIR=${HOME}/sources/chengos/build/esp
EDK2_DIR=${HOME}/sources/edk2
EDK2_CCLDR_DIR=${EDK2_DIR}/MdeModulePkg/Application/CcLoader

pushd ${EDK2_DIR}
source edksetup.sh BaseTools
build
popd
mkdir -pv ${BUILD_DIR}
echo ""
nasm -f bin -o ${BUILD_DIR}/ccldr ${EDK2_CCLDR_DIR}/src/ccldr.asm
echo "NASMing	${BUILD_DIR}/ccldr"
echo ""
cp -vf ${HOME}/sources/edk2/Build/MdeModule/DEBUG_GCC5/X64/CcLoader.efi ${BUILD_DIR}/EFI/BOOT/BOOTX64.EFI
echo ""
echo "Compiled successfully"
echo ""