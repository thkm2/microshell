#ifndef MICRO_H
# define MICRO_H

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

typedef struct s_cmd
{
	char			**av;
	struct s_cmd	*next;
	struct s_cmd	*prev;
}	t_cmd;

// utils
int pipe_in_av(char **av);
t_cmd *create_cmd_node(t_cmd **head, char **value);

#endif