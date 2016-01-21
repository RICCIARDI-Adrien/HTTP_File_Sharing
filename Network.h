/** @file Network.h
 * Gather all needed network functions.
 * @author Adrien RICCIARDI
 */
#ifndef H_NETWORK_H
#define H_NETWORK_H

//-------------------------------------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------------------------------------
/** Create a TCP server bound on all interfaces and the provided port.
 * @param Port The port to bind the server onto.
 * @return -1 if an error occurred,
 * @return a positive number corresponding to the server socket if the function succeeded.
 */
int NetworkCreateServer(unsigned short Port);

/** Wait for a single client connection on the previously bound socket.
 * @param Server_Socket The server socket.
 * @return -1 if an error occurred,
 * @return a positive number corresponding to the client socket if the function succeeded.
 */
int NetworkWaitForClient(int Server_Socket);

/** Retrieve the IP address of the main network interface.
 * @param String_IP_Address On output, contain the IPv4 address converted to dotted format.
 * @return -1 if an error occurred,
 * @return 0 if the function succeeded.
 */
int NetworkGetIPAddress(char *String_IP_Address);

#endif
