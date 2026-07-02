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

refresh_local_binder_paths()
{
    LOCAL_BINDER_SDK_DIR=${BINDER_SDK_DIR:-${RDK_HAL_DIR}/out/target}
    LOCAL_BINDER_INCLUDE_DIR=${BINDER_SDK_INCLUDE_DIR:-${RDK_HAL_DIR}/out/build/include/binder_sdk}
    LOCAL_BINDER_LIB_DIR=${BINDER_SDK_LIB_DIR:-${LOCAL_BINDER_SDK_DIR}/lib/binder}
}

refresh_local_binder_paths

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
# Clone rdk-halif-aidl if not present, or switch to the required branch.
###############################################################################
checkout_halif_repo()
{
    if [ ! -d "$RDK_HAL_DIR" ]; then
        echo "Cloning rdk-halif-aidl (branch: $RDK_HALIF_AIDL_VERSION)..."
        git clone -b "$RDK_HALIF_AIDL_VERSION" https://github.com/rdkcentral/rdk-halif-aidl.git "$RDK_HAL_DIR" || {
            echo "ERROR: Failed to clone rdk-halif-aidl."
            exit 1
        }
        return
    fi

    local current_branch
    current_branch=$(git -C "$RDK_HAL_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null)
    if [ "${current_branch}" = "${RDK_HALIF_AIDL_VERSION}" ]; then
        return
    fi

    echo "rdk-halif-aidl is on '${current_branch}', switching to '${RDK_HALIF_AIDL_VERSION}'..."
    git -C "$RDK_HAL_DIR" fetch origin "$RDK_HALIF_AIDL_VERSION" || {
        echo "ERROR: Failed to fetch branch ${RDK_HALIF_AIDL_VERSION}."
        exit 1
    }
    git -C "$RDK_HAL_DIR" checkout "$RDK_HALIF_AIDL_VERSION" || {
        echo "ERROR: Failed to checkout branch ${RDK_HALIF_AIDL_VERSION}."
        exit 1
    }
}

###############################################################################
# Cross-compile ARM binder runtime (libbinder.so, libutils.so, etc.) from
# the linux_binder_idl sources using the ARM cross-compiler.
###############################################################################
build_arm_binder()
{
    local BINDER_REPO_DIR="${RDK_HAL_DIR}/build-tools/linux_binder_idl"
    local ARM_BINDER_BUILD="${RDK_HAL_DIR}/build/binder-arm"
    local ARM_BINDER_SDK="${RDK_HAL_DIR}/out/target"

    if [ -f "${ARM_BINDER_SDK}/.sdk_ready" ]; then
        echo "ARM Binder SDK already built at ${ARM_BINDER_SDK}"
        return 0
    fi

    if [ ! -d "${BINDER_REPO_DIR}/.git" ]; then
        echo "Cloning linux_binder_idl for ARM build..."
        local _binder_version
        _binder_version=$(grep -vE '^[[:space:]]*(#|$)' \
            "${RDK_HAL_DIR}/binder_sdk.version" 2>/dev/null \
            | head -1 | tr -d '[:space:]')
        _binder_version="${_binder_version:-2.0.0}"

        [ -d "${BINDER_REPO_DIR}" ] && rm -rf "${BINDER_REPO_DIR}"
        mkdir -p "${RDK_HAL_DIR}/build-tools"
        git clone "https://github.com/rdkcentral/linux_binder_idl" \
            "${BINDER_REPO_DIR}" || {
            echo "ERROR: Failed to clone linux_binder_idl."
            exit 1
        }
        ( cd "${BINDER_REPO_DIR}" && \
          git checkout "${_binder_version}" 2>/dev/null || \
          echo "Note: version ${_binder_version} not found, using default branch" )

        ( cd "${BINDER_REPO_DIR}" && bash ./clone-android-binder-repo.sh ) || {
            echo "ERROR: Failed to clone Android binder sources."
            exit 1
        }
    fi

    local cc_bin cxx_bin cc_flags
    cc_bin=$(echo "${CC}" | awk '{print $1}')
    cxx_bin=$(echo "${CXX}" | awk '{print $1}')
    cc_flags=$(echo "${CC}" | cut -d' ' -f2-)

    mkdir -p "${ARM_BINDER_BUILD}"
    local TOOLCHAIN_FILE="${ARM_BINDER_BUILD}/arm-cross.cmake"
    cat > "${TOOLCHAIN_FILE}" <<TOOLCHAIN_EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER   "${cc_bin}")
