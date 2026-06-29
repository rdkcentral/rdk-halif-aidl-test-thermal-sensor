#!/bin/bash
#* ******************************************************************************
#*  If not stated otherwise in this file or this component's LICENSE
#*  file the following copyright and licenses apply:
#*
#*  Copyright 2026 RDK Management
#*
#*  Licensed under the Apache License, Version 2.0 (the License);
#*  you may not use this file except in compliance with the License.
#*  You may obtain a copy of the License at
#*
#*  http://www.apache.org/licenses/LICENSE-2.0
#*
#*  Unless required by applicable law or agreed to in writing, software
#*  distributed under the License is distributed on an AS IS BASIS,
#*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#*  See the License for the specific language governing permissions and
#*  limitations under the License.
#*
#* ******************************************************************************

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

declare -r TOP=`dirname $(realpath $0)`
declare -r HALIF_VERSION=current
RDK_HALIF_AIDL_VERSION="${RDK_HALIF_AIDL_VERSION="main"}"
RDK_HALIF_TEST_BASE_URL="https://github.com/rdkcentral/rdk-halif-binder-test-"

declare -A HALIF_COMPONENTS=(
  [sensor]="current"
)

declare -A HALIF_TEST_COMPONENTS=(
  [sensor]="RDK_HALIF_THERMALSENSOR_TEST_VERSION"
)

# Define the directory where the repository will be cloned
declare -r UT_CORE_DIR="${TOP}/ut-core"
declare -r TOP_BUILD_DIR="${TOP}/build"
declare -r BUILD_INSTALL_PREFIX=${TOP_BUILD_DIR}/usr
declare -r AIDL_LIB_DIR=${TOP}/aidl_lib
declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl
declare -r PROFILES_DIR=${TOP}/profiles

OUT_TAR_FILE=vts-thermalsensor.tar.gz

LOCAL_BINDER_TOOLCHAIN_DIR=${BINDER_SOURCE_DIR:-${TOP}/build-tools/linux_binder_idl}
LOCAL_BINDER_SDK_DIR=${BINDER_SDK_DIR:-${TOP}/out/target}
LOCAL_BINDER_INCLUDE_DIR=${BINDER_SDK_INCLUDE_DIR:-${TOP}/out/build/include/binder_sdk}
LOCAL_BINDER_LIB_DIR=${BINDER_SDK_LIB_DIR:-${LOCAL_BINDER_SDK_DIR}/lib/binder}

mkdir -p $TOP_BUILD_DIR

# Parse Target argument (e.g., Target=arm or target=linux)
# Default to 'linux' if not provided
TARGET="linux"

###############################################################################
# Function to validate TARGET option
###############################################################################
for arg in "$@"; do
    if [[ $arg == [Tt]arget=* ]]; then
        TARGET="${arg#*=}"
    fi
done

# Validate TARGET
valid_targets=("linux" "arm")
if [[ ! " ${valid_targets[@]} " =~ " ${TARGET} " ]]; then
    echo -e "${BOLD}${RED}Invalid Target: $TARGET. Valid targets are: ${valid_targets[@]}${RESET}"
    help
    exit 1
fi

###############################################################################
# Function to validate or stage the local Binder SDK via rdk-halif-aidl/build_binder.sh
###############################################################################
verify_local_binder_sdk()
{
    local sdk_ready="${LOCAL_BINDER_SDK_DIR}/.sdk_ready"
    local binder_builder="${TOP}/build_binder.sh"

    if [ ! -f "${sdk_ready}" ]; then
        echo -e "${BOLD}${YELLOW}Local Binder SDK not staged. Bootstrapping via ${binder_builder}${RESET}"
        BINDER_SOURCE_DIR="${LOCAL_BINDER_TOOLCHAIN_DIR}" "${binder_builder}"
        if [ $? -ne 0 ] || [ ! -f "${sdk_ready}" ]; then
            echo -e "${BOLD}${RED}ERROR: Local Binder SDK is not staged at ${LOCAL_BINDER_SDK_DIR}${RESET}"
            exit 1
        fi
    fi

    if [ ! -d "${LOCAL_BINDER_INCLUDE_DIR}" ]; then
        echo -e "${BOLD}${RED}ERROR: Local Binder headers not found: ${LOCAL_BINDER_INCLUDE_DIR}${RESET}"
        exit 1
    fi

    if [ ! -d "${LOCAL_BINDER_LIB_DIR}" ]; then
        echo -e "${BOLD}${RED}ERROR: Local Binder libraries not found: ${LOCAL_BINDER_LIB_DIR}${RESET}"
        exit 1
    fi
}

