# Author: A. Lochmann C 2010
# Based on a makefile found at: http://ubuntuforums.org/showthread.php?t=1204739

INCLUDE_PATH:= ./include
BUILD_PATH:=build
OBJ_PATH:=$(BUILD_PATH)/obj
TEST_DIR:=test

LD_TEXT = -e "LD\t$@"
CC_TEXT = -e "CC\t$<"
DEB_TEXT= -e "DEB\t$<"

# FIND EVERY EXISTING DEPENDENCY FILE
ifneq (,$(wildcard $(BUILD_PATH)))
EXISTING_DEPS:=$(shell find $(BUILD_PATH) -name '*.d')
else
EXISTING_DEPS:=
endif

# COMPILER AND LINKER FLAGS
CC:=gcc
CFLAGS := -Wall -Werror -c -g -I$(INCLUDE_PATH)
LDFLAGS := 

#***************************** ADD YOUR LISTING OF SOURCE FILES FOR EACH DIRECTORY HERE *****************************
# Example:
#<name>_DIR=<dir_name>
#<name>_SRC=<source files>
#<name>_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(<name>_DIR)/%.o,$(<name>_SRC:%.cpp=%.o))

LIB_DIR=lib
LIB_SRC=datamodel.c query.c resultset.c
LIB_SRC_USERSPACE=$(LIB_SRC) datamodel-userspace.c query-userspace.c resultset-userspace.c
LIB_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(LIB_DIR)/%.o,$(LIB_SRC:%.c=%.o))
LIB_OBJ_USERSPACE=$(patsubst %.o,$(OBJ_PATH)/$(LIB_DIR)/%.o,$(LIB_SRC_USERSPACE:%.c=%.o))


#***************************** BEGIN SOURCE FILES FOR TEST APPS *****************************

# Example:
#<name>_TEST=<testapp>
#<name>_TEST_SRC = <source files>
#<name>_TEST_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(TEST_DIR)/%.o,$(<name>_TEST_SRC:%.cpp=%.o))

QUERY_TEST=query-test
QUERY_TEST_SRC = query-test.c
QUERY_TEST_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(TEST_DIR)/%.o,$(QUERY_TEST_SRC:%.c=%.o))

DATAMODEL_TEST=datamodel-test
DATAMODEL_TEST_SRC = datamodel-test.c
DATAMODEL_TEST_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(TEST_DIR)/%.o,$(DATAMODEL_TEST_SRC:%.c=%.o))

RESULTSET_TEST=resultset-test
RESULTSET_TEST_SRC = resultset-test.c
RESULTSET_TEST_OBJ=$(patsubst %.o,$(OBJ_PATH)/$(TEST_DIR)/%.o,$(RESULTSET_TEST_SRC:%.c=%.o))

#*****************************			END SOURCE FILE				*****************************

# ADD YOUR NEW OBJ VAR HERE
# Example: $(<name>_OBJ)
OBJ = $(LIB_OBJ)
OBJ_USERSPACE = $(OBJ) $(LIB_OBJ_USERSPACE)

# ADD HERE THE VAR FOR THE TEST APP
# Example: $(<name>_OBJ)
TEST_OBJ = $(QUERY_TEST_OBJ) $(DATAMODEL_TEST_OBJ)
TEST_BIN = $(QUERY_TEST) $(DATAMODEL_TEST) $(RESULTSET_TEST)
TEST_BIN := $(addprefix $(BUILD_PATH)/,$(TEST_BIN))

# ADD HERE YOUR NEW SOURCE DIRECTORY
# Example: $(<name>_DIR)
DIRS = $(TEST_DIR) $(LIB_DIR)

#***************************** DO NOT EDIT BELOW THIS LINE EXCEPT YOU WANT TO ADD A TEST APPLICATION (OR YOU KNOW WHAT YOU'RE DOING :-) )***************************** 
DEP = $(subst .o,.d,$(OBJ_USERSPACE)) $(subst .o,.d,$(TEST_OBJ))

all: buildrepo git_version.h $(DEP) $(OBJ) $(OBJ_USERSPACE) $(TEST_BIN) $(TEST_OBJ)

#***************************** BEGIN TARGETS FOR TEST APPLICATION *****************************

$(BUILD_PATH)/$(QUERY_TEST): $(QUERY_TEST_OBJ) $(OBJ_USERSPACE)
	@echo $(LD_TEXT)
	@$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_PATH)/$(DATAMODEL_TEST): $(DATAMODEL_TEST_OBJ) $(OBJ_USERSPACE)
	@echo $(LD_TEXT)
	@$(CC) $^ $(LDFLAGS) -o $@

$(BUILD_PATH)/$(RESULTSET_TEST): $(RESULTSET_TEST_OBJ) $(OBJ_USERSPACE)
	@echo $(LD_TEXT)
	@$(CC) $^ $(LDFLAGS) -o $@

#***************************** END TARGETS FOR TEST APPLICATION	  *****************************

objects: buildrepo $(OBJ) $(OBJ_USERSPACE) $(TEST_OBJ)

tests: buildrepo $(TEST_BIN)

# Every object file depends on its source and dependency file
$(OBJ_PATH)/%.o: %.c $(OBJ_PATH)/%.d
	@echo $(CC_TEXT)
	@$(CC) $(CFLAGS) $< -o $@

# Every dependency file depends only on the corresponding source file
$(OBJ_PATH)/%.d: %.c
	@echo $(DEB_TEXT)
	@$(call make-depend,$<,$(subst .d,.o,$@),$(subst .o,.d,$@))

git_version.h:
	@echo "Generating version information"
	@./git_version.sh -o git_version.h

clean: clean-dep clean-obj

clean-dep:
	$(RM) $(DEP)

clean-obj:
	$(RM) $(OBJ)

distclean: clean
	$(RM) -r $(BUILD_PATH)

buildrepo:
	@$(call make-repo)

#***************************** INCLUDE EVERY EXISTING DEPENDENCY FILE  *****************************
include $(EXISTING_DEPS)
#*****************************		END INCLUDE		       *****************************

.PHONY: all clean clean-deps clean-obj distclean tests objects

define make-repo
   for dir in $(DIRS); \
   do \
	mkdir -p $(OBJ_PATH)/$$dir; \
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
