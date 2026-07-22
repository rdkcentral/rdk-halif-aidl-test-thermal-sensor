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

declare -r TOP="$(dirname "$(realpath "$0")")"
RDK_HALIF_AIDL_VERSION="${RDK_HALIF_AIDL_VERSION="main"}"

declare -A HALIF_COMPONENTS=(
  [sensor]="current"
)

# Define important workspace paths.
declare -r UT_CORE_DIR="${TOP}/ut-core"
declare -r TOP_BUILD_DIR="${TOP}/build"
declare -r BUILD_INSTALL_PREFIX=${TOP_BUILD_DIR}/usr
declare -r AIDL_LIB_DIR=${TOP}/aidl_lib
declare -r RDK_HAL_DIR=${TOP}/rdk-halif-aidl
declare -r PROFILES_DIR=${TOP}/profiles

OUT_TAR_FILE=vts-thermalsensor.tar.gz

LOCAL_BINDER_TOOLCHAIN_DIR=${BINDER_SOURCE_DIR:-${RDK_HAL_DIR}/build-tools/linux_binder_idl}
LOCAL_BINDER_SDK_DIR=${BINDER_SDK_DIR:-${RDK_HAL_DIR}/out/target}
LOCAL_BINDER_INCLUDE_DIR=${BINDER_SDK_INCLUDE_DIR:-${RDK_HAL_DIR}/out/build/include/binder_sdk}
LOCAL_BINDER_LIB_DIR=${BINDER_SDK_LIB_DIR:-${LOCAL_BINDER_SDK_DIR}/lib/binder}
BINDER_SDK_VERSION_FILE=${BINDER_SDK_VERSION_FILE:-${TOP}/binder_sdk.version}

# Default to 'linux' if not provided
TARGET="linux"

###############################################################################
# Cross-compile ARM binder runtime from linux_binder_idl (BUILD_HOST_AIDL=OFF,
# runtime libs only). Used when no pre-built BINDER_SDK_DIR is provided.
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

    # Clone linux_binder_idl directly to avoid OE PATH issues with build_binder.sh.
    if [ ! -d "${BINDER_REPO_DIR}/.git" ]; then
        echo "Cloning linux_binder_idl for ARM build..."
        local _binder_version
        _binder_version=$(grep -vE '^[[:space:]]*(#|$)' \
            "${BINDER_SDK_VERSION_FILE}" 2>/dev/null \
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

        # Clone Android binder sources required by the cmake build
        ( cd "${BINDER_REPO_DIR}" && bash ./clone-android-binder-repo.sh ) || {
            echo "ERROR: Failed to clone Android binder sources."
            exit 1
        }
    fi

    # Write a cmake toolchain file to split CC/CXX binary from flags and
    # override OEToolchainConfig.cmake auto-loading (cmake 3.22+).
    if [ -z "${CC:-}" ] || [ -z "${CXX:-}" ]; then
        echo -e "${BOLD}${RED}ERROR: CC/CXX not set for ARM binder build. Source the cross-toolchain env or set BINDER_SDK_DIR to a prebuilt Binder SDK.${RESET}"
        exit 1
    fi
    local cc_bin cxx_bin cc_flags cxx_flags
    cc_bin=$(echo "${CC}" | awk '{print $1}')
    cxx_bin=$(echo "${CXX}" | awk '{print $1}')
    cc_flags=$(echo "${CC}" | cut -d' ' -f2-)
    cxx_flags=$(echo "${CXX}" | cut -d' ' -f2-)

    mkdir -p "${ARM_BINDER_BUILD}"
    local TOOLCHAIN_FILE="${ARM_BINDER_BUILD}/arm-cross.cmake"
    cat > "${TOOLCHAIN_FILE}" <<TOOLCHAIN_EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER   "${cc_bin}")
