#G++ := g++ -O3
#INCLUDE := ./include/
#SRC_DIR	:= ./src
#
#SRCS := $(shell find $(SRC_DIR) -name "*.cpp" )
#
#all: init5
#
#init5:
#	$(G++) -I $(INCLUDE) -w -o $@ $(SRCS)
#
#
#clean:
#	rm -f init5
#	echo '' > log.txt