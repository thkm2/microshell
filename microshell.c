#include "micro.h"

t_cmd	*parse_commands(char **av) {
	if (!pipe_in_av(av))
		return (create_cmd_node(NULL, av));
}

int	execute_command(t_cmd *cmd) {

}

int main(int ac, char **av, char **envp) {
	if (ac < 2)
		return (1);
	t_cmd *head = parse_commands(av);
	t_cmd *current = head;
	while (current) {
		execute_command(current);
	}
	while (wait(NULL) != -1) {
		/* code */
	}
	return (0);
}