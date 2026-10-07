#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_client
{
	int		id;
	char	*buffer;
}	t_client;

void	fatal(void)
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

char	*str_join(char *old_buffer, char *new_text)
{
	char	*new_buffer;
	size_t	old_length;

	old_length = 0;
	if (old_buffer != NULL)
		old_length = strlen(old_buffer);
	new_buffer = malloc(old_length + strlen(new_text) + 1);
	if (new_buffer == NULL)
		fatal();
	new_buffer[0] = '\0';
	if (old_buffer != NULL)
		strcat(new_buffer, old_buffer);
	strcat(new_buffer, new_text);
	free(old_buffer);
	return (new_buffer);
}

int	extract_message(char **buffer, char **message)
{
	char	*rest;
	int		i;

	*message = NULL;
	if (*buffer == NULL)
		return (0);
	i = 0;
	while ((*buffer)[i] != '\0')
	{
		if ((*buffer)[i] == '\n')
		{
			rest = calloc(strlen(*buffer + i + 1) + 1, sizeof(char));
			if (rest == NULL)
				fatal();
			strcpy(rest, *buffer + i + 1);
			*message = *buffer;
			(*message)[i + 1] = '\0';
			*buffer = rest;
			return (1);
		}
		i++;
	}
	return (0);
}

void	broadcast(int sender_fd, int max_fd, fd_set write_fds, char *message)
{
	int	fd;

	fd = 0;
	while (fd <= max_fd)
	{
		if (fd != sender_fd && FD_ISSET(fd, &write_fds))
			send(fd, message, strlen(message), 0);
		fd++;
	}
}

int	main(int argc, char **argv)
{
	int					server_fd;
	int					client_fd;
	int					max_fd;
	int					next_id;
	int					bytes_read;
	int					fd;
	socklen_t			client_length;
	struct sockaddr_in	server_address;
	struct sockaddr_in	client_address;
	fd_set				active_fds;
	fd_set				read_fds;
	fd_set				write_fds;
	t_client			clients[FD_SETSIZE] = {0};

	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		return (1);
	}
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
		fatal();

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(2130706433);
	server_address.sin_port = htons(atoi(argv[1]));
	if (bind(server_fd, (struct sockaddr *)&server_address,
			sizeof(server_address)) < 0)
		fatal();
	if (listen(server_fd, 10) < 0)
		fatal();

	FD_ZERO(&active_fds);
	FD_SET(server_fd, &active_fds);
	max_fd = server_fd;
	next_id = 0;

	while (1)
	{
		read_fds = active_fds;
		write_fds = active_fds;
		if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
			fatal();

		if (FD_ISSET(server_fd, &read_fds))
		{
			client_length = sizeof(client_address);
			client_fd = accept(server_fd,
					(struct sockaddr *)&client_address, &client_length);
			if (client_fd < 0 || client_fd >= FD_SETSIZE)
				fatal();

			clients[client_fd].id = next_id++;
			clients[client_fd].buffer = NULL;
			FD_SET(client_fd, &active_fds);
			if (client_fd > max_fd)
				max_fd = client_fd;

			{
				char	message[64];

				sprintf(message, "server: client %d just arrived\n",
					clients[client_fd].id);
				broadcast(client_fd, max_fd, write_fds, message);
			}
		}
		else
		{
			fd = 0;
			while (fd <= max_fd)
			{
				if (fd != server_fd && FD_ISSET(fd, &read_fds))
				{
					char	buffer[1024];
					char	*line;

					memset(buffer, 0, sizeof(buffer));
					bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);
					if (bytes_read < 0)
						fatal();
					if (bytes_read == 0)
					{
						char	message[64];

						sprintf(message, "server: client %d just left\n",
							clients[fd].id);
						broadcast(fd, max_fd, write_fds, message);
						FD_CLR(fd, &active_fds);
						close(fd);
						free(clients[fd].buffer);
						clients[fd].buffer = NULL;
					}
					else
					{
						clients[fd].buffer = str_join(
								clients[fd].buffer, buffer);
						while (extract_message(&clients[fd].buffer, &line))
						{
							char	message[64 + strlen(line)];

							sprintf(message, "client %d: %s",
								clients[fd].id, line);
							broadcast(fd, max_fd, write_fds, message);
							free(line);
						}
						if (clients[fd].buffer != NULL
							&& clients[fd].buffer[0] == '\0')
						{
							free(clients[fd].buffer);
							clients[fd].buffer = NULL;
						}
					}
				}
				fd++;
			}
		}
	}
}