###############################################################################
# Install Binder service manager, headers and libraries into build/usr/ from
# the rdk-halif-aidl Binder SDK (built by verify_local_binder_sdk).
# Must be called after verify_local_binder_sdk().
###############################################################################
install_linux_binder()
{
    local BINDER_SDK="${LOCAL_BINDER_SDK_DIR}"

    echo "Installing binder service manager, headers and libraries..."
    install -d ${BUILD_INSTALL_PREFIX}/bin ${BUILD_INSTALL_PREFIX}/lib ${BUILD_INSTALL_PREFIX}/include

    if [ -d "${BINDER_SDK}/bin" ]; then
        find "${BINDER_SDK}/bin" -maxdepth 1 -type f -executable \
            -exec install -t ${BUILD_INSTALL_PREFIX}/bin {} \; 2>/dev/null || true
    fi

    if [ -d "${BINDER_SDK}/lib/binder" ]; then
        find "${BINDER_SDK}/lib/binder" -maxdepth 1 \( -name "*.so" -o -name "*.so.*" \) \
            -exec install -t ${BUILD_INSTALL_PREFIX}/lib {} \; 2>/dev/null || true
    fi

    if [ -d "${LOCAL_BINDER_INCLUDE_DIR}" ]; then
        cp -apr "${LOCAL_BINDER_INCLUDE_DIR}/." ${BUILD_INSTALL_PREFIX}/include/
    fi

    # Bundle host libstdc++ so the binary runs on targets with older system libs
    # (avoids GLIBCXX_x.x.xx not found errors at runtime)
    local libstdcpp
    libstdcpp=$(realpath "$(g++ -print-file-name=libstdc++.so.6)" 2>/dev/null)
    if [ -f "${libstdcpp}" ]; then
        install -t ${BUILD_INSTALL_PREFIX}/lib "${libstdcpp}"
        # Create the soname symlink (libstdc++.so.6) that the dynamic linker looks for
        local libstdcpp_basename
        libstdcpp_basename=$(basename "${libstdcpp}")
        ln -sf "${libstdcpp_basename}" "${BUILD_INSTALL_PREFIX}/lib/libstdc++.so.6"
        echo "Bundled host libstdc++: ${libstdcpp}"
    fi
}

###############################################################################
# Function to clone rdk-halif-aidl and resolve versioned src/include dirs
###############################################################################
resolve_halif_versions()
{
    # Clone the repository if not already present
    if [ ! -d "$RDK_HAL_DIR" ]; then
        echo "Checking out version: $RDK_HALIF_AIDL_VERSION"
        git clone -b $RDK_HALIF_AIDL_VERSION 'https://github.com/rdkcentral/rdk-halif-aidl.git' $RDK_HAL_DIR
        if [ $? -ne 0 ]; then
            echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
            echo -e "${BOLD}* ${RED}Failed to clone rdk-halif-aidl. Exiting.${RESET}"
            echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
            exit 1
        fi
    fi

    for component in "${!HALIF_COMPONENTS[@]}"; do
        local metadata="${RDK_HAL_DIR}/${component}/metadata.yaml"
        if [ ! -f "$metadata" ]; then
            echo -e "${BOLD}${RED}ERROR: metadata.yaml not found: ${metadata}${RESET}"
            exit 1
        fi

        local version
        version=$(grep -m1 '^version:' "$metadata" | awk '{print $2}')
        if [ -z "$version" ]; then
            echo -e "${BOLD}${RED}ERROR: Could not parse version from ${metadata}${RESET}"
            exit 1
        fi

        local inc_dir="${RDK_HAL_DIR}/${component}/${version}/include"
        local src_dir="${RDK_HAL_DIR}/${component}/${version}/src"

        if [ ! -d "$inc_dir" ]; then
            echo -e "${BOLD}${RED}ERROR: versioned include dir not found: ${inc_dir}${RESET}"
            exit 1
        fi
        if [ ! -d "$src_dir" ]; then
            echo -e "${BOLD}${RED}ERROR: versioned src dir not found: ${src_dir}${RESET}"
            exit 1
        fi

        echo -e "${BOLD}${GREEN}${component}${RESET}: version=${YELLOW}${version}${RESET}"
        HALIF_AIDL_SRC_DIR=$(realpath "$src_dir")
        HALIF_AIDL_INC_DIR=$(realpath "$inc_dir")
    done
}

