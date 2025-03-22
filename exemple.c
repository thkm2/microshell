#include <unistd.h>    // pour fork, pipe, dup2, chdir, execve, etc.
#include <stdlib.h>    // pour malloc, free, exit
#include <string.h>    // pour strcmp, strncmp
#include <sys/wait.h>  // pour waitpid

// Petit utilitaire pour écrire un message sur stderr
static void put_error(const char *msg)
{
    while (msg && *msg)
        if (write(2, msg, 1) < 0)
            ; // on ignore le retour ici pour ne pas compliquer
        else
            msg++;
}

// Affichage d’une erreur fatale et sortie immédiate
static void fatal_error(void)
{
    put_error("error: fatal\n");
    exit(1);
}

// Cette fonction exécute un programme via execve.
// En cas d’erreur d’execve, elle affiche le message d’erreur
// approprié et sort du processus enfant.
static void exec_cmd(char **argv, char **env)
{
    // argv[0] = chemin de l'exécutable
    // si execve échoue -> message d’erreur + exit(1)
    if (execve(argv[0], argv, env) == -1)
    {
        put_error("error: cannot execute ");
        put_error(argv[0]);
        put_error("\n");
        _exit(1); // on sort juste du processus fils
    }
}

// Vérifie si la commande est un "cd" et la gère si c’est le cas.
// Retourne 1 si c’est cd (et qu’on l’a géré), 0 sinon.
static int is_cd(char **argv)
{
    if (!argv[0])
        return 0;
    if (strcmp(argv[0], "cd") != 0)
        return 0;
    return 1;
}

// Gère la commande cd. Retourne 0 si on doit continuer, sinon on s’arrête.
static void handle_cd(char **argv)
{
    // cd doit avoir exactement 2 arguments => argv[0] = "cd", argv[1] = chemin, argv[2] = NULL
    int count = 0;
    for (int i = 0; argv[i]; i++)
        count++;

    if (count != 2)
    {
        put_error("error: cd: bad arguments\n");
        return;
    }
    // Tenter le chdir
    if (chdir(argv[1]) == -1)
    {
        put_error("error: cd: cannot change directory to ");
        put_error(argv[1]);
        put_error("\n");
    }
}

// Cette fonction exécute une suite de commandes reliées par des pipes.
// Elle reçoit un tableau de “commandes”, chaque commande étant un tableau d’arguments
// se terminant par NULL (comme argv). La dernière entrée de ce tableau de “commandes” est NULL.
static void execute_pipeline(char ***cmds, char **env)
{
    // On compte combien de commandes dans cmds
    int n = 0;
    while (cmds[n])
        n++;

    // S’il n’y a rien, on ne fait rien
    if (n == 0)
        return;

    // Cas d’une seule commande (pas de pipe)
    // On gère éventuellement cd en direct.
    if (n == 1)
    {
        if (is_cd(cmds[0]))
            handle_cd(cmds[0]);
        else
        {
            // forker et exécuter
            pid_t pid = fork();
            if (pid < 0)
                fatal_error();
            if (pid == 0)
            {
                // Processus fils
                exec_cmd(cmds[0], env);
            }
            // Processus parent attend
            if (waitpid(pid, NULL, 0) < 0)
                fatal_error();
        }
        return;
    }

    // Sinon, il y a au moins un pipe
    // On va chaîner les pipes successivement.
    int pipes[2];
    int input_fd = 0;  // au début, on lira sur stdin

    for (int i = 0; i < n; i++)
    {
        // S’il reste une commande après celle-ci, on crée un pipe.
        if (i < n - 1)
        {
            if (pipe(pipes) < 0)
                fatal_error();
        }

        // On gère un éventuel "cd" si c’est la seule commande du pipeline
        // Dans un pipeline, la consigne dit qu’on n’a pas de cd entouré par |,
        // donc normalement ça ne se présente pas ici si le parse est correct.
        // On peut ignorer le cas "cd" dans un pipeline (ou le traiter comme un exec normal).
        pid_t pid = fork();
        if (pid < 0)
            fatal_error();
        if (pid == 0)
        {
            // Processus fils
            // rediriger la lecture depuis input_fd
            if (dup2(input_fd, 0) < 0)
                fatal_error();

            // si on n’est pas la dernière commande, on redirige la sortie vers pipes[1]
            if (i < n - 1)
            {
                if (dup2(pipes[1], 1) < 0)
                    fatal_error();
            }
            // fermer les descripteurs
            if (close(pipes[0]) < 0) ; // on ignore
            if (close(pipes[1]) < 0) ; // on ignore
            // si input_fd n’est pas 0, on peut le fermer
            // (mais attention à ne pas fermer 0 si input_fd = 0)
            if (input_fd != 0 && close(input_fd) < 0) ;

            // exécuter la commande
            exec_cmd(cmds[i], env);
            // si on arrive ici, c’est qu’il y a eu une erreur dans exec_cmd
            _exit(1);
        }
        // Processus parent
        if (waitpid(pid, NULL, 0) < 0)
            fatal_error();

        // fermer l’ancien input_fd
        if (input_fd != 0 && close(input_fd) < 0) ;

        // fermer le côté write du pipe
        if (close(pipes[1]) < 0) ;

        // le côté read du pipe devient le prochain input_fd
        if (i < n - 1)
            input_fd = pipes[0];
    }
}

