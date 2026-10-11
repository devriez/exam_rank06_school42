#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/select.h>

int extract_message(char **buf, char **msg) {
  char *newbuf;
  int i;

  *msg = 0;
  if (*buf == 0)
    return (0);
  i = 0;
  while ((*buf)[i]) {
    if ((*buf)[i] == '\n') {
      newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
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

char *str_join(char *buf, char *add) {
  char *newbuf;
  int len;

  if (buf == 0)
    len = 0;
  else
    len = strlen(buf);
  newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
  if (newbuf == 0)
    return (0);
  newbuf[0] = 0;
  if (buf != 0)
    strcat(newbuf, buf);
  free(buf);
  strcat(newbuf, add);
  return (newbuf);
}

typedef struct s_client {
	int	id;
	char*	buf;
} t_client;

t_client g_clients[FD_SETSIZE];

fd_set	g_active_fds;

int	g_max_fd;
int	g_server_fd;
int	g_next_id;

void fatal_error(void) {
	write(2, "Fatal error\n", 12);
	exit(1);
}

void	broadcast(int sender_fd, fd_set write_fds, char* message) {
	int	fd=0;

	while (fd <= g_max_fd) {
		if (fd != g_server_fd && fd != sender_fd && FD_ISSET(fd, &write_fds)) {
			if (send(fd, message, strlen(message), 0) < 1)
				fatal_error();
		}
		fd++;
	}
}

void	accept_client(int client_fd, fd_set write_fds) {
	char	message[64];

	if (g_max_fd < client_fd)
		g_max_fd = client_fd;

	FD_SET(client_fd, &g_active_fds);
	g_clients[client_fd].id = g_next_id++;
	g_clients[client_fd].buf = NULL;

	sprintf(message, "server: client %d just arrived\n", g_clients[client_fd].id);
	broadcast(client_fd, write_fds, message);	
}

void	delete_client(int client_fd, fd_set write_fds) {
	char message[64];

	sprintf(message, "server: client %d just left\n", g_clients[client_fd].id);
	broadcast(client_fd, write_fds, message);

	close(client_fd);
	free(g_clients[client_fd].buf);
	g_clients[client_fd].buf = NULL;
	FD_CLR(client_fd, &g_active_fds);

	while (g_max_fd > g_server_fd && !FD_ISSET(g_max_fd, &g_active_fds))
	       g_max_fd--;	
}


void	send_message(int client_fd, fd_set write_fds) {
	char	*message;
	char	prefix[64];
	int	extract_res;

	extract_res = extract_message(&g_clients[client_fd].buf, &message);
	while(extract_res == 1) {
		sprintf(prefix, "client %d: ", g_clients[client_fd].id);
		broadcast(client_fd, write_fds, prefix);
		broadcast(client_fd, write_fds, message);
		free(message);
		extract_res = extract_message(&g_clients[client_fd].buf, &message);
	}
	if (extract_res < 0)
		fatal_error();

}

int main(int argc, char** argv) {

  char buffer[1001];
 
  struct sockaddr_in	servaddr;

  fd_set	write_fds;
  fd_set	read_fds;

  int	client_fd;
  int	fd;
  int	byte_read;

  if (argc != 2) {
  	write(2, "Wrong number of arguments\n", 26);
	exit(1);
  }

  // socket create and verification
  g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_server_fd == -1)
	  fatal_error();
  bzero(&servaddr, sizeof(servaddr));

  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
  servaddr.sin_port = htons(atoi(argv[1]));

  // Binding newly created socket to given IP and verification
  if ((bind(g_server_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) !=
      0) {
    fatal_error();
  }
  if (listen(g_server_fd, 10) != 0) {
    fatal_error();
  }

  g_next_id = 0;
  g_max_fd = g_server_fd;
  FD_ZERO(&g_active_fds);
  FD_SET(g_server_fd, &g_active_fds);

  while(1) {
  	write_fds = g_active_fds;
	read_fds = g_active_fds;

	if (select(g_max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
		fatal_error();
	
	if (FD_ISSET(g_server_fd, &read_fds)) {
		client_fd = accept(g_server_fd, NULL, NULL);
	        if (client_fd < 0 || client_fd >= FD_SETSIZE)
        	        fatal_error();
		accept_client(client_fd, write_fds);
	}
	fd = 0;
	while (fd <= g_max_fd) {
		if (fd != g_server_fd && FD_ISSET(fd, &read_fds)) {
			byte_read = recv(fd, buffer, 1000, 0);
			if (byte_read < 0)
				fatal_error();
			if (byte_read == 0)
				delete_client(fd, write_fds);
			else {
				buffer[byte_read] = '\0';
				g_clients[fd].buf = str_join(g_clients[fd].buf, buffer);
				send_message(fd, write_fds);
			}
		}
		fd++;
	}
	
  }
}
