# 声明编译器和编译选项
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra

# 声明库路径
LIBS = -lpthread -lmysqlclient

# 声明目标文件名和源文件名
TARGET = server
SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)

# 默认目标
all: $(TARGET)

# 编译目标
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

# 生成目标的依赖关系
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# 清理中间文件和可执行文件
clean:
	rm -f $(TARGET) $(OBJS)
