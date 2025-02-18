#include "micro.h"

int pipe_in_s(char *s) {
	int i = -1;
	while (s[++i])
		if (s[i] == '|')
			return (1);
	return (0);
}

int pipe_in_av(char **av) {
	int i = -1;
	while (av[++i])
		if (pipe_in_s(av[i]))
			return (1);
	return (0);
}

t_cmd *create_cmd_node(t_cmd **head, char **value) {
	t_cmd *rs = malloc(sizeof(t_cmd));
	if (!rs)
		return (NULL);
	rs->av = value;
	rs->next = NULL;
	if (!head) {
		rs->prev = NULL;
		*head = rs;
	} else {
		t_cmd *last = *head;
		while (last->next)
			last = last->next;
		last->next = rs;
		rs->prev = last;
	}
}