set(CMAKE_CXX_COMPILER "${cxx_bin}")
set(CMAKE_C_FLAGS_INIT   "${cc_flags} ${CFLAGS:-}")
set(CMAKE_CXX_FLAGS_INIT "${cc_flags} ${CXXFLAGS:-}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${LDFLAGS:-}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${LDFLAGS:-}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
TOOLCHAIN_EOF

    local _OECORE_NATIVE="${OECORE_NATIVE_SYSROOT:-}"
    local _OECORE_TARGET="${OECORE_TARGET_SYSROOT:-}"
    unset OECORE_NATIVE_SYSROOT OECORE_TARGET_SYSROOT

    echo "Cross-compiling ARM binder runtime (BUILD_HOST_AIDL=OFF)..."
    echo "  Compiler: ${cc_bin}"
    echo "  Flags:    ${cc_flags} ${CFLAGS:-}"
    cmake -S "${BINDER_REPO_DIR}" \
          -B "${ARM_BINDER_BUILD}" \
          -DCMAKE_INSTALL_PREFIX="${ARM_BINDER_SDK}" \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_HOST_AIDL=OFF \
          -DBUILD_ENV_YOCTO=OFF \
          -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
    local cmake_cfg_rc=$?

    [ -n "${_OECORE_NATIVE}" ] && export OECORE_NATIVE_SYSROOT="${_OECORE_NATIVE}"
    [ -n "${_OECORE_TARGET}" ] && export OECORE_TARGET_SYSROOT="${_OECORE_TARGET}"

    if [ $cmake_cfg_rc -ne 0 ]; then
        echo "ERROR: ARM binder cmake configuration failed."
        exit 1
    fi
    cmake --build "${ARM_BINDER_BUILD}" -j"$(nproc)" || {
        echo "ERROR: ARM binder build failed."
        exit 1
    }
    cmake --install "${ARM_BINDER_BUILD}" || {
        echo "ERROR: ARM binder install failed."
        exit 1
    }

    local ARM_BINDER_INCLUDE="${RDK_HAL_DIR}/out/build"
    if [ -d "${ARM_BINDER_SDK}/lib" ]; then
        rm -rf "${ARM_BINDER_SDK}/lib/binder"
        mkdir -p "${ARM_BINDER_SDK}/lib/binder"
        mv "${ARM_BINDER_SDK}"/lib/*.so* "${ARM_BINDER_SDK}/lib/binder/" 2>/dev/null || true
    fi
    if [ -d "${ARM_BINDER_SDK}/include" ]; then
        mkdir -p "${ARM_BINDER_INCLUDE}/include/binder_sdk"
        find "${ARM_BINDER_SDK}/include" -mindepth 1 -maxdepth 1 -type d \
            -exec mv {} "${ARM_BINDER_INCLUDE}/include/binder_sdk/" \; 2>/dev/null || true
        find "${ARM_BINDER_SDK}/include" -maxdepth 1 -type f -name "*.h" \
            -exec mv {} "${ARM_BINDER_INCLUDE}/include/binder_sdk/" \; 2>/dev/null || true
        rmdir "${ARM_BINDER_SDK}/include" 2>/dev/null || true
    fi
    mkdir -p "${ARM_BINDER_SDK}/lib/halif"
    touch "${ARM_BINDER_SDK}/.sdk_ready"
    touch "${ARM_BINDER_INCLUDE}/.sdk_ready"
    echo "ARM Binder SDK built and installed to ${ARM_BINDER_SDK}"
}

###############################################################################
# Set up binder SDK from rdk-halif-aidl for linux/arm builds.
###############################################################################
setup_binder_sdk()
{
    local binder_toolchain_dir="${RDK_HAL_DIR}/build-tools/linux_binder_idl"

    checkout_halif_repo

    if [ "${TARGET}" = "arm" ]; then
        if [ -n "${BINDER_SDK_DIR:-}" ]; then
            refresh_local_binder_paths
            return
        fi

        if [ -n "${OECORE_TARGET_SYSROOT:-}" ] && [ -d "${OECORE_TARGET_SYSROOT}/usr/lib/binder" ]; then
            export BINDER_SDK_DIR="${OECORE_TARGET_SYSROOT}/usr"
            echo "ARM build: using OE sysroot binder SDK at ${BINDER_SDK_DIR}"
            refresh_local_binder_paths
            return
        fi

        echo "ARM build: building AIDL host tools with x86 compiler..."
        local _CC="${CC:-}" _CXX="${CXX:-}" _CFLAGS="${CFLAGS:-}"
        local _CXXFLAGS="${CXXFLAGS:-}" _LDFLAGS="${LDFLAGS:-}"
        local _ARCH="${ARCH:-}" _CMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-}"
        local _OECORE_NATIVE="${OECORE_NATIVE_SYSROOT:-}"
        local _OECORE_TARGET="${OECORE_TARGET_SYSROOT:-}"
        unset CC CXX CFLAGS CXXFLAGS LDFLAGS ARCH CMAKE_TOOLCHAIN_FILE
        unset OECORE_NATIVE_SYSROOT OECORE_TARGET_SYSROOT
        export M4=/usr/bin/m4
        (
            cd "${RDK_HAL_DIR}"
            ./build_binder.sh
        ) || {
            echo "ERROR: build_binder.sh (host tools) failed."
            exit 1
        }
        [ -n "${_CC}" ]                   && export CC="${_CC}"
        [ -n "${_CXX}" ]                  && export CXX="${_CXX}"
        [ -n "${_CFLAGS}" ]               && export CFLAGS="${_CFLAGS}"
        [ -n "${_CXXFLAGS}" ]             && export CXXFLAGS="${_CXXFLAGS}"
        [ -n "${_LDFLAGS}" ]              && export LDFLAGS="${_LDFLAGS}"
        [ -n "${_ARCH}" ]                 && export ARCH="${_ARCH}"
        [ -n "${_CMAKE_TOOLCHAIN_FILE}" ] && export CMAKE_TOOLCHAIN_FILE="${_CMAKE_TOOLCHAIN_FILE}"
        [ -n "${_OECORE_NATIVE}" ]        && export OECORE_NATIVE_SYSROOT="${_OECORE_NATIVE}"
        [ -n "${_OECORE_TARGET}" ]        && export OECORE_TARGET_SYSROOT="${_OECORE_TARGET}"

        rm -f "${RDK_HAL_DIR}/out/target/.sdk_ready"
        echo "ARM build: cross-compiling ARM binder runtime..."
        build_arm_binder
        export BINDER_SDK_DIR="${RDK_HAL_DIR}/out/target"
    else
        if [ -d "${binder_toolchain_dir}" ] && [ ! -d "${binder_toolchain_dir}/.git" ]; then
            echo "Removing incomplete toolchain stub before fresh clone..."
            rm -rf "${binder_toolchain_dir}"
        fi
        (
            cd "${RDK_HAL_DIR}"
            ./build_binder.sh
        ) || {
            echo "ERROR: build_binder.sh failed. Cannot continue."
            exit 1
        }
    fi

    refresh_local_binder_paths

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

    # Bundle libstdc++ for linux builds so the binary runs on targets with older system libs
    # (avoids GLIBCXX_x.x.xx not found errors at runtime). Do not bundle for cross builds.
    if [ "${TARGET}" = "linux" ]; then
        local cxx_bin=${CXX:-g++}
        local libstdcpp
        libstdcpp=$(realpath "$(${cxx_bin} -print-file-name=libstdc++.so.6)" 2>/dev/null)
        if [ -f "${libstdcpp}" ]; then
            install -t ${BUILD_INSTALL_PREFIX}/lib "${libstdcpp}"
            ln -sf "$(basename "${libstdcpp}")" "${BUILD_INSTALL_PREFIX}/lib/libstdc++.so.6"
            echo "Bundled libstdc++: ${libstdcpp}"
        fi
    fi
}

###############################################################################
# Resolve and install HALIF interfaces for non-manifest single-component builds
###############################################################################
generate_hal_interfaces()
{
    # Iterate through the HALIF components
    for component in "${!HALIF_COMPONENTS[@]}"; do
        version=${HALIF_COMPONENTS[$component]}
        echo "Component: $component, Version: $version"

        HAL_TOP_BUILD_DIR=${TOP_BUILD_DIR}/${component}_aidl

        cmake \
        -DINTERFACE_TARGET=$component \
        -DAIDL_SRC_VERSION=$version \
        -B ${HAL_TOP_BUILD_DIR} \
        $RDK_HAL_DIR
        if [ $? -ne 0 ]; then
            echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
            echo -e "${BOLD}* ${RED}Failed to Configure AIDL files. Exiting.${RESET}"
            echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
            exit 1
        fi

        cmake --build ${HAL_TOP_BUILD_DIR} -j$(nproc)
        if [ $? -ne 0 ]; then
            echo -e "${BOLD}╔══════════════════════════════════════════════╗${RESET}"
            echo -e "${BOLD}* ${RED}Failed to Generate AIDL headers. Exiting.${RESET}"
            echo -e "${BOLD}╚══════════════════════════════════════════════╝${RESET}"
            exit 1
        fi

        # Set src/include paths for build_aidl_libs
        HALIF_AIDL_SRC_DIR=$(realpath "${RDK_HAL_DIR}/${component}/${version}/src")
        HALIF_AIDL_INC_DIR=$(realpath "${RDK_HAL_DIR}/${component}/${version}/include")

        # Copy generated headers to build/usr/include so the test build can find them
        local gen_inc_dir="${RDK_HAL_DIR}/out/build/include/${component}/${version}/include"
        if [ -d "${gen_inc_dir}" ]; then
            install -d "${BUILD_INSTALL_PREFIX}/include"
            cp -apr "${gen_inc_dir}/." "${BUILD_INSTALL_PREFIX}/include/"
        else
            echo -e "${BOLD}${RED}ERROR: Generated include dir not found: ${gen_inc_dir}${RESET}"
            exit 1
        fi
    done
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
    rm -f  "${TOP}/build_binder.sh"
    rm -f  "${TOP}/binder_sdk.version"
    rm -rf "${TOP}/build-tools"
    rm -rf "${TOP}/out"
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
    setup_binder_sdk
    install_linux_binder
    generate_hal_interfaces
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