set(CMAKE_CXX_COMPILER "${cxx_bin}")
set(CMAKE_C_FLAGS_INIT   "${cc_flags} ${CFLAGS:-}")
set(CMAKE_CXX_FLAGS_INIT "${cxx_flags} ${CXXFLAGS:-}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${LDFLAGS:-}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${LDFLAGS:-}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
TOOLCHAIN_EOF

    # Clear OECORE_* vars temporarily so BUILD_ENV_YOCTO stays OFF during cmake configure.
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

    # Restore OE env vars before any further steps
    [ -n "${_OECORE_NATIVE}" ] && export OECORE_NATIVE_SYSROOT="${_OECORE_NATIVE}"
    [ -n "${_OECORE_TARGET}" ] && export OECORE_TARGET_SYSROOT="${_OECORE_TARGET}"

    if [ $cmake_cfg_rc -ne 0 ]; then
        echo -e "${BOLD}${RED}ERROR: ARM binder cmake configuration failed.${RESET}"
        exit 1
    fi
    cmake --build "${ARM_BINDER_BUILD}" -- -j"$(nproc)" || {
        echo -e "${BOLD}${RED}ERROR: ARM binder build failed.${RESET}"
        exit 1
    }
    cmake --install "${ARM_BINDER_BUILD}" || {
        echo -e "${BOLD}${RED}ERROR: ARM binder install failed.${RESET}"
        exit 1
    }

    # Reorganise install tree to match expected SDK layout (lib/binder/, include/binder_sdk/).
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
    mkdir -p "${ARM_BINDER_SDK}/lib/rdk-halif-aidl"
    touch "${ARM_BINDER_SDK}/.sdk_ready"
    touch "${ARM_BINDER_INCLUDE}/.sdk_ready"
    echo "ARM Binder SDK built and installed to ${ARM_BINDER_SDK}"
}

