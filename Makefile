# Author: A. Lochmann C 2010
# Based on a makefile found at: http://ubuntuforums.org/showthread.php?t=1204739
KDIR?=/lib/modules/`uname -r`/build
KARCH?=`uname -i`
KCROSS?=
INCLUDE_PATH:= ./include
BUILD_PATH:=build
BUILD_KERN:=$(BUILD_PATH)/kern
BUILD_USER:=$(BUILD_PATH)/user
TEST_DIR:=test

LD_TEXT = -e "LD\t$@"
LD_SO_TEXT = -e "LD SHARED\t$@"
CC_TEXT = -e "CC\t$<"
DEB_TEXT= -e "DEB\t$<"

# FIND EVERY EXISTING DEPENDENCY FILE
ifneq (,$(wildcard $(BUILD_PATH)))
EXISTING_DEPS:=$(shell find $(BUILD_PATH) -name '*.d')
else
EXISTING_DEPS:=
endif

ifndef VERBOSE
VERBOSE = 0
endif
ifeq ($(VERBOSE),0)
OUTPUT= @
else
OUTPUT=
endif

# COMPILER AND LINKER FLAGS
CROSS_COMPILE?=
CC:=$(CROSS_COMPILE)gcc
CFLAGS := -fPIC -Wall -Werror -c -g -I$(INCLUDE_PATH) -Werror=format
LDFLAGS :=
LDLIBS := -ldl -lpthread -lrt

#***************************** ADD YOUR LISTING OF SOURCE FILES FOR EACH DIRECTORY HERE *****************************
# Example:
#<name>_DIR=<dir_name>
#<name>_SRC=<source files>
#<name>_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(<name>_DIR)/%.o,$(<name>_SRC:%.cpp=%.o))

LIB_COMMON_DIR=lib
LIB_COMMON_SRC=datamodel.c query.c resultset.c api.c liballoc.c communication.c
LIB_COMMON_OBJ=$(patsubst %.o,$(BUILD_USER)/$(LIB_COMMON_DIR)/%.o,$(LIB_COMMON_SRC:%.c=%.o))

LIB_USERSPACE_DIR=$(LIB_COMMON_DIR)/userspace
LIB_USERSPACE_SRC=datamodel-userspace.c query-userspace.c resultset-userspace.c
LIB_USERSPACE_OBJ=$(patsubst %.o,$(BUILD_USER)/$(LIB_USERSPACE_DIR)/%.o,$(LIB_USERSPACE_SRC:%.c=%.o))

LIB_KERNEL_DIR:=$(LIB_COMMON_DIR)/kernel
LIB_KERNEL_SRC_= libkernel.c
LIB_KERNEL_SRC= $(patsubst %.c,$(BUILD_KERN)/$(LIB_KERNEL_DIR)/%.c,$(LIB_KERNEL_SRC_)) $(patsubst $(BUILD_USER)/%.c,$(BUILD_KERN)/%.c,$(LIB_COMMON_OBJ:%.o=%.c))

SLC_USER_BIN_SRC= libuserspace-main.c libuserspace-layer.c
SLC_USER_BIN_OBJ=$(patsubst %.o,$(BUILD_USER)/$(LIB_USERSPACE_DIR)/%.o,$(SLC_USER_BIN_SRC:%.c=%.o))

PROVIDER_KERNEL_DIR=provider/kernel
PROVIDER_KERNEL_SRC= $(patsubst %.c,$(BUILD_KERN)/%.c,$(shell find $(PROVIDER_KERNEL_DIR) -name "*.c"))

PROVIDER_USER_DIR=provider/userspace
PROVIDER_USER_SRC=$(shell find $(PROVIDER_USER_DIR) -name "*.c")
PROVIDER_USER_OBJ=$(patsubst %.o,$(BUILD_USER)/%.o,$(PROVIDER_USER_SRC:%.c=%.o))
PROVIDER_USER_SO=$(PROVIDER_USER_OBJ:%.o=%.so)

#***************************** BEGIN SOURCE FILES FOR TEST APPS *****************************

# Example:
#<name>_TEST=<testapp>
#<name>_TEST_SRC = <source files>
#<name>_TEST_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(TEST_DIR)/%.o,$(<name>_TEST_SRC:%.cpp=%.o))

QUERY_TEST=query-test
QUERY_TEST_SRC = query-test.c dummy.c
QUERY_TEST_OBJ=$(patsubst %.o,$(BUILD_USER)/$(TEST_DIR)/%.o,$(QUERY_TEST_SRC:%.c=%.o))

