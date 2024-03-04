CXX = g++
CXXFLAGS = -I/usr/include/opencv4 -Wall -Wextra -O3
AR = ar
ARFLAGS = rcs
GO = go

CPP_DIR = cpp-processing-service
GO_DIR = go-html-service

LIBS = -L./$(CPP_DIR) -l:libprocessing.a -L/usr/lib -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
TARGETS = $(GO_DIR)/main $(CPP_DIR)/server

.PHONY: all clean uploads

all: $(TARGETS)

$(CPP_DIR)/server: $(CPP_DIR)/server.cpp $(CPP_DIR)/libprocessing.a
	$(CXX) $^ $(CXXFLAGS) $(LIBS) -o $@

$(CPP_DIR)/libprocessing.a: $(CPP_DIR)/processing.o
	$(AR) $(ARFLAGS) $@ $^

$(CPP_DIR)/%.o: $(CPP_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(GO_DIR)/main: $(GO_DIR)/main.go $(CPP_DIR)/libprocessing.a uploads
	cd $(GO_DIR) && \
	$(GO) mod tidy && \
	$(GO) build -o main main.go

uploads:
	mkdir -p uploads

clean:
	rm -f $(TARGETS) $(CPP_DIR)/*.a $(CPP_DIR)/*.o
	cd $(GO_DIR) && $(GO) clean

