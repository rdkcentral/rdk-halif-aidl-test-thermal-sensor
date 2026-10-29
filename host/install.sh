#!/usr/bin/env bash
#** *****************************************************************************
# *
# * If not stated otherwise in this file or this component's LICENSE file the
# * following copyright and licenses apply:
# *
# * Copyright 2026 RDK Management
# *
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *
# http://www.apache.org/licenses/LICENSE-2.0
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *
#* ******************************************************************************
MY_PATH="$(realpath ${BASH_SOURCE[0]})"
MY_DIR="$(dirname ${MY_PATH})"
TESTS_DIR="${MY_DIR}/tests"
RAFT_DIR="${MY_DIR}/raft"
RDK_RAFT_DIR="${MY_DIR}/rdk_raft"
PLUGINS_FRAMEWORK_DIR="${RAFT_DIR}/framework/plugins"
VENV_NAME="python_venv"
VENV_DIR="${MY_DIR}/${VENV_NAME}"  # Default virtual environment directory name

mkdir -p ${TESTS_DIR}

NO_COLOR="\e[0m"
RED="\e[0;31m"
CYAN="\e[0;36m"
YELLOW="\e[1;33m"
GREEN="\e[0;32m"
RED_BOLD="\e[1;31m"
BLUE_BOLD="\e[1;34m"
YELLOW_BOLD="\e[1;33m"

set -e # return on errors

function DUMP_VAR()
{
    local var_name="$1"
    local var_content="${!var_name}"
    echo -e ${CYAN}$var_name:[${var_content}]${NO_COLOR}
}
function ECHO()
{
    echo -e "$*"
}

function DEBUG()
{
    if [[ "$-" =~ "x" ]]; then
        return
    fi
    if [[ "${DEBUG_FLAG}" == "1" ]];then
        ECHO "${BLUE_BOLD}DEBUG: ${CYAN}$*${NO_COLOR}" > /dev/stderr
    fi
}

function INFO()
{
    ECHO "${GREEN}$*${NO_COLOR}"
}

function WARNING()
{
    ECHO "${YELLOW_BOLD}Warning: ${YELLOW}$*${NO_COLOR}" > /dev/stderr
}

function ERROR()
{
    ECHO "${RED_BOLD}ERROR: ${RED}$*${NO_COLOR}"
    exit 1
}

function install_pip_requirements()
{
    local requirements_file="$1"

    if [ ! -f ${requirements_file} ]; then
        WARNING "No ${requirements_file} found"
        return
    fi

    INFO "install_pip_requirements( ${requirements_file} ):"
    if pip install -r "$requirements_file"; then
        INFO "pip install completed"
    else
        ERROR "install_pip_requirements(): pip install failed."
        return 1
    fi
}

function clone_repo()
{
    local repo_url="$1"
    local path="$2"
    local version="$3"
    local message="$4"

    if [[ -z "${repo_url}" ]]; then
        ERROR "clone_repo: A url for a repository must be passed to the clone repo function"
    fi
    if [[ -z "${version}" ]]; then
        ERROR "clone_repo: Version not specified"
    fi
    if [[ ! -z "${path}" ]]; then
        if [[ ! -d "${path}" ]]; then
            INFO "git clone ${repo_url} @ ${version} ${CYAN}${message}${NO_COLOR}"
            git clone ${repo_url} "${path}"
            cd ${path}
            git checkout ${version}
            cd -
        fi
    fi
}

function setup_and_enable_venv()
{
    if [[ ! -d "$VENV_DIR" ]]; then
        ECHO "Creating Virtual environment ${YELLOW}'$VENV_NAME'${NO_COLOR}"
        python3 -m venv "$VENV_DIR"
        ECHO "Virtual environment created."
    fi

    if [[ ! -n "$VIRTUAL_ENV" ]]; then
        ECHO "please source & re-run ${YELLOW}install.sh${NO_COLOR} to ensure setup:"
        ECHO ${YELLOW}"source ./activate_venv.sh"${NO_COLOR}
        exit 1
    fi

    if [ -f "${VENV_DIR}/.installed" ]; then
        return
    fi

    touch ${VENV_DIR}/.installed
}

## Setup and start venv
setup_and_enable_venv

### Clone required repos ###
# Setup raft
clone_repo git@github.com:rdkcentral/python_raft.git "${RAFT_DIR}" 1.1.0 "in ./raft"
install_pip_requirements "${RAFT_DIR}"/requirements.txt

# Setup ut-raft
clone_repo git@github.com:rdkcentral/ut-raft.git "${PLUGINS_FRAMEWORK_DIR}/ut_raft" 2.2.1 "in ./raft/framework/plugins"
install_pip_requirements "${PLUGINS_FRAMEWORK_DIR}"/ut_raft/requirements.txt

# Setup rdk-raft alongside python_raft
clone_repo git@github.com:rdkcentral/rdk_raft.git "${RDK_RAFT_DIR}" 1.0.0 "in ./rdk_raft"

## Install your own sub git repo's in here as required
#
INFO "Install Complete"
