NAME = webserv

OBJ_DIR = obj

SOURCES = Webserv.cpp WebservInit.cpp WebservEvents.cpp WebservCgi.cpp WebservUtils.cpp \
		  Response.cpp ResponseConsts.cpp ResponseDirectory.cpp ResponseUtils.cpp \
		  Logger.cpp Request.cpp main.cpp 

OBJECTS = $(addprefix $(OBJ_DIR)/, $(SOURCES:.cpp=.o))

CFLAGS += -Wall -Wextra -Werror -std=c++20 -g

all: $(NAME)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(OBJ_DIR)
	c++ $(CFLAGS) -c $< -o $@

$(NAME): $(OBJECTS)
	c++ $(CFLAGS) -o $(NAME) $(OBJECTS)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
