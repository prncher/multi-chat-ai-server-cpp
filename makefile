# Compiler
CXX = cl.exe

# Compiler flags
CXXFLAGS = /EHsc /Zi /MDd

# Define your include directories
INCLUDE_DIRS = /I"C:\Program Files\OpenSSL\include" /I "C:\Ollama\vcpkg\installed\x64-windows\include"

LIBPATHS = /link /LIBPATH:"C:\Program Files\OpenSSL\lib" /LIBPATH:"C:\Ollama\vcpkg\installed\x64-windows\lib"

LIBS = Ws2_32.lib libssl.lib libcrypto.lib libcurl.lib

LFLAGS = 

# Target executable
TARGET = winsock-server.exe

# For deleting the target
TARGET_DEL = winsock-server.exe

# Source files
SRCS = SocketConnection.cpp SocketManager.cpp winsock-server.cpp

# Object files
MISC = *.obj *.ilk *.pdb


# Default rule to build and run the executable
all: $(TARGET) run


$(TARGET): $(OBJS)
	$(CXX) /Fe$(TARGET) $(LFLAGS) $(CXXFLAGS) $(INCLUDE_DIRS) $(SRCS) $(LIBS) $(LIBPATHS)

# Rule to run the executable
run: $(TARGET)
	$(TARGET)

# Clean rule to remove generated files
clean:
	del $(TARGET_DEL) $(MISC)
