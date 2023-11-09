# 声明编译器和编译选项
CC = g++
CCFLAGS = -std=c++11 -Wall -Wextra

# 声明库路径
LIBS = -lpthread -lmysqlclient

# 声明目标文件名和源文件名
TARGET = server
SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp,obj/%.o,$(SRC))
INC = $(wildcard inc/*.h)

# 默认目标
all:$(TARGET)

# 编译目标
$(TARGET):$(OBJ)
	$(CC) $(CCFLAGS) $^ -o $@ $(LIBS)

# 生成目标的依赖关系
obj/%.o:src/%.cpp inc/%.h
	$(CC) $(CCFLAGS) -c $< -o $@

$(OBJ):|obj

# 创建obj文件夹
obj:
	mkdir -p obj

# 清理中间文件和可执行文件
clean:
	-rm -f $(TARGET) $(OBJ)

.PHONY: all clean
