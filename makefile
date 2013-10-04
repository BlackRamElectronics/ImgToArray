# I am a comment, and I want to say that the variable CC will be
# the compiler to use.
CC=g++

BUILD_DIR = Debug

LIBS =
LIBS_DIR =
INCLUDES=

CFLAGS=-c -Wall $(INCLUDES)
CSOURCES= ImgToArray.c
CPPSOURCES= 

OBJECTS=${CPPSOURCES:.cpp=.o} ${CSOURCES:.c=.o}
OBJECTS_DEBUG=$(addprefix ${BUILD_DIR}/,$(CSOURCES:.c=.o)) $(addprefix ${BUILD_DIR}/,$(CPPSOURCES:.cpp=.o))
VPATH = Debug

all: $(OBJECTS)
	echo "Linking $(OBJECTS_DEBUG)"
	$(CC) $(OBJECTS_DEBUG) -o ImgToArray $(LIBS_DIR) $(LIBS)

%.o: %.c
	echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o ${BUILD_DIR}/$@
	
%.o: %.cpp
	echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o ${BUILD_DIR}/$@
	
%dir: %.c
	echo "Generating dir $(subst /,\,$(dir ${BUILD_DIR}\$@))"
	mkdir $(subst /,\,$(dir ${BUILD_DIR}\$@))

clean:
	rm -rf *o AtDev
	rm $(OBJECTS_DEBUG)
