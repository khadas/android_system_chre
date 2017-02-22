#
# Build targets for a Hexagon-based processor
#

include $(CHRE_PREFIX)/build/defs.mk

# Hexagon Environment Checks ###################################################

# Ensure that the user has specified a path to the Hexagon toolchain that they
# wish to use.
ifeq ($(HEXAGON_TOOLS_PREFIX),)
$(error "You must supply a HEXAGON_TOOLS_PREFIX environment variable \
         containing a path to the hexagon toolchain. Example: \
         export HEXAGON_TOOLS_PREFIX=$$HOME/Qualcomm/HEXAGON_Tools/8.0.07")
endif

# Hexagon Tools ################################################################

TARGET_AR = $(HEXAGON_TOOLS_PREFIX)/Tools/bin/hexagon-ar
TARGET_CC = $(HEXAGON_TOOLS_PREFIX)/Tools/bin/hexagon-clang
TARGET_LD = $(HEXAGON_TOOLS_PREFIX)/Tools/bin/hexagon-link

# Hexagon Compiler Flags #######################################################

# Add Hexagon compiler flags
TARGET_CFLAGS += $(HEXAGON_CFLAGS)

# Enable position independence.
TARGET_CFLAGS += -fpic

# Disable splitting double registers.
TARGET_CFLAGS += -mllvm -disable-hsdr

# Enable the POSIX threading model.
TARGET_CFLAGS += -mthread-model posix

# This code is loaded into a dynamic module. Define this symbol in the event
# that any Qualcomm code needs it.
TARGET_CFLAGS += -D__V_DYNAMIC__

# Hexagon Archive Flags ########################################################

TARGET_ARFLAGS += -rsc

# Hexagon Linker Flags #########################################################

TARGET_LDFLAGS += -shared
TARGET_LDFLAGS += -call_shared

# Supported Hexagon Architectures ##############################################

HEXAGON_SUPPORTED_ARCHS = v60 v62

# Environment Checks ###########################################################

# Ensure that an architecture is chosen.
ifeq ($(filter $(HEXAGON_ARCH), $(HEXAGON_SUPPORTED_ARCHS)),)
$(error "The HEXAGON_ARCH variable must be set to a supported architecture \
         ($(HEXAGON_SUPPORTED_ARCHS))")
endif

# Target Architecture ##########################################################

# Set the Hexagon architecture.
TARGET_CFLAGS += -m$(strip $(HEXAGON_ARCH))

# Optimization Level ###########################################################

TARGET_CFLAGS += -O$(OPT_LEVEL)

# TODO: Consider disabling this when compiling for >-O0.
TARGET_CFLAGS += -D_DEBUG

# Variant Specific Sources #####################################################

TARGET_VARIANT_SRCS += $(HEXAGON_SRCS)