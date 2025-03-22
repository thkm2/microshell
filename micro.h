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

#endif