###############################################################################
# Install HALIF pre-generated headers into build/usr/include/
###############################################################################
install_halif_headers()
{
    echo "Installing HALIF headers..."
    install -d ${BUILD_INSTALL_PREFIX}/include
    if [ -d "${HALIF_AIDL_INC_DIR}" ]; then
        cp -apr "${HALIF_AIDL_INC_DIR}/." "${BUILD_INSTALL_PREFIX}/include/"
    fi
}

###############################################################################
# Function to build AIDL libraries
###############################################################################
build_aidl_libs()
{
    make -j`nproc` -C "${AIDL_LIB_DIR}" TARGET=$TARGET VARIANT=CPP \
        HALIF_AIDL_SRC_DIR="${HALIF_AIDL_SRC_DIR}" \
        HALIF_AIDL_INC_DIR="${HALIF_AIDL_INC_DIR}" \
        BINDER_SDK_INCLUDE_DIR="${LOCAL_BINDER_INCLUDE_DIR}" \
        BINDER_SDK_LIB_DIR="${LOCAL_BINDER_LIB_DIR}"
    if [ $? -ne 0 ]; then
        echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
        echo -e "${BOLD}* ${RED}Failed to build the AIDL Lib. Exiting.${RESET}"
        echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
        exit 1
    fi
    echo "AIDL Build completed successfully."
}

###############################################################################
# Function to clone and build the ut-core repository
###############################################################################
build_ut_core()
{
    if [ ! -d ${UT_CORE_DIR} ]; then
        git clone https://github.com/rdkcentral/ut-core.git ${UT_CORE_DIR}
        if [ $? -ne 0 ]; then
            echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
            echo -e "${BOLD}* ${RED}Failed to clone ut-core.${RESET}"
            echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
            exit 1
        fi
    fi

    pushd ${UT_CORE_DIR}

    git fetch -q -t origin
    # get the latest version of ut-core
    if [ -z "$UT_CORE_VERSION" ]; then
        UT_CORE_VERSION=`git tag | sort -Vr | head -n1`
    fi

    git checkout $UT_CORE_VERSION

    ./build.sh TARGET=$TARGET VARIANT=CPP
    if [ $? -ne 0 ]; then
        echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
        echo -e "${BOLD}* ${RED}Failed to build the UT Core. Exiting.${RESET}        ${BOLD}*${RESET}"
        echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
        exit 1
    fi

    popd

}

###############################################################################
# Function to Build HALIF test
###############################################################################
build_halif_test()
{
    # Build the tests
    make -j`nproc` -C ${TOP}
    if [ $? -ne 0 ]; then
        echo -e "${BOLD}╔═════════════════════════════════════════════════╗${RESET}"
        echo -e "${BOLD}* ${RED}Failed to build the main tests. Exiting.${RESET}"
        echo -e "${BOLD}╚═════════════════════════════════════════════════╝${RESET}"
        exit 1
    fi
}

