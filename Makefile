G++ := g++ -O3
INCLUDE := ./include/
SRC_DIR := ./src 
SRCS := $(shell find $(SRC_DIR) -name "*.cpp" )

all: init5
init5:
	$(G++) -I $(INCLUDE) -o $@ $(SRCS)

clean:
	rm init5