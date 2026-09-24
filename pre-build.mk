# Check if compiler version was already checked during current build
# session
ifneq ($(PRE_BUILT),1)

export PRE_BUILT = 1

MIN_GCC_VERSION = 9
MIN_CLANG_VERSION = 10

IS_GCC = $(shell $(CC) -v 2>&1 | grep -c "^gcc version ")
IS_CLANG = $(shell $(CC) -v 2>&1 | grep -c "^clang version ")

ifeq ($(IS_GCC),1)
GCC_VERSION = $(shell gcc -v 2>&1 | grep ^gcc | sed -E "s/gcc version ([0-9]+)\..*/\1/")
IS_GCC_ABOVE_MIN_VERSION := $(shell expr "$(GCC_VERSION)" ">=" "$(MIN_GCC_VERSION)")
ifneq ($(IS_GCC_ABOVE_MIN_VERSION),1)
$(error GCC version check failed. Installed version: $(GCC_VERSION). Minimal: $(MIN_GCC_VERSION))
endif
else ifeq ($(IS_CLANG),1)
CLANG_VERSION = $(shell clang -v 2>&1 | grep ^clang | sed -E "s/clang version ([0-9]+)\..*/\1/")
IS_CLANG_ABOVE_MIN_VERSION := $(shell expr "$(CLANG_VERSION)" ">=" "$(MIN_CLANG_VERSION)")
ifneq ($(IS_CLANG_ABOVE_MIN_VERSION),1)
$(error CLANG version check failed. Installed version: $(CLANG_VERSION). Minimal: $(MIN_CLANG_VERSION))
endif
endif

endif

# One checkpatch ignore list for every directory that runs the style gate, so
# the same code is accepted or reported the same way wherever the file sits.
# Outside the check above, which runs once per build session, so a recursive
# make still sees it.
CHECKPATCH_IGNORE_TYPES = \
	ARRAY_SIZE \
	BLOCK_COMMENT_STYLE \
	CODE_INDENT \
	CONST_STRUCT \
	EMBEDDED_FUNCTION_NAME \
	INITIALISED_STATIC \
	LEADING_SPACE \
	NEW_TYPEDEFS \
	PREFER_DEFINED_ATTRIBUTE_MACRO \
	SPACING \
	SPDX_LICENSE_TAG \
	SPLIT_STRING \
	SYMBOLIC_PERMS \
	UNSPECIFIED_INT \
	STATIC_CONST_CHAR_ARRAY

# checkpatch takes them as one comma separated word
CHECKPATCH_COMMA := ,
CHECKPATCH_EMPTY :=
CHECKPATCH_SPACE := $(CHECKPATCH_EMPTY) $(CHECKPATCH_EMPTY)
CHECKPATCH_IGNORE = $(subst $(CHECKPATCH_SPACE),$(CHECKPATCH_COMMA),$(strip $(CHECKPATCH_IGNORE_TYPES)))