###############################################################################
# Function to install test binaries and scripts to the build directory
###############################################################################
install_test()
{
    declare -r INSTALL_PATH=${TOP_BUILD_DIR}/bin

    install -t ${INSTALL_PATH} ${TOP_BUILD_DIR}/usr/bin/*
    install -t ${INSTALL_PATH} ${TOP_BUILD_DIR}/usr/lib/*
    install -t ${INSTALL_PATH} run.sh

    # Remove bundled binder runtime libs — the target's system libbinder.so is
    # compatible with its kernel binder driver. Bundling our compiled version
    # causes a kernel/userspace protocol mismatch at runtime.
    rm -f "${INSTALL_PATH}"/libbinder.so* \
          "${INSTALL_PATH}"/libutils.so* \
          "${INSTALL_PATH}"/libbase.so* \
          "${INSTALL_PATH}"/libcutils.so* \
          "${INSTALL_PATH}"/libcutils_sockets.so* \
          "${INSTALL_PATH}"/liblog.so* \
          "${INSTALL_PATH}"/servicemanager

    cp -r ${PROFILES_DIR} ${INSTALL_PATH}
    cp -r "${TOP}/test_configuration" ${INSTALL_PATH}

    cd ${INSTALL_PATH}
    tar -czf ${OUT_TAR_FILE} *
    cd -
}

###############################################################################
# Function to clean the test build directory
###############################################################################
clean_test()
{
    echo -e "${BOLD}removing *.o${RESET}"

    make clean
    make -C ${AIDL_LIB_DIR} clean

    rm -rf ${TOP_BUILD_DIR}/bin/${OUT_TAR_FILE}
}

###############################################################################
# Function to clean the test build directory
###############################################################################
dist_clean_test()
{
    clean_test
    rm -rf $TOP_BUILD_DIR
    rm -rf $RDK_HAL_DIR
    rm -rf ${UT_CORE_DIR}
}

###############################################################################
# help function to display usage information
###############################################################################
help()
{
    echo -e "${BOLD}${CYAN}Usage:${RESET}"

    echo -e "${BOLD}╔════════════════════════════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 TARGET=<target>${RESET} ${YELLOW}- Builds the test for specified target (arm, linux)${RESET} ${BOLD}*${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 clean${RESET}           ${YELLOW}- Cleans the build folder${RESET}                           ${BOLD}*${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 dist_clean${RESET}      ${YELLOW}- Deletes all build and checkout folders${RESET}            ${BOLD}*${RESET}"
    echo -e "${BOLD}* ${GREEN}$0 help/-h/--help${RESET}  ${YELLOW}- Prints this message${RESET}                               ${BOLD}*${RESET}"
    echo -e "${BOLD}╚════════════════════════════════════════════════════════════════════════════════╝${RESET}"
}


echo -e "${BOLD}${GREEN}VTS Version                  : ${RESET}${YELLOW}$(git rev-parse --abbrev-ref HEAD)${RESET}"
echo -e "${BOLD}${GREEN}UT_CORE_VERSION              : ${RESET}${YELLOW}$UT_CORE_VERSION${RESET}"
echo -e "${BOLD}${GREEN}RDK_HALIF_AIDL_VERSION       : ${RESET}${YELLOW}$RDK_HALIF_AIDL_VERSION${RESET}"
echo -e "${BOLD}${GREEN}LOCAL_BINDER_SDK_DIR         : ${RESET}${YELLOW}$LOCAL_BINDER_SDK_DIR${RESET}"

###############################################################################
# Main script execution starts here
###############################################################################
if [ "$1" == "clean" ]; then
    clean_test
elif [ "$1" == "dist_clean" ]; then
    dist_clean_test
elif [ "$1" == "help" ]; then
    help
elif [[ "$1" == [Tt]arget=* ]]; then
    verify_local_binder_sdk
    install_linux_binder
    resolve_halif_versions
    install_halif_headers
    build_aidl_libs
    build_ut_core
    build_halif_test
    install_test
    echo -e "${BOLD}╔════════════════════════════════╗${RESET}"
    echo -e "${BOLD}* ${GREEN}Build Successful${RESET}               ${BOLD}*${RESET}"
    echo -e "${BOLD}╚════════════════════════════════╝${RESET}"
else
    echo -e "${BOLD}${RED}Invalid Arguments${RESET}"
    help
fi
