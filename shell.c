#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#define CHK(op) do { if ((op) == -1) {perror(""); exit(EXIT_FAILURE);} } while (0)
#define NCHK(op) do { if ((op) == NULL) {perror(""); exit(EXIT_FAILURE);} } while (0)

#define READ_END  0
#define WRITE_END 1

#define MAX_NUMBER_PIPE 10
#define MAX_NUMBER_PROCESS MAX_NUMBER_PIPE + 1
#define MAX_NUMBER_COMMAND 10
#define MAX_NUMBER_STRINGS 20
#define MAX_LENGTH_STRING 256


struct process
{
	bool redir_stdin;
	char* file_redir_stdin;
	
	bool here_document;
	char* delimiter;
	char strings[MAX_NUMBER_STRINGS][MAX_LENGTH_STRING];
	int nb_string;
	bool here_string;
	int pseudo_pipe[2]; //simuler le comportement de "<<" et "<<<" avec un pipe qui n'est pas écrit explicitement dans la commande
	
	bool redir_stdout;
	bool append;
	char* file_redir_stdout;
	bool redir_stderr;
	char* file_redir_stderr;
	bool is_job;
	bool is_cd;
	char* dir_cd;
	char* command_string[MAX_NUMBER_COMMAND]; 
	int nb_command;
};

struct command
{
	struct process p[MAX_NUMBER_PROCESS];
	int pipes[MAX_NUMBER_PIPE][2];
	int nb_pipe;
	int index_command;
	int index_process;
};


