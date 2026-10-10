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

/* ===== KEEP UNCHANGED: function already supplied in main.c ===== */
char	*str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	len = 0;
	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(len + strlen(add) + 1);
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	strcat(newbuf, add);
	free(buf);
	return (newbuf);
}

/* ================================================================================ ADD ============================================================ */
#include <sys/select.h>

typedef struct s_client
{
	int		id;
	char	*buf;
} t_client;

int		g_next_id;
int		g_max_fd;
int		g_server_fd;

t_client	g_clients[FD_SETSIZE];
fd_set		g_active_fds;

void	fatal_error(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void	broadcast(int sender_fd, fd_set write_fds, char *message)
{
	int	fd;

	fd = 0;
	while (fd <= g_max_fd)
	{
		if (fd != sender_fd && fd != g_server_fd && FD_ISSET(fd, &write_fds))
			send(fd, message, strlen(message), 0);
		fd++;
	}
}

void	register_client(int client_fd, fd_set write_fds)
{
	char	message[64];

	g_clients[client_fd].id = g_next_id++;
	g_clients[client_fd].buf = NULL;
	FD_SET(client_fd, &g_active_fds);
	if (client_fd > g_max_fd)
		g_max_fd = client_fd;
	sprintf(message, "server: client %d just arrived\n", g_clients[client_fd].id);
	broadcast(client_fd, write_fds, message);
}

void	remove_client(int client_fd, fd_set write_fds)
{
	char	message[64];

	sprintf(message, "server: client %d just left\n", g_clients[client_fd].id);
	broadcast(client_fd, write_fds, message);
	FD_CLR(client_fd, &g_active_fds);
	close(client_fd);
	free(g_clients[client_fd].buf);
	g_clients[client_fd].buf = NULL;
	while (g_max_fd > g_server_fd && !FD_ISSET(g_max_fd, &g_active_fds))
		g_max_fd--;
}

void	send_messages(int client_fd, fd_set write_fds)
{
	char	*message;
	char	prefix[64];
	int		extract_result;

	extract_result = extract_message(&g_clients[client_fd].buf, &message);
	while (extract_result == 1)
	{
		sprintf(prefix, "client %d: ", g_clients[client_fd].id);
		broadcast(client_fd, write_fds, prefix);
		broadcast(client_fd, write_fds, message);
		free(message);
		extract_result = extract_message(&g_clients[client_fd].buf, &message);
	}
	if (extract_result == -1)
		fatal_error();
}

/*
** ADAPT: replace this starter declaration:
** int main()
**
** with argc and argv, so the user can give a port: ./mini_serv 8080.
*/
int	main(int argc, char **argv)
{
	/*
	** DELETE these starter-only local variables from this location:
	** int sockfd, connfd, len;
	** struct sockaddr_in servaddr, cli;
	**
	** ADD these variables for the select-based many-client server:
	*/
	struct sockaddr_in	server_address;
	struct sockaddr_in	client_address;
	fd_set				read_fds;
	fd_set				write_fds;
	socklen_t			client_length;
	int					client_fd;
	int					fd;
	int					bytes_read;
	char				buffer[1001];

	/* ADD: required by subject.txt; main.c did not check its arguments. */
	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		return (1);
	}

	/* ADD: initialize the permanent set before adding g_server_fd to it. */
	FD_ZERO(&g_active_fds);

	/* FROM main.c: socket creation, with its debug output removed. */
	g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (g_server_fd < 0)
		fatal_error();
	/* DELETE: printf("socket creation failed...\\n"); */
	/* DELETE: printf("Socket successfully created..\\n"); */

	/*
	** FROM main.c: clearing and filling the IPv4 server address.
	** DELETE: bzero(&servaddr, sizeof(servaddr));
	** ADD: memset(&server_address, 0, sizeof(server_address));
	*/
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(2130706433);
	/* DELETE: servaddr.sin_port = htons(8081); */
	/* ADD: read the port from the terminal command. */
	server_address.sin_port = htons(atoi(argv[1]));

	/* FROM main.c: bind and listen, with starter debug output removed. */
	if (bind(g_server_fd, (struct sockaddr *)&server_address,
			sizeof(server_address)) < 0)
		fatal_error();
	/* DELETE: printf("socket bind failed...\\n"); */
	/* DELETE: printf("Socket successfully binded..\\n"); */
	if (listen(g_server_fd, 10) < 0)
		fatal_error();
	/* DELETE: printf("cannot listen\\n"); */

	/*
	** DELETE this final starter section from this location:
	** len = sizeof(cli);
	** connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	** if (connfd < 0) { ... } else { printf(...); }
	**
	** ADD this full loop instead: select waits for new clients, messages, and
	** disconnects without blocking a lazy client.
	*/
	FD_SET(g_server_fd, &g_active_fds);
	g_max_fd = g_server_fd;
	while (1)
	{
		read_fds = g_active_fds;
		write_fds = g_active_fds;
		if (select(g_max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
			fatal_error();
		client_length = sizeof(client_address);
		if (FD_ISSET(g_server_fd, &read_fds))
		{
			client_fd = accept(g_server_fd, (struct sockaddr *)&client_address,
					&client_length);
			if (client_fd < 0 || client_fd >= FD_SETSIZE)
				fatal_error();
			register_client(client_fd, write_fds);
		}
		fd = 0;
		while (fd <= g_max_fd)
		{
			if (fd != g_server_fd && FD_ISSET(fd, &read_fds))
			{
				bytes_read = recv(fd, buffer, 1000, 0);
				if (bytes_read < 0)
					fatal_error();
				if (bytes_read == 0)
					remove_client(fd, write_fds);
				else
				{
					buffer[bytes_read] = '\0';
					g_clients[fd].buf = str_join(g_clients[fd].buf, buffer);
					if (g_clients[fd].buf == NULL)
						fatal_error();
					send_messages(fd, write_fds);
				}
			}
			fd++;
		}
	}
}
