CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -MMD -MP

SRCDIR := src
OBJDIR := build/obj
BINDIR := bin
TARGET := $(BINDIR)/pa3

SOURCES := $(SRCDIR)/main.cpp \
           $(SRCDIR)/parser.cpp \
           $(SRCDIR)/grid.cpp \
           $(SRCDIR)/graph.cpp \
           $(SRCDIR)/router.cpp
OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

$(BINDIR) $(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

-include $(DEPS)
