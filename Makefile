NAME = webserv

OBJ_DIR = obj

SOURCES = main.cpp Webserv.cpp Logger.cpp Request.cpp

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