DATAMODEL_TEST=datamodel-test
DATAMODEL_TEST_SRC = datamodel-test.c dummy.c
DATAMODEL_TEST_OBJ=$(patsubst %.o,$(BUILD_USER)/$(TEST_DIR)/%.o,$(DATAMODEL_TEST_SRC:%.c=%.o))

RESULTSET_TEST=resultset-test
RESULTSET_TEST_SRC = resultset-test.c dummy.c
RESULTSET_TEST_OBJ=$(patsubst %.o,$(BUILD_USER)/$(TEST_DIR)/%.o,$(RESULTSET_TEST_SRC:%.c=%.o))

OBJ_API_TEST=obj-api-test
OBJ_API_TEST_SRC = obj-api-test.c dummy.c
OBJ_API_TEST_OBJ=$(patsubst %.o,$(BUILD_USER)/$(TEST_DIR)/%.o,$(OBJ_API_TEST_SRC:%.c=%.o))

EVT_API_TEST=evt-api-test
EVT_API_TEST_SRC = evt-api-test.c dummy.c
EVT_API_TEST_OBJ=$(patsubst %.o,$(BUILD_USER)/$(TEST_DIR)/%.o,$(EVT_API_TEST_SRC:%.c=%.o))

#*****************************			END SOURCE FILE				*****************************

# ADD YOUR NEW OBJ VAR HERE
# Example: $(<name>_OBJ)
OBJ = $(LIB_COMMON_OBJ) $(LIB_USERSPACE_OBJ) $(PROVIDER_USER_OBJ) $(SLC_USER_BIN_OBJ)
SLC_USER_BIN = $(BUILD_USER)/slc-core

# ADD HERE THE VAR FOR THE TEST APP
# Example: $(<name>_OBJ)
TEST_OBJ = $(QUERY_TEST_OBJ) $(DATAMODEL_TEST_OBJ) $(RESULTSET_TEST_OBJ) $(OBJ_API_TEST_OBJ) $(EVT_API_TEST_OBJ)
TEST_BIN = $(QUERY_TEST) $(DATAMODEL_TEST) $(RESULTSET_TEST) $(OBJ_API_TEST) $(EVT_API_TEST)
TEST_BIN := $(addprefix $(BUILD_PATH)/,$(TEST_BIN))

# ADD HERE YOUR NEW SOURCE DIRECTORY
# Example: $(<name>_DIR)
DIRS_ = $(TEST_DIR) $(LIB_COMMON_DIR) $(LIB_USERSPACE_DIR) $(LIB_KERNEL_DIR) $(PROVIDER_KERNEL_DIR) $(PROVIDER_USER_DIR)
DIRS_USER = $(patsubst %,$(BUILD_USER)/%,$(DIRS_))
DIRS_KERN = $(patsubst %,$(BUILD_KERN)/%,$(DIRS_))

#***************************** DO NOT EDIT BELOW THIS LINE EXCEPT YOU WANT TO ADD A TEST APPLICATION (OR YOU KNOW WHAT YOU'RE DOING :-) )***************************** 
DEP = $(subst .o,.d,$(OBJ)) $(subst .o,.d,$(TEST_OBJ))

all: git_version.h $(DIRS_USER) $(DIRS_KERN) $(DEP) $(OBJ) $(PROVIDER_USER_SO) $(SLC_USER_BIN) $(TEST_BIN) $(TEST_OBJ) kernel

user: git_version.h $(DIRS_USER) $(DEP) $(OBJ) $(PROVIDER_USER_SO) $(SLC_USER_BIN) $(TEST_BIN) $(TEST_OBJ)

#***************************** BEGIN TARGETS FOR TEST APPLICATION *****************************

$(BUILD_PATH)/$(QUERY_TEST): $(QUERY_TEST_OBJ) $(LIB_COMMON_OBJ) $(LIB_USERSPACE_OBJ)
	@echo $(LD_TEXT)
	$(OUTPUT)$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_PATH)/$(DATAMODEL_TEST): $(DATAMODEL_TEST_OBJ) $(LIB_COMMON_OBJ) $(LIB_USERSPACE_OBJ)
	@echo $(LD_TEXT)
	$(OUTPUT)$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_PATH)/$(RESULTSET_TEST): $(RESULTSET_TEST_OBJ) $(LIB_COMMON_OBJ) $(LIB_USERSPACE_OBJ)
	@echo $(LD_TEXT)
	$(OUTPUT)$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_PATH)/$(OBJ_API_TEST): $(OBJ_API_TEST_OBJ) $(LIB_COMMON_OBJ) $(LIB_USERSPACE_OBJ)
	@echo $(LD_TEXT)
	$(OUTPUT)$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_PATH)/$(EVT_API_TEST): $(EVT_API_TEST_OBJ) $(LIB_COMMON_OBJ) $(LIB_USERSPACE_OBJ)
	@echo $(LD_TEXT)
	$(OUTPUT)$(CC) $^ $(LDFLAGS) $(LDLIBS) -o $@

