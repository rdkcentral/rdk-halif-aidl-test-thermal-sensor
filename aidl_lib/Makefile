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

.PHONY: all clean

CXX ?= g++

AIDL_LIB_NAME := aidl_client

AIDL_LIB := lib$(AIDL_LIB_NAME).so

BUILD_DIR ?= $(CURDIR)/../build

# Versioned src/include dirs resolved from rdk-halif-aidl by build.sh
HALIF_AIDL_SRC_DIR ?= $(error HALIF_AIDL_SRC_DIR is not set - run via build.sh)
HALIF_AIDL_INC_DIR ?= $(error HALIF_AIDL_INC_DIR is not set - run via build.sh)
BINDER_SDK_INCLUDE_DIR ?= $(error BINDER_SDK_INCLUDE_DIR is not set - run via build.sh)
BINDER_SDK_LIB_DIR ?= $(error BINDER_SDK_LIB_DIR is not set - run via build.sh)

# Source files from the versioned AIDL src directory
AIDL_SRC := $(shell find $(HALIF_AIDL_SRC_DIR) -type f \( -name '*.cpp' -o -name '*.c' \))

AIDL_LIB_INC_FLAGS := -I$(HALIF_AIDL_INC_DIR)
AIDL_LIB_INC_FLAGS += -I$(BINDER_SDK_INCLUDE_DIR)

AIDL_LIB_LD_LIBS := -lbinder -lutils

AIDL_LIB_LDFLAGS := -L$(BINDER_SDK_LIB_DIR)
AIDL_LIB_LDFLAGS += -Wl,-rpath-link=$(BINDER_SDK_LIB_DIR)

AIDL_LIB_CFLAGS := -Wall -Wextra -std=c++17 -ggdb -fPIC

AIDL_LIB := $(BUILD_DIR)/usr/lib/$(AIDL_LIB)


all: $(AIDL_LIB)

$(AIDL_LIB): $(AIDL_SRC:%.cpp=%.o)
	mkdir -p $(dir $@)
	$(CXX) -shared $(AIDL_LIB_LDFLAGS) $^ -o $@ $(AIDL_LIB_LD_LIBS)

%.o: %.cpp
	$(CXX) $(AIDL_LIB_CFLAGS) $(AIDL_LIB_INC_FLAGS) -c $< -o $@

clean:
	rm -f $(AIDL_LIB) $(AIDL_SRC:%.cpp=%.o)