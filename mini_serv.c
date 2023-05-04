#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

/*
	Program interface
*/

void	errorPrint(const char *message)
{
	write(2, message, strlen(message));
	exit(EXIT_FAILURE);
}

/*
	Socket interaction related
*/

static int	openPortSocket(const char *port)
{
	int	sockfd;
	struct sockaddr_in servaddr; 

	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		errorPrint("Fatal error\n");
	} 

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(port)); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		errorPrint("Fatal error\n");
	} 
	printf("Socket successfully binded..\n");
	if (listen(sockfd, 10) != 0) {
		errorPrint("Fatal error\n");
	}
	return (sockfd);
}

static int	acceptConnection(const int socket)
{
	int					connfd;
	socklen_t			len;
	struct sockaddr_in	cli;
 
	len = sizeof(cli);
	connfd = accept(socket, (struct sockaddr *)&cli, &len);
	if (connfd < 0) { 
        printf("server acccept failed...\n"); 
        exit(0); 
    } 
	return (connfd);
}

/*
	DEBUG related
*/

#include <signal.h> // debuging memleaks

void	infoPrint(const char *message, const int number)
{
	char buf[100];

	sprintf(buf, "%s: %d\n", message, number);
	write(2, buf, strlen(buf));
}

int		run = 42;

void	terminateProgram(int signal_code)
{
	(void) signal_code;
	run = 0;
}

/*
	Connection List related
*/

typedef struct connection_s
{
	int	fd;
	int	id;
	struct connection_s *next;
}	connection_t;

static connection_t	*lastListNode(connection_t **connectionListBegin)
{
	connection_t	*node = *connectionListBegin;

	if (node == NULL)
		return (NULL);
	while (node->next)
		node = node->next;
	return (node);
}

/*returns pointer to previous node - helps to keep iteration valid */
connection_t	*listEraseNode(connection_t **connectionListBegin, connection_t *target)
{
	connection_t	*node = *connectionListBegin;

	if (node == target)
	{
		*connectionListBegin = node->next;
		free(node);
		return (*connectionListBegin);
	}
	while (node)
	{
		if (target == node->next)
		{
			if (node->next->next)
			{
				free(node->next);
				node->next = node->next->next;
			} else
			{
				free(node->next);
				node->next = NULL;
			}
			return (node);
		}
		node = node->next;
	}
	return (NULL);
}

void	listClear(connection_t **connectionListBegin)
{
	while (*connectionListBegin)
	{
		listEraseNode(connectionListBegin, lastListNode(connectionListBegin));
	}
}

/* it applies a function onto a list, but function can return EXIT_FAILURE to signify that we need to delete 
current node, but forward iterations are valid */
static void	smartListIter(connection_t **connectionListBegin, int (*function)(connection_t*, void*, const int), void *param, const int param2)
{
	connection_t	*node = *connectionListBegin;

	while (node)
	{
		if (function(node, param, param2) == EXIT_FAILURE)
		{
			node = listEraseNode(connectionListBegin, node);
			if (node)
				node = node->next;
		}
		else
			node = node->next;
	}
}

void	addConnection(connection_t **connectionListBegin, const int connect_fd)
{
	connection_t	*connection;
	static int		id_count;

	connection = calloc(1, sizeof(connection_t));
	connection->fd = connect_fd;
	connection->id = id_count++;
	connection->next = NULL;
	if (*connectionListBegin == NULL)
		*connectionListBegin = connection;
	else
		lastListNode(connectionListBegin)->next = connection;
}

/*
	Connection related
*/

int	sendToOthers(connection_t *connection, void *message, const int id)
{
	if (connection->id != id)
		send(connection->fd, message, strlen((const char *)message), 0);
	return (EXIT_SUCCESS);
}

/*
	Protocol procedure related
*/

void	informOfNewConnection(connection_t **connectionListBegin)
{
	char	message[50];

	memset(message, 0, 50);
	sprintf(message, "server: client %d just arrived\n", lastListNode(connectionListBegin)->id);
	smartListIter(connectionListBegin, sendToOthers, message, lastListNode(connectionListBegin)->id);
}

typedef struct mesanger_app_data_s
{
	fd_set			*incoming;
	fd_set 			*outgoing;
	connection_t	**connectionListBegin;
}	mesanger_app_data_t;

/* wraps message properly for sending - handles if there newlines or and no new lines*/
void	splitSend(char *buffer, connection_t **connectionListBegin, connection_t *connection)
{
	char	messageBuffer[1024];
	int		iterator = 0;

	memset(messageBuffer, 0, 1024);
	sprintf(messageBuffer, "client %d: ", connection->id);
	while (buffer[iterator])
	{
		messageBuffer[strlen(messageBuffer)] = buffer[iterator];
		if (buffer[iterator] == '\n' || !buffer[iterator + 1]) // 
		{
			smartListIter(connectionListBegin, sendToOthers, messageBuffer, connection->id);
			memset(messageBuffer, 0, 1024);
			sprintf(messageBuffer, "client %d: ", connection->id);
		}
		iterator++;
	}
}

