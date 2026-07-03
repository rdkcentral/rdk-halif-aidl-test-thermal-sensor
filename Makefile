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

TARGET ?= linux
VARIANT := CPP

ROOT_DIR := $(CURDIR)
THERMALSENSOR_TEST_DIR := $(ROOT_DIR)/src

BIN_DIR := $(ROOT_DIR)/build/bin
LIB_DIR := $(ROOT_DIR)/build/$(TARGET)/lib
BUILD_DIR := $(ROOT_DIR)/build/$(TARGET)/obj

TARGET_EXEC := vts_thermalsensor_hal_test

# Get overall project version from .repo/manifests
MANIFEST_DIR := $(ROOT_DIR)/.repo/manifests
ifneq ($(wildcard $(MANIFEST_DIR)/.git),)
	VERSION := $(shell cd $(MANIFEST_DIR) && git describe --tags --always 2>/dev/null || echo "unknown")
else
	VERSION := unknown
$(warning Manifest directory $(MANIFEST_DIR) not found or not a Git repository. Using VERSION=unknown)
endif
$(info PROJECT_VERSION: $(VERSION))

# Define module versions (use main VERSION if no distinct tags)
THERMALSENSOR_VERSION := $(shell git describe --tags --always 2>/dev/null || echo "$(VERSION)")
BUILD_TIMESTAMP := $(shell date +"%Y-%m-%d_%H:%M:%S")

# Print module versions
$(info THERMALSENSOR_VERSION: $(THERMALSENSOR_VERSION))

# Read RDK_HALIF_AIDL_VERSION from environment, default to 'main'
RDK_HALIF_AIDL_VERSION ?= main
$(info RDK_HALIF_AIDL_VERSION: $(RDK_HALIF_AIDL_VERSION))

# Compiler flags with version and debug info
CXXFLAGS := -std=c++17 -g3 -O0  -Wno-unknown-pragmas -Wno-attributes \
            -DHALIF_TEST_PROJECT_VERSION=\"$(VERSION)\" \
            -DRDK_HALIF_AIDL_VERSION=\"$(RDK_HALIF_AIDL_VERSION)\" \
            -DTHERMALSENSOR_VERSION=\"$(THERMALSENSOR_VERSION)\"
$(info CXXFLAGS: $(CXXFLAGS))
$(info TARGET: $(TARGET))
$(info VARIANT: $(VARIANT))
CXXFLAGS += -DBUILD_TIMESTAMP=\"$(BUILD_TIMESTAMP)\"

# TODO: remove this once the ut-core implements runtime logging configuration
ifeq ($(ENABLE_UT_INFO_LOGGING),1)
CXXFLAGS += -DENABLE_UT_INFO_LOGGING
endif

SRC_DIRS := $(THERMALSENSOR_TEST_DIR)
SRC_DIRS += $(ROOT_DIR)/test

INC_DIRS := $(ROOT_DIR)/build/usr/include
INC_DIRS += $(SRC_DIRS)

XLDFLAGS := -L$(ROOT_DIR)/build/usr/lib
XLDFLAGS += -Wl,-rpath-link=$(ROOT_DIR)/build/usr/lib -Wl,--export-dynamic -Wl,-rpath,.
ifeq ($(TARGET),arm)
# ARM openssl is built by ut-core; add its lib dir before -lssl/-lcrypto so the
# linker can resolve them (ut-control appends static paths, but they come after
# the inherited -lssl/-lcrypto flags which would otherwise fail to resolve).
XLDFLAGS += -L$(ROOT_DIR)/ut-core/framework/ut-control/build/$(TARGET)/openssl/lib
XLDFLAGS += -Wl,-rpath-link=$(ROOT_DIR)/ut-core/framework/ut-control/build/$(TARGET)/openssl/lib
endif
XLDFLAGS += -lssl -lcrypto -lbinder -lutils
XLDFLAGS += -laidl_client

# required for std::optional
CXXFLAGS += -std=c++17
CXXFLAGS += -Wall -Wformat -Werror=format

# TODO: remove this once the ut-core implements runtime logging configuration
ifeq ($(DISABLE_UT_INFO_LOGGING),1)
CXXFLAGS += -DDISABLE_UT_INFO_LOGGING
endif

$(info SRC_DIRS: $(SRC_DIRS))
UT_CORE_DIR := $(CURDIR)/ut-core

export BIN_DIR
export LIB_DIR
export BUILD_DIR
export SRC_DIRS
export TARGET_EXEC
export INC_DIRS
export XLDFLAGS
export CXXFLAGS

.PHONY: all clean

all: $(TARGET_EXEC)

$(TARGET_EXEC):
	make -C $(UT_CORE_DIR) -j $(shell nproc) TARGET=$(TARGET) VARIANT=$(VARIANT)

clean:
	make -C $(UT_CORE_DIR) -j TARGET=$(TARGET) VARIANT=$(VARIANT) clean
	rm -rf $(THERMALSENSOR_TEST_DIR)/*.o
	rm -rf $(ROOT_DIR)/test/*.o
