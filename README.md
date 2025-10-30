# Shell

This shell has the following functionalities : 
- simple commands (eg. ***./shell ls -a***)
- commands using one or more pipes (eg. ***./shell ls -a | wc***)
- redirection of stdin, stdout, and stderr (eg. ***./shell ls -a > file***)
- job creation (eg. ***./shell find / -size 15c &***)
- *SIGINT*, *SIGTSTP*, and *SIGQUIT* signals handling (eg. using CTRL+C (= *SIGINT*) will interrupt the child process (= command executed in the shell) and not the shell itself)
