SOURCES = elf-set-symbols.cpp
TARGET = elf-set-symbols

OBJECT_DIR = obj_files

SOURCE_DIRS = .
INCLUDE_DIRS = . ../elfio

ifeq (${wildcard $(OBJECT_DIR)},)
 	DUMMY := ${shell mkdir $(OBJECT_DIR)}
endif

oname = ${patsubst %.cpp, %.o, ${patsubst %.h,%.h.gch,$(1)}}

OBJECT_FILES = ${addprefix $(OBJECT_DIR)/,${call oname, $(SOURCES)}}
PRECOMPILED_OBJECT_FILES = ${call oname, $(PRECOMPILED_HEADERS)}

INCLUDES += ${addprefix -include, $(PRECOMPILED_HEADERS)}
INCLUDES += ${addprefix -I, $(INCLUDE_DIRS)}
INCLUDES += ${addprefix -I, $(SOURCE_DIRS)}

vpath %.cpp $(SOURCE_DIRS)


ifeq ($(ARCH),arm)
  CXX = arm-linux-gnueabi-g++
endif

ifeq ($(ARCH),m32)
  CXXFLAGS += -m32
endif

ifeq ($(ARCH),m64)
  CXXFLAGS += -m64
endif

### Static build
CXXFLAGS += -Os -Wall -fmessage-length=0
LDFLAGS +=  -static


all: $(TARGET) 
	
$(TARGET): $(PRECOMPILED_OBJECT_FILES) $(OBJECT_FILES)
	$(CXX) $(OBJECT_FILES) -o $@ $(LDFLAGS)

$(OBJECT_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

%.h.gch: %.h
	$(CXX) $(CXXFLAGS) -x c++-header $(INCLUDES) -c $< -o $@


clean:
	rm -rf $(OBJECT_DIR)

cleanall:
	rm -rf $(OBJECT_DIR) *.h.gch


