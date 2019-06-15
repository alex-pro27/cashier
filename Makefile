CC = g++
CFLAGS = -c
LDFLAGS =
SOURCES = main.cpp websocket.cpp sha1.cpp base64.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = chatroom

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *o $(EXECUTABLE)
