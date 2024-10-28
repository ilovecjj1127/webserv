NAME = webserv

SRC_DIR = src
OBJ_DIR = obj

SOURCES = $(addprefix $(SRC_DIR)/, main.cpp \
	Webserv.cpp \
	WebservInit.cpp \
	WebservEvents.cpp \
	WebservCgi.cpp \
	WebservUtils.cpp \
	Response.cpp \
	ResponseConsts.cpp \
	ResponseDirectory.cpp \
	ResponseUtils.cpp \
	WebservConfig.cpp \
	Logger.cpp \
	Request.cpp)

OBJECTS = $(addprefix $(OBJ_DIR)/, $(notdir $(SOURCES:.cpp=.o)))

CFLAGS += -Wall -Wextra -Werror -std=c++20 -g

all: $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	c++ $(CFLAGS) -c $< -o $@

$(NAME): $(OBJECTS)
	c++ $(CFLAGS) -o $(NAME) $(OBJECTS)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

bonus: all

.PHONY: all clean fclean re bonus