###############################################################################
# Create unversioned softlinks (lib<module>-cpp_vts.so -> lib<module>-v<ver>-cpp.so)
# from versions_released.yaml and write halif_generated.mk.
###############################################################################
create_halif_softlinks()
{
    local YAML="${RDK_HAL_DIR}/versions_released.yaml"
    local LIB_DIR="${RDK_HAL_DIR}/out/target/lib/rdk-halif-aidl"
    local in_components=0

    if [ ! -f "$YAML" ]; then
        echo "Warning: versions_released.yaml not found, skipping softlink creation"
        return
    fi

    echo "Creating halif library softlinks..."
    while IFS= read -r line; do
        if [[ "$line" =~ ^components: ]]; then
            in_components=1
            continue
        fi
        if [[ $in_components -eq 1 ]]; then
            if [[ "$line" =~ ^[[:space:]]+([A-Za-z0-9_]+):[[:space:]]*([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+) ]]; then
                local module="${BASH_REMATCH[1]}"
                local version="${BASH_REMATCH[2]}"
                local versioned="lib${module}-v${version}-cpp.so"
                local link="lib${module}-cpp_vts.so"
                if [ -f "${LIB_DIR}/${versioned}" ]; then
                    ln -sf "${versioned}" "${LIB_DIR}/${link}"
                    echo "  ${link} -> ${versioned}"
                fi
            elif [[ "$line" =~ ^[^[:space:]] && ! "$line" =~ ^# ]]; then
                in_components=0
            fi
        fi
    done < "$YAML"

    local MK_FILE="${RDK_HAL_DIR}/out/halif_generated.mk"
    local mk_inc_dirs mk_module_libs
    mk_inc_dirs=$(find "${RDK_HAL_DIR}/out/build/include" -mindepth 3 -maxdepth 3 \
        -type d -name "include" 2>/dev/null | tr '\n' ' ' | sed 's/ $//')
    mk_module_libs=$(find "${LIB_DIR}" -maxdepth 1 -type l -name 'lib*-cpp_vts.so' \
        2>/dev/null | sed 's|.*/lib\(.*\)\.so|-l\1|' | tr '\n' ' ' | sed 's/ $//')
    {
        echo "# Auto-generated by build.sh create_halif_softlinks() - do not edit"
        echo "HALIF_INC_DIRS    := ${mk_inc_dirs}"
        echo "HALIF_MODULE_LIBS := ${mk_module_libs}"
    } > "${MK_FILE}"
    echo "  Generated ${MK_FILE}"
}

###############################################################################
# Clone rdk-halif-aidl if not present, or switch to the required branch.
# Invalidates the halif modules sentinel when the branch changes so the next
# build_halif_modules() call does a full rebuild.
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
        echo "rdk-halif-aidl already on '${RDK_HALIF_AIDL_VERSION}'"
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
    rm -f "${RDK_HAL_DIR}/out/target/lib/rdk-halif-aidl/.halif_modules_ready"
}

###############################################################################
# Set up the Binder SDK required for compilation.
#
# Linux: runs rdk-halif-aidl/build_binder.sh (no-op if .sdk_ready exists).
# ARM:   checks in order —
#   1. BINDER_SDK_DIR env var (user-supplied pre-built ARM binder SDK).
#   2. OE sysroot binder at ${OECORE_TARGET_SYSROOT}/usr/lib/binder.
#   3. Two-step cross-compile:
#        Step 1 — build host AIDL tools with the x86 compiler (unsets OE env
#                 so linux_binder_idl does not skip its install() rules).
#        Step 2 — cross-compile the ARM binder runtime via build_arm_binder().
###############################################################################
setup_binder_sdk()
{
    local binder_toolchain_dir="${LOCAL_BINDER_TOOLCHAIN_DIR}"
    local sdk_ready="${LOCAL_BINDER_SDK_DIR}/.sdk_ready"
    if [ "${TARGET}" = "arm" ]; then
        if [ -n "${BINDER_SDK_DIR:-}" ]; then
            echo "ARM build: using pre-built Binder SDK at ${BINDER_SDK_DIR}"
            LOCAL_BINDER_SDK_DIR="${BINDER_SDK_DIR}"
            LOCAL_BINDER_LIB_DIR="${BINDER_SDK_DIR}/lib/binder"
            return
        fi

        if [ -n "${OECORE_TARGET_SYSROOT:-}" ] && [ -d "${OECORE_TARGET_SYSROOT}/usr/lib/binder" ]; then
            export BINDER_SDK_DIR="${OECORE_TARGET_SYSROOT}/usr"
            LOCAL_BINDER_SDK_DIR="${BINDER_SDK_DIR}"
            LOCAL_BINDER_LIB_DIR="${BINDER_SDK_DIR}/lib/binder"
            echo "ARM build: using OE sysroot binder SDK at ${BINDER_SDK_DIR}"
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
        # Restore ARM cross-compile environment.
        [ -n "${_CC}" ]                   && export CC="${_CC}"
        [ -n "${_CXX}" ]                  && export CXX="${_CXX}"
        [ -n "${_CFLAGS}" ]               && export CFLAGS="${_CFLAGS}"
        [ -n "${_CXXFLAGS}" ]             && export CXXFLAGS="${_CXXFLAGS}"
        [ -n "${_LDFLAGS}" ]              && export LDFLAGS="${_LDFLAGS}"
        [ -n "${_ARCH}" ]                 && export ARCH="${_ARCH}"
        [ -n "${_CMAKE_TOOLCHAIN_FILE}" ] && export CMAKE_TOOLCHAIN_FILE="${_CMAKE_TOOLCHAIN_FILE}"
        [ -n "${_OECORE_NATIVE}" ]        && export OECORE_NATIVE_SYSROOT="${_OECORE_NATIVE}"
        [ -n "${_OECORE_TARGET}" ]        && export OECORE_TARGET_SYSROOT="${_OECORE_TARGET}"

        rm -f "${LOCAL_BINDER_SDK_DIR}/.sdk_ready"
        echo "ARM build: cross-compiling ARM binder runtime..."
        build_arm_binder
        LOCAL_BINDER_LIB_DIR="${LOCAL_BINDER_SDK_DIR}/lib/binder"
        export BINDER_SDK_DIR="${LOCAL_BINDER_SDK_DIR}"
    else
        if [ -d "${binder_toolchain_dir}" ] && [ ! -d "${binder_toolchain_dir}/.git" ]; then
            echo "Removing incomplete toolchain stub before fresh clone..."
            rm -rf "${binder_toolchain_dir}"
        fi
        if [ ! -f "${sdk_ready}" ]; then
            echo -e "${BOLD}${YELLOW}Local Binder SDK not staged. Bootstrapping via rdk-halif-aidl/build_binder.sh${RESET}"
            # linux_binder_idl defaults to TARGET_LIB32_VERSION=ON (protocol 7 / BINDER_IPC_32BIT).
            # For a 64-bit Linux host/vdevice we need protocol 8.  Patch build_binder.sh to
            # pass -DTARGET_LIB64_VERSION=ON so cmake selects the 64-bit path.
            if ! grep -q 'TARGET_LIB64_VERSION=ON' "${RDK_HAL_DIR}/build_binder.sh"; then
                sed -i 's/-DBUILD_HOST_AIDL=ON/-DBUILD_HOST_AIDL=ON -DTARGET_LIB64_VERSION=ON/g' \
                    "${RDK_HAL_DIR}/build_binder.sh"
            fi
            BINDER_SOURCE_DIR="${binder_toolchain_dir}" "${RDK_HAL_DIR}/build_binder.sh" || {
                echo -e "${BOLD}${RED}ERROR: build_binder.sh failed. Cannot continue.${RESET}"
                exit 1
            }
        fi
        if [ ! -d "${LOCAL_BINDER_INCLUDE_DIR}" ]; then
            echo -e "${BOLD}${RED}ERROR: Local Binder headers not found: ${LOCAL_BINDER_INCLUDE_DIR}${RESET}"
            exit 1
        fi
        if [ ! -d "${LOCAL_BINDER_LIB_DIR}" ]; then
            echo -e "${BOLD}${RED}ERROR: Local Binder libraries not found: ${LOCAL_BINDER_LIB_DIR}${RESET}"
            exit 1
        fi
    fi
}

###############################################################################
# Build the sensor HAL module from the released snapshot pinned in
# versions_released.yaml, then create the unversioned softlink.
# Skips the build entirely if the sentinel .halif_modules_ready already exists.
###############################################################################
build_halif_modules()
{
    local HALIF_LIB_DIR="${RDK_HAL_DIR}/out/target/lib/rdk-halif-aidl"
    local HALIF_READY="${HALIF_LIB_DIR}/.halif_modules_ready"
    local manifest_file="${RDK_HAL_DIR}/versions_released.yaml"

    if [ -f "${HALIF_READY}" ]; then
        echo "HAL module .so files already present, skipping build_modules.sh..."
        return
    fi

    local component
    for component in "${!HALIF_COMPONENTS[@]}"; do break; done

    echo "Building ${component} HAL module from versions_released.yaml..."
    local default_version
    default_version=$(grep -m1 '^default:' "$manifest_file" | awk '{print $2}')
    default_version="${default_version:-current}"

    local component_version
    component_version=$(grep -m1 "^[[:space:]]*${component}:" "$manifest_file" | awk '{print $2}')
    component_version="${component_version:-$default_version}"

    local sdk_dir_arg=""
    if [ "${TARGET}" = "arm" ]; then
        export BINDER_ALLOW_CROSS_ENV=1
        [ -n "${BINDER_SDK_DIR:-}" ] && sdk_dir_arg="--sdk-dir ${BINDER_SDK_DIR}"
    fi
    (
        cd "${RDK_HAL_DIR}"
        ./build_modules.sh "${component}" --version "${component_version}" ${sdk_dir_arg}
    ) || {
        echo -e "${BOLD}╔══════════════════════════════════════════════════════════════════════╗${RESET}"
        echo -e "${BOLD}* ${RED}Failed to build ${component} HAL module. Exiting.${RESET}"
        echo -e "${BOLD}╚══════════════════════════════════════════════════════════════════════╝${RESET}"
        exit 1
    }
    touch "${HALIF_READY}"
    create_halif_softlinks
}

###############################################################################
# Prepare and stage HAL artifacts required by the local test build.
# This includes repo checkout, Binder SDK setup, HAL module generation,
# version resolution, and staging of Binder and HALIF headers/libraries.
###############################################################################
stage_hal_artifacts()
{
    checkout_halif_repo
    setup_binder_sdk
    build_halif_modules
    resolve_halif_versions
    install_linux_binder
    install_halif_headers
}

###############################################################################
# Install Binder service manager, headers and libraries into build/usr/ from
# the rdk-halif-aidl Binder SDK staged by setup_binder_sdk().
# Must be called after setup_binder_sdk().
###############################################################################
install_linux_binder()
{
    local BINDER_SDK="${LOCAL_BINDER_SDK_DIR}"
    local BINDER_INC="${RDK_HAL_DIR}/out/build"
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

    if [ -d "${BINDER_INC}/include/binder_sdk" ]; then
        cp -apr ${BINDER_INC}/include/binder_sdk/. ${BUILD_INSTALL_PREFIX}/include/
    fi
    if [ "${TARGET}" = "linux" ]; then
        local libstdcpp
        libstdcpp=$(realpath "$(g++ -print-file-name=libstdc++.so.6)" 2>/dev/null)
        if [ -f "${libstdcpp}" ]; then
            install -t ${BUILD_INSTALL_PREFIX}/lib "${libstdcpp}"
            local libstdcpp_basename
            libstdcpp_basename=$(basename "${libstdcpp}")
            ln -sf "${libstdcpp_basename}" "${BUILD_INSTALL_PREFIX}/lib/libstdc++.so.6"
            echo "Bundled host libstdc++: ${libstdcpp}"
        fi
    fi
}

###############################################################################
# Install versioned HALIF headers into build/usr/include/ so the Makefile can
# reference them via the staged build/usr/include path.
# Must be called after resolve_halif_versions().
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
# Resolve versioned src/include dirs from rdk-halif-aidl/versions_released.yaml
# with a per-component override and default fallback.
###############################################################################
resolve_halif_versions()
{
    local versions_file="${RDK_HAL_DIR}/versions_released.yaml"

    if [ ! -f "$versions_file" ]; then
        echo -e "${BOLD}${RED}ERROR: versions_released.yaml not found: ${versions_file}${RESET}"
        exit 1
    fi

    local default_version
    default_version=$(grep -m1 '^default:' "$versions_file" | awk '{print $2}')
    if [ -z "$default_version" ]; then
        default_version="current"
    fi

    local component
    for component in "${!HALIF_COMPONENTS[@]}"; do break; done
    local version
    version=$(grep -m1 "^[[:space:]]*${component}:" "$versions_file" | awk '{print $2}')
    if [ -z "$version" ]; then
        echo -e "${BOLD}${YELLOW}WARNING: ${component} not listed in versions_released.yaml, using default: ${default_version}${RESET}"
        version="$default_version"
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

    echo -e "${BOLD}${GREEN}${component}${RESET}: version=${YELLOW}${version}${RESET} ${CYAN}(versions_released.yaml)${RESET}"
    HALIF_AIDL_SRC_DIR=$(realpath "$src_dir")
    HALIF_AIDL_INC_DIR=$(realpath "$inc_dir")
}

###############################################################################
# Clone ut-core if needed, select the requested version, and build it.
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
# Build the local HALIF test binaries for the selected target.
###############################################################################
build_halif_test()
{
    # Remove stale .o files so sources are always recompiled for the current TARGET.
    find "${TOP}/src" "${TOP}/test" -name "*.o" -delete 2>/dev/null || true

    make -j`nproc` -C ${TOP} TARGET=$TARGET
    if [ $? -ne 0 ]; then
        echo -e "${BOLD}╔════════════════════════════════════════════╗${RESET}"
        echo -e "${BOLD}* ${RED}Failed to build the main tests. Exiting.${RESET}"
        echo -e "${BOLD}╚════════════════════════════════════════════╝${RESET}"
        exit 1
    fi
}

###############################################################################
# Stage test binaries, runtime assets, and the output tarball under build/bin.
###############################################################################
install_test()
{
    declare -r INSTALL_PATH=${TOP_BUILD_DIR}/bin

    install -t ${INSTALL_PATH} ${TOP_BUILD_DIR}/usr/bin/*
    install -t ${INSTALL_PATH} ${TOP_BUILD_DIR}/usr/lib/*
    install -t ${INSTALL_PATH} run.sh

    # Remove bundled binder-stack libs — the target system supplies its own.
    rm -f "${INSTALL_PATH}"/libbinder.so* \
          "${INSTALL_PATH}"/libutils.so* \
          "${INSTALL_PATH}"/libbase.so* \
          "${INSTALL_PATH}"/libcutils.so* \
          "${INSTALL_PATH}"/libcutils_sockets.so* \
          "${INSTALL_PATH}"/liblog.so* \
          "${INSTALL_PATH}"/servicemanager

    # Install versioned halif libs and their softlinks from the snapshot build
    local HALIF_LIB_DIR="${RDK_HAL_DIR}/out/target/lib/rdk-halif-aidl"
    if [ -d "${HALIF_LIB_DIR}" ]; then
        # Dereference versioned libs (they may be symlinks to cmake build output)
        find "${HALIF_LIB_DIR}" -maxdepth 1 -name "lib*-v*-cpp.so" \
            -exec install -t "${INSTALL_PATH}" {} \; 2>/dev/null || true
        find "${HALIF_LIB_DIR}" -maxdepth 1 -type l -name "lib*-cpp_vts.so" \
            -exec cp -a {} "${INSTALL_PATH}/" \; 2>/dev/null || true
    fi

    # Copy ut-control shared library required at runtime
    local UT_CTRL_LIB_DIR="${UT_CORE_DIR}/framework/ut-control/build/${TARGET}/lib"
    if [ -d "${UT_CTRL_LIB_DIR}" ]; then
        find "${UT_CTRL_LIB_DIR}" -maxdepth 1 -name "libut_control.so*" \
            -exec install -t "${INSTALL_PATH}" {} \; 2>/dev/null || true
    fi

    cp -r ${PROFILES_DIR} ${INSTALL_PATH}
    cp -r "${TOP}/test_configuration" ${INSTALL_PATH}

    cd ${INSTALL_PATH}
    tar -czf ${OUT_TAR_FILE} *
    cd -
}

###############################################################################
# Clean local build outputs produced by the test and AIDL library builds.
###############################################################################
clean_test()
{
    echo -e "${BOLD}removing *.o${RESET}"

    make clean

    rm -rf ${TOP_BUILD_DIR}/bin/${OUT_TAR_FILE}
}

###############################################################################
# Remove all generated build output and checked-out dependency directories.
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
# Display command usage and supported build actions.
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

###############################################################################
# Print the current build configuration and resolved toolchain paths.
###############################################################################
print_build_config()
{
    echo -e "${BOLD}${GREEN}VTS Version                  : ${RESET}${YELLOW}$(git rev-parse --abbrev-ref HEAD)${RESET}"
    echo -e "${BOLD}${GREEN}UT_CORE_VERSION              : ${RESET}${YELLOW}$UT_CORE_VERSION${RESET}"
    echo -e "${BOLD}${GREEN}RDK_HALIF_AIDL_VERSION       : ${RESET}${YELLOW}$RDK_HALIF_AIDL_VERSION${RESET}"
    echo -e "${BOLD}${GREEN}LOCAL_BINDER_TOOLCHAIN_DIR   : ${RESET}${YELLOW}$LOCAL_BINDER_TOOLCHAIN_DIR${RESET}"
    echo -e "${BOLD}${GREEN}LOCAL_BINDER_SDK_DIR         : ${RESET}${YELLOW}$LOCAL_BINDER_SDK_DIR${RESET}"
    echo -e "${BOLD}${GREEN}LOCAL_BINDER_INCLUDE_DIR     : ${RESET}${YELLOW}$LOCAL_BINDER_INCLUDE_DIR${RESET}"
    echo -e "${BOLD}${GREEN}LOCAL_BINDER_LIB_DIR         : ${RESET}${YELLOW}$LOCAL_BINDER_LIB_DIR${RESET}"
    echo -e "${BOLD}${GREEN}BINDER_SDK_VERSION_FILE      : ${RESET}${YELLOW}$BINDER_SDK_VERSION_FILE${RESET}"
    echo -e "${BOLD}${GREEN}TARGET                       : ${RESET}${YELLOW}$TARGET${RESET}"
}

###############################################################################
# Validate the requested target before starting the build flow.
###############################################################################
validate_target()
{
    local valid_targets=("linux" "arm")

    if [[ ! " ${valid_targets[*]} " =~ " ${TARGET} " ]]; then
        echo -e "${BOLD}${RED}Invalid Target: ${TARGET}. Valid targets are: ${valid_targets[*]}${RESET}"
        help
        exit 1
    fi
}

###############################################################################
# Run the complete local build, install, and packaging flow.
###############################################################################
package_build_artifacts()
{
    echo -e "${BOLD}╔════════════════════════════════╗${RESET}"
    echo -e "${BOLD}* ${GREEN}Build Started${RESET}                  ${BOLD}*${RESET}"
    echo -e "${BOLD}╚════════════════════════════════╝${RESET}"
    stage_hal_artifacts
    build_ut_core
    build_halif_test
    install_test
    echo -e "${BOLD}╔════════════════════════════════╗${RESET}"
    echo -e "${BOLD}* ${GREEN}Build Successful${RESET}               ${BOLD}*${RESET}"
    echo -e "${BOLD}╚════════════════════════════════╝${RESET}"
}

###############################################################################
# Main script execution starts here
###############################################################################
mkdir -p "$TOP_BUILD_DIR"

for arg in "$@"; do
    if [[ "${arg^^}" == TARGET=* ]]; then
        TARGET="${arg#*=}"
    fi
done

print_build_config

if [ "$1" == "clean" ]; then
    clean_test
elif [ "$1" == "dist_clean" ]; then
    dist_clean_test
elif [ "$1" == "help" ]; then
    help
elif [[ "${1^^}" == TARGET=* ]]; then
    validate_target
    package_build_artifacts
else
    echo -e "${BOLD}${RED}Invalid Arguments${RESET}"
    help
fi