int main()
{
	struct sigaction s_pere, s_fils;
	s_pere.sa_handler = SIG_IGN;
	s_pere.sa_flags = 0;
	CHK(sigemptyset(&s_pere.sa_mask));
	CHK(sigaction(SIGINT, &s_pere, NULL));
	CHK(sigaction(SIGQUIT, &s_pere, NULL));
	CHK(sigaction(SIGTSTP, &s_pere, NULL));
	
	while(1)
	{
		char* cwd = getcwd(NULL, 0);
		char* login;
		NCHK(login = getlogin());
		char hostname[HOST_NAME_MAX];
		CHK(gethostname(hostname, HOST_NAME_MAX));
		
		printf("%s@%s:%s$ ", login, hostname, cwd);
		CHK(fflush(stdout));
	
		char* line = NULL;
		size_t n = 0;
		if(getline(&line, &n, stdin) == -1) 
		{
			if(errno != 0) //pour ne pas afficher "success" dans le cas d'un CTRL+D (= EOF)
			{
				perror("getline");
			}
			else 
			{
				printf("\n");
			}
			free(line);
			free(cwd);
			exit(EXIT_FAILURE);
		}
		
		char* token = strtok(line, " \n");
		
		struct command c;
		c.index_command = 0;
		c.index_process = 0;
		c.nb_pipe = 0;
		
		for(int i = 0; i < MAX_NUMBER_PROCESS; ++i)
		{
			c.p[i].redir_stdin = false;
			c.p[i].file_redir_stdin = NULL;
			c.p[i].here_document = false;
			c.p[i].here_string = false;
			c.p[i].redir_stdout = false;
			c.p[i].append = false;
			c.p[i].file_redir_stdout = NULL;
			c.p[i].redir_stderr = false;
			c.p[i].file_redir_stderr = NULL;
			c.p[i].is_job = false;
			c.p[i].is_cd = false;
			c.p[i].dir_cd = NULL;
		}	

		while(token != NULL)
		{
			if(strcmp(token, "|") == 0)
			{
				c.p[c.index_process].command_string[c.index_command] = NULL;
				c.index_process += 1;
				c.nb_pipe += 1;
				c.p[c.index_process].nb_command = c.index_command;
				c.index_command = 0;
			}
			else if(strcmp(token, ">") == 0 || strcmp(token, "1>") == 0)
			{
				c.p[c.index_process].redir_stdout = true;
				c.p[c.index_process].append = false;
				c.p[c.index_process].file_redir_stdout = strtok(NULL, " \n"); //next token = filename
			}
			else if(strcmp(token, "2>") == 0)
			{
				c.p[c.index_process].redir_stderr = true;
				c.p[c.index_process].append = false;
				c.p[c.index_process].file_redir_stderr = strtok(NULL, " \n"); //next token = filename
			}
			else if(strcmp(token, ">>") == 0 || strcmp(token, "1>>") == 0)
			{
				c.p[c.index_process].redir_stdout = true;
				c.p[c.index_process].append = true;
				c.p[c.index_process].file_redir_stdout = strtok(NULL, " \n"); //next token = filename
			}
			else if(strcmp(token, "2>>") == 0)
			{
				c.p[c.index_process].redir_stderr = true;
				c.p[c.index_process].append = true;
				c.p[c.index_process].file_redir_stderr = strtok(NULL, " \n"); //next token = filename
			}
			else if(strcmp(token, "<") == 0 || strcmp(token, "0<") == 0)
			{
				c.p[c.index_process].redir_stdin = true;
				c.p[c.index_process].file_redir_stdin = strtok(NULL, " \n"); //next token = filename
			}
			else if(strcmp(token, "<<") == 0)
			{
				c.p[c.index_process].here_document = true;
				c.p[c.index_process].delimiter = strtok(NULL, " \n"); //next token = delimiter

				//tant que dans stdin, il n'y a pas exactement le délimiteur, continuer à lire dans stdin du père et écrire dans strings
				int index = 0;
				bool stop = false;
				while(!stop)
				{
					printf("> ");
					char string[MAX_LENGTH_STRING] = {0};
					fgets(string, MAX_LENGTH_STRING, stdin);
					string[strcspn(string, "\n")] = '\0';
					
					if(strcmp(string, c.p[c.index_process].delimiter) != 0)
					{
						snprintf(c.p[c.index_process].strings[index], MAX_LENGTH_STRING, "%s", string);
						index += 1;
					}
					else stop = true;
				}
				c.p[c.index_process].nb_string = index;
				CHK(pipe(c.p[c.index_process].pseudo_pipe));				
			}
			else if(strcmp(token, "<<<") == 0)
			{
				c.p[c.index_process].here_string = true;
				c.p[c.index_process].file_redir_stdin = strtok(NULL, " \n"); //next token = string
				CHK(pipe(c.p[c.index_process].pseudo_pipe));	
			}
			else if(strcmp(token, "&") == 0)
			{
				c.p[c.index_process].is_job = true;
			}
			else if(strchr(token, '&') != NULL)
			{
				c.p[c.index_process].is_job = true;
				token[strlen(token)-1] = '\0';
				c.p[c.index_process].command_string[c.index_command] = token;
				c.index_command += 1;
				c.p[c.index_process].nb_command = c.index_command;
			}
			else if(strcmp(token, "cd") == 0)
			{
				c.p[c.index_process].is_cd = true;
				c.p[c.index_process].dir_cd = strtok(NULL, " \n"); //next token = dir name
			}
			else
			{
				c.p[c.index_process].command_string[c.index_command] = token;
				c.index_command += 1;
				c.p[c.index_process].nb_command = c.index_command;
			}
			token = strtok(NULL, " \n");
		}
		c.p[c.index_process].command_string[c.index_command] = NULL;
		
		for(int i = 0; i < c.nb_pipe; ++i)
		{
			CHK(pipe(c.pipes[i]));
		}
		
		pid_t pid;
		int raison;
		for(int i = 0; i <= c.index_process; ++i)
		{
			switch(pid = fork())
			{
				case -1:
					perror("fork");
					exit(EXIT_FAILURE);
					break;
				
				case 0: 
					s_fils.sa_handler = SIG_DFL;
					s_fils.sa_flags = 0;
					CHK(sigemptyset(&s_fils.sa_mask));
					CHK(sigaction(SIGINT, &s_fils, NULL));
					CHK(sigaction(SIGTSTP, &s_fils, NULL));
					CHK(sigaction(SIGQUIT, &s_fils, NULL));
				
					if(c.nb_pipe > 0)
					{
						if(i == 0) //premier processus
						{
							CHK(close(c.pipes[i][READ_END]));
							CHK(close(STDOUT_FILENO));
							CHK(dup(c.pipes[i][WRITE_END]));
							CHK(close(c.pipes[i][WRITE_END]));
						}
						else if(i == c.index_process) //dernier processus
						{
							CHK(close(c.pipes[i-1][WRITE_END]));
							CHK(close(STDIN_FILENO));
							CHK(dup(c.pipes[i-1][READ_END]));
							CHK(close(c.pipes[i-1][READ_END]));
						}
						else //processus intermédiaire
						{
							CHK(close(c.pipes[i-1][WRITE_END]));
							CHK(close(STDIN_FILENO));
							CHK(dup(c.pipes[i-1][READ_END]));
							CHK(close(c.pipes[i-1][READ_END]));
							
							CHK(close(c.pipes[i][READ_END]));
							CHK(close(STDOUT_FILENO));
							CHK(dup(c.pipes[i][WRITE_END]));
							CHK(close(c.pipes[i][WRITE_END]));
						}
					}
					
					if(c.p[i].redir_stdin)
					{
						CHK(close(STDIN_FILENO));
						CHK(open(c.p[i].file_redir_stdin, O_RDONLY, 0666));
					}
					
					if(c.p[i].here_document || c.p[i].here_string) //le père va écrire le(s) string(s) dans le tube, ce contenu sera redirigé dans le stdin du fils
					{
						CHK(close(c.p[i].pseudo_pipe[WRITE_END]));
						CHK(dup2(c.p[i].pseudo_pipe[READ_END], STDIN_FILENO));
						CHK(close(c.p[i].pseudo_pipe[READ_END]));
					}
					
					if(c.p[i].redir_stdout)
					{
						CHK(close(STDOUT_FILENO));
						int flags;
						if(c.p[i].append)
						{
							flags = O_WRONLY|O_CREAT|O_APPEND;
						}
						else
						{
							flags = O_WRONLY|O_CREAT|O_TRUNC;
						}
						CHK(open(c.p[i].file_redir_stdout, flags, 0666));
					}
					
					if(c.p[i].redir_stderr)
					{
						CHK(close(STDERR_FILENO));
						int flags;
						if(c.p[i].append)
						{
							flags = O_WRONLY|O_CREAT|O_APPEND;
						}
						else
						{
							flags = O_WRONLY|O_CREAT|O_TRUNC;
						}
						CHK(open(c.p[i].file_redir_stderr, flags, 0666));
					}
					
					if(!c.p[i].is_cd)
					{
						CHK(execvp(c.p[i].command_string[0], c.p[i].command_string));
					}
					else 
					{
						free(line);
						free(cwd);
						exit(EXIT_SUCCESS);
					}
					break;
				
				default:
					if(c.nb_pipe > 0)
					{
						if(i > 0) //fermer le tube "précédent"
						{
							CHK(close(c.pipes[i-1][READ_END]));
							CHK(close(c.pipes[i-1][WRITE_END]));
						}
					}
					
					if(c.p[i].here_document || c.p[i].here_string) //le père écrit dans le tube dont le contenu sera redirigé dans le stdin du fils
					{
						CHK(close(c.p[i].pseudo_pipe[READ_END]));
						
						if(c.p[i].here_document)
						{
							for(int j = 0; j < c.p[i].nb_string; j++)
							{
								CHK(write(c.p[i].pseudo_pipe[WRITE_END], c.p[i].strings[j], strlen(c.p[i].strings[j])));
								CHK(write(c.p[i].pseudo_pipe[WRITE_END], "\n", 2)); //écrire un "\n" à la fin de chaque string
							}
						}
						else if(c.p[i].here_string)
						{
							CHK(write(c.p[i].pseudo_pipe[WRITE_END], c.p[i].file_redir_stdin, strlen(c.p[i].file_redir_stdin)));
							CHK(write(c.p[i].pseudo_pipe[WRITE_END], "\n", 2)); //écrire un "\n" à la fin de chaque string
						}
						
						CHK(close(c.p[i].pseudo_pipe[WRITE_END]));
					}
					
					if(c.p[i].is_cd)
					{
						CHK(chdir(c.p[i].dir_cd));
					}
					
					if(!c.p[i].is_job)
					{
						CHK(waitpid(-1, &raison, WUNTRACED)); //WUNTRACED est important pour prendre en compte un fils qui a été stoppé par le signal SIGTSTP
					}
					break;
			}
		}
		free(line);
		free(cwd);
	}
	return EXIT_SUCCESS;
}
