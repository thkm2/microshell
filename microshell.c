#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

typedef struct s_cmd
{
	char			**av;
	struct s_cmd	*next;
	struct s_cmd	*prev;
}	t_cmd;

t_cmd	*parse_commands(char **av) {
	
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