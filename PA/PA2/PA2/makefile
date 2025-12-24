CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
SRCDIR = src
BINDIR = bin
SRCS = $(wildcard $(SRCDIR)/*.cpp)
DEPS = $(wildcard $(SRCDIR)/*.h)
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(SRCDIR)/%.o,$(SRCS))
TARGET = mps
BINARY = $(BINDIR)/$(TARGET)

.PHONY: all clean help

all: $(BINARY)

$(BINARY): $(OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BINDIR):
	mkdir -p $(BINDIR)

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BINARY)
	# don't force-delete bin; leave it if it contains other files
	-rmdir --ignore-fail-on-non-empty $(BINDIR) 2>/dev/null || true

help:
	@echo "Builds '$(BINARY)' from sources in $(SRCDIR)."
	@echo "Usage: $(BINARY) <input file> <output file>"

# Example:
#   make        # builds $(BINARY)
#   make clean  # removes binary and object files
# After building, run as: $(BINARY) inputs/12.in outputs/12.out