// Cette fonction lit argv[] jusqu’à rencontrer un ";" ou la fin,
// et construit la liste de commandes (séparées par "|") pour ce bloc.
// Elle retourne l’index où elle s’est arrêtée (pour pouvoir continuer
// avec le prochain bloc si besoin).
static int fill_pipeline(char **argv, int start, char ***buffer, int bufsize)
{
    // buffer = tableau de tableaux d’arguments
    // chaque élément de buffer[i] sera un *tableau d’argv* se terminant par NULL
    // On construit ça en lisant jusqu’au ";" ou fin ou argument NULL
    int cmd_count = 0;

    while (argv[start] && strcmp(argv[start], ";") != 0)
    {
        // On a une commande (jusqu’à "|" ou ";" ou fin)
        // On stocke les arguments pour cette commande
        char **one_cmd = NULL;
        int size_cmd = 0;
        // Parcourir tant qu’on ne rencontre pas "|", ";" ou fin
        while (argv[start] && strcmp(argv[start], "|") != 0 && strcmp(argv[start], ";") != 0)
        {
            // Ajouter argv[start] à one_cmd
            char **tmp = (char **)malloc(sizeof(char *) * (size_cmd + 2));
            if (!tmp)
                fatal_error();
            for (int k = 0; k < size_cmd; k++)
                tmp[k] = one_cmd ? one_cmd[k] : NULL;
            tmp[size_cmd] = argv[start];
            tmp[size_cmd + 1] = NULL;
            free(one_cmd);
            one_cmd = tmp;
            size_cmd++;
            start++;
        }
        // one_cmd est terminé par NULL
        if (cmd_count >= bufsize - 1)
        {
            put_error("error: fatal\n");
            exit(1);
        }
        buffer[cmd_count] = one_cmd;
        cmd_count++;

        if (argv[start] && strcmp(argv[start], "|") == 0)
            start++; // on saute le "|"
    }
    // on termine buffer
    buffer[cmd_count] = NULL;
    return start;
}

int main(int argc, char **argv, char **env)
{
    // On saute le nom du programme
    // argv ressemble à : ./microshell /bin/ls "|" /usr/bin/grep microshell ";" /bin/echo i love my microshell
    // On va boucler pour chaque bloc séparé par ";"
    int i = 1;

    while (i < argc)
    {
        // Ignorer les ";" en trop
        if (strcmp(argv[i], ";") == 0)
        {
            i++;
            continue;
        }

        // Préparer un buffer pour stocker jusqu’à 1024 commandes dans un pipeline
        // (C’est arbitraire, on peut adapter).
        char ***pipeline = (char ***)malloc(sizeof(char **) * 1024);
        if (!pipeline)
            fatal_error();

        // Remplir pipeline à partir de argv[i] jusqu’à ";" ou fin
        int old_i = i;
        i = fill_pipeline(argv, i, pipeline, 1024);

        // Exécuter le pipeline
        execute_pipeline(pipeline, env);

        // Libérer la mémoire allouée
        // pipeline[j] est un char**, pipeline[j][k] est un char*
        // On ne libère pas pipeline[j][k] car ce sont les argv originaux,
        // mais on libère pipeline[j] (le tableau de char*).
        for (int j = 0; pipeline[j]; j++)
            free(pipeline[j]);
        free(pipeline);

        // Si on est tombé sur un ";", il faut l’ignorer pour le prochain tour
        if (argv[i] && strcmp(argv[i], ";") == 0)
            i++;
    }
    return 0;
}