#***************************** END TARGETS FOR TEST APPLICATION	  *****************************

$(SLC_USER_BIN): $(LIB_COMMON_OBJ) $(LIB_USERSPACE_OBJ) $(SLC_USER_BIN_OBJ)
	@echo $(LD_TEXT)
	$(OUTPUT)$(CC) $^ $(LDFLAGS) -Wl,--export-dynamic $(LDLIBS) -o $@

$(BUILD_USER)/$(PROVIDER_USER_DIR)/%.so: $(BUILD_USER)/$(PROVIDER_USER_DIR)/%.o
	@echo $(LD_SO_TEXT)
	$(OUTPUT)$(LD) -shared -soname,lib$(patsubst $(BUILD_USER)/$(PROVIDER_USER_DIR)/%.so,%,$@).so -o $@ $< -lc

# Every object file depends on its source and dependency file
$(BUILD_USER)/%.o: %.c $(BUILD_USER)/%.d
	@echo $(CC_TEXT)
	$(OUTPUT)$(CC) $(CFLAGS) $< -o $@

# Every dependency file depends only on the corresponding source file
$(BUILD_USER)/%.d: %.c
	@echo $(DEB_TEXT)
	$(OUTPUT)$(call make-depend,$<,$(subst .d,.o,$@),$(subst .o,.d,$@))

kernel: $(DIRS_KERN) $(LIB_KERNEL_SRC) $(BUILD_KERN)/Kbuild $(BUILD_KERN)/Makefile $(PROVIDER_KERNEL_SRC)
	$(MAKE) -C $(KDIR) ARCH=$(KARCH) CROSS_COMPILE=$(KCROSS) KBUILD_EXTMOD=$$PWD/$(BUILD_KERN) KBUILD_SRC=$(KDIR)
	
kernel-clean:
	$(MAKE) -C $(KDIR) ARCH=$(KARCH) CROSS_COMPILE=$(KCROSS) KBUILD_EXTMOD=$$PWD/$(BUILD_KERN) KBUILD_SRC=$(KDIR) clean
	$(RM) $(LIB_KERNEL_SRC)
	$(RM) $(PROVIDER_KERNEL_SRC)
	$(RM) $(BUILD_KERN)/Kbuild
	$(RM) $(BUILD_KERN)/Makefile

git_version.h:
	@echo "Generating version information"
	$(OUTPUT)./git_version.sh -o git_version.h

$(BUILD_KERN)/%.c: %.c
	@echo "Creating link from $@ to $<"
	$(OUTPUT)ln -s $(PWD)/$< $@

$(BUILD_KERN)/Kbuild: Kbuild
	@echo "Creating link from $@ to $<"
	$(OUTPUT)ln -s $(PWD)/Kbuild $@

$(BUILD_KERN)/Makefile: Makefile
	@echo "Creating link from $@ to $<"
	$(OUTPUT)ln -s $(PWD)/Makefile $@

$(BUILD_PATH)/%:
	@echo "Creating directory $@"
	$(OUTPUT)mkdir -p $@

clean: kernel-clean clean-dep clean-obj 

clean-dep:
	$(RM) $(DEP)

clean-obj:
	$(RM) $(OBJ)

distclean: clean
	$(RM) -r $(BUILD_PATH)

#***************************** INCLUDE EVERY EXISTING DEPENDENCY FILE  *****************************
include $(EXISTING_DEPS)
#*****************************		END INCLUDE		       *****************************

.PHONY: all clean clean-deps clean-obj distclean kernel-clean tests objects kernel kernel-clean

define make-repo
   for dir in $(DIRS); \
   do \
	mkdir -p $(1)/$$dir; \
   done
endef

# usage: $(call make-depend,source-file,object-file,depend-file)
define make-depend
  $(CXX) -MM       \
        -MF $3    \
        -MP       \
        -MT $2    \
	-I$(INCLUDE_PATH) \
        $1
endef
