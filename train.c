/* #include <errno.h> */
/* #include <netdb.h> */
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* ===== KEEP UNCHANGED: function already supplied in main.c ===== */
int	extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int		i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf)
					* (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

#include <sys/select.h>

typedef struct s_client {
	int	id;
	char* buf;
} t_client;

int	g_max_fd;
int	g_nextid;
int	g_server_fd;

fd_set		g_active_fds;
t_client	g_cliets[FD_SETSIZE];

void fatal(void) {
	write(2, "Fatal error'\n", 12);
}

void	broadcast(int	client_fd, fd_set write_fds, char* message) {
	int	fd = 0;

	while (fd <= g_max_fd && fd != g_server_fd && FD_ISSET(fd, &write_fds)) {
		send(fd, message, strlen(message), 0);
		fd++;
	}
}

void	register_clietn(int	client_fd, fd_set write_fds) {
	char	message[64];

	FD_SET(client_fd, &g_active_fds);
	g_cliets[client_fd].id = g_nextid;
	g_nextid ++;
	g_cliets[client_fd].buf = NULL;
	if (client_fd > g_max_fd)
		g_max_fd = client_fd;
	sprintf(message, "%d:  ", g_cliets[client_fd].id);
	broadcast(client_fd, write_fds, message);
}

void	delete_client(int client_fd, fd_set write_fds) {
	char message[64];

	FD_CLR(client_fd, &g_active_fds);
	free(g_cliets[client_fd].buf);
	g_cliets[client_fd].buf = NULL;
	sprintf(message, "%d: ", g_cliets[client_fd].id);
	broadcast(client_fd, write_fds, message);
	while(g_max_fd > g_server_fd && !FD_ISSET(g_max_fd, &g_active_fds))
		g_max_fd--;
	close(client_fd);
}

void	send_message(int client_fd, fd_set write_fds) {

}