/* if client sends a message this function will receive the message, and distribute to all connected clients
	handles if client disconected also, uses messageSplit to phrase messages for distribution*/
int	distributeMessages(connection_t *connection, void *param, const int empty)
{
	mesanger_app_data_t	*messanger = (mesanger_app_data_t*)param;
	char				messageBuffer[1024*4];
	int					receive = 1024;

	(void) empty;
	if (FD_ISSET(connection->fd, messanger->incoming))
	{
		memset(messageBuffer, 0, 1024*4);
		while (receive == 1024 || messageBuffer[strlen(messageBuffer) - 1] != '\n')
		{
			receive = recv(connection->fd, messageBuffer + strlen(messageBuffer), 1024, 0);
			if (receive == 0)
			{
				if (strlen(messageBuffer))
					splitSend(messageBuffer, messanger->connectionListBegin, connection);
				memset(messageBuffer, 0, 1024*4);
				sprintf(messageBuffer, "server: client %d just left\n", connection->id);
				smartListIter(messanger->connectionListBegin, sendToOthers, messageBuffer, connection->id);
				FD_CLR(connection->fd, messanger->incoming);
				FD_CLR(connection->fd, messanger->outgoing);
				close(connection->fd);
				return (EXIT_FAILURE);
			}
		}
		splitSend(messageBuffer, messanger->connectionListBegin, connection);
	}
	return (EXIT_SUCCESS);
}

void	handleMessaging(connection_t **connectionListBegin, fd_set *incoming, fd_set *outgoing)
{
	mesanger_app_data_t	messanger;

	messanger.incoming = incoming;
	messanger.outgoing = outgoing;
	messanger.connectionListBegin = connectionListBegin;

	smartListIter(connectionListBegin, distributeMessages, (void*)&messanger, 0);
}

/*
	OBSERVATION related
*/

static int	selectBiggerFd(connection_t *connection, void *biggest_fd, const int sock_fd)
{
	(void) sock_fd;
	if (*(int*)biggest_fd < connection->fd)
		*(int*)biggest_fd = connection->fd;
	return (EXIT_SUCCESS);
}
/* gain proper select first parameter */
int	getBiggestFd(connection_t **connectionListBegin, const int sock_fd)
{
	int	biggest_fd = sock_fd;
	if (*connectionListBegin != NULL)
	{
		smartListIter(connectionListBegin, selectBiggerFd, &biggest_fd, sock_fd);
	}
	return (biggest_fd + 1);

}

static int	setObservation(connection_t *connection, void *data, const int param)
{
	mesanger_app_data_t	*observables = (mesanger_app_data_t*)data;

	(void) param;
	FD_SET(connection->fd, observables->incoming);
	FD_SET(connection->fd, observables->outgoing);
	return (EXIT_SUCCESS);
}

/* Observation Cycle handle */
void	reinitiateObserver(const int socket, connection_t **connectionListBegin, fd_set *incoming, fd_set *outgoing)
{
	mesanger_app_data_t	observables;

	observables.incoming = incoming;
	observables.outgoing = outgoing;
	observables.connectionListBegin = connectionListBegin;

	FD_ZERO(incoming);
	FD_ZERO(outgoing);
	FD_SET(socket, incoming);
	smartListIter(connectionListBegin, setObservation, (void*)&observables, 0);
}

int main(int argc, char **argv)
{
	int				socket;
	fd_set			incoming;
	fd_set			outgoing;
	connection_t	*connectionListBegin = NULL;

	if (argc < 2)
		errorPrint("Wrong number of arguments\n");
	socket = openPortSocket(argv[1]);
	FD_ZERO(&incoming);
	FD_ZERO(&outgoing);
	FD_SET(socket, &incoming);
	signal(SIGINT, terminateProgram); // debug memleaks
	while (run) // debug memleaks
	{
		if (select(getBiggestFd(&connectionListBegin, socket), &incoming, &outgoing, NULL, NULL) == -1)
		{
			close(socket);
			listClear(&connectionListBegin);
			return (0);
		}
		handleMessaging(&connectionListBegin, &incoming, &outgoing);
		if (FD_ISSET(socket, &incoming))
		{
			addConnection(&connectionListBegin, acceptConnection(socket));
			informOfNewConnection(&connectionListBegin);
			FD_SET(lastListNode(&connectionListBegin)->fd, &incoming);
			FD_SET(lastListNode(&connectionListBegin)->fd, &outgoing);
		}
		reinitiateObserver(socket, &connectionListBegin, &incoming, &outgoing);
	}
	close(socket);
	listClear(&connectionListBegin);
	return (0);
}
