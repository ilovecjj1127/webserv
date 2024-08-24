NAME = webserv

OBJ_DIR = obj

SOURCES = main.cpp Webserv.cpp requestParser.cpp debug.cpp

OBJECTS = $(addprefix $(OBJ_DIR)/, $(SOURCES:.cpp=.o))

CFLAGS += -Wall -Wextra -Werror

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
