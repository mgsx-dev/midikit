
MKDIR_P = mkdir -p
LN_S = ln -s

BUILDDIR := $(PROJECTDIR)/build

OBJDIR := $(BUILDDIR)/$(SUBDIR)
LIBDIR := $(BUILDDIR)
BINDIR := $(BUILDDIR)

CC = clang
CFLAGS = -O3 -Wall -I$(PROJECTDIR) -DSUBDIR=\"$(SUBDIR)\" #-DNO_LOG -DNO_ASSERT -DNO_PRECOND -DNO_ERROR
CFLAGS_OBJ = $(CFLAGS) -c
LDFLAGS = -L$(LIBDIR)
LDFLAGS_LIB = $(LDFLAGS) -shared -O4
LDFLAGS_BIN = $(LDFLAGS)

LIB_SUFFIX = .so
BIN_SUFFIX =

DRIVERS=applemidi

