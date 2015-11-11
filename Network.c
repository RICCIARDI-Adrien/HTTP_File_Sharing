/** @file Network_Linux.c
 * @see Network.h for description.
 * @author Adrien RICCIARDI
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "Network.h"

//-------------------------------------------------------------------------------------------------
// Public functions
//-------------------------------------------------------------------------------------------------
int NetworkCreateServer(unsigned short Port)
{
	int Socket, Is_Enabled = 1;
	struct sockaddr_in Address;
	
	// Create the server socket
	Socket = socket(AF_INET, SOCK_STREAM, 0);
	if (Socket == -1)
	{
		printf("[%s] Error : failed to create the socket (%s).\n", __func__, strerror(errno));
		return -1;
	}
	
	// Configure the socket to allow reusing the address without waiting
	setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &Is_Enabled, sizeof(Is_Enabled));
	
	// Fill the address
	Address.sin_family = AF_INET;
	Address.sin_port = htons(Port);
	Address.sin_addr.s_addr = INADDR_ANY;
	// Bind the socket to the address
	if (bind(Socket, (const struct sockaddr *) &Address, sizeof(Address)) == -1)
	{
		printf("[%s] Error : failed to bind the socket (%s).\n", __func__, strerror(errno));
		close(Socket);
		return -1;
	}
	
	return Socket;
}

int NetworkWaitForClient(int Server_Socket)
{
	int Client_Socket;
	
	// Set the number of clients to wait for
	if (listen(Server_Socket, 1) == -1)
	{
		printf("[%s] Error : listen() failed (%s).\n", __func__, strerror(errno));
		return -1;
	}
	
	// Wait for a client
	Client_Socket = accept(Server_Socket, NULL, NULL);
	if (Client_Socket == -1)
	{
		printf("[%s] Error : accept() failed (%s).\n", __func__, strerror(errno));
		return -1;
	}

	return Client_Socket;
}