/** @file Main.c
 * Allow to easily share files by sending a simple URL to the receiver (like the woof program).
 * @author Adrien RICCIARDI
 */
#include <errno.h>
#include <fcntl.h>
#include <features.h> // Needed by _FILE_OFFSET_BITS macro defined in the makefile so it is available before the headers are included
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "Network.h"

//-------------------------------------------------------------------------------------------------
// Private constants
//-------------------------------------------------------------------------------------------------
/** The server default binding port. */
#define SERVER_DEFAULT_BINDING_PORT 8080

//-------------------------------------------------------------------------------------------------
// Private variables
//-------------------------------------------------------------------------------------------------
/** The HTTP line buffer. */
static char String_Buffer[1024 * 1024]; // 1MB should be overkill

/** The buffer used to transfer the file content to the browser. */
static unsigned char Buffer[4096];

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
/** Read an HTTP line (terminated by CRLF).
 * @param File_Descriptor The file descriptor to read from.
 * @param String_Line On ouput, contain the read line. Make sure that the buffer is large enough to contain the whole line.
 * @return The read string length in characters (the terminating CRLF are replaced by a zero).
 */
static int HTTPReadLine(int File_Descriptor, char *String_Line)
{
	int i = 0;
	ssize_t Result;
	char Character;
	
	while (1)
	{
		// Read one byte
		Result = read(File_Descriptor, &Character, 1);
		if (Result < 0) printf("[%s] Error : failed to read from the file descriptor (%s).\n", __func__, strerror(errno));
		if (Result <= 0)
		{
			String_Line[i] = 0; // Append terminating zero
			return i;
		}
		
		// Are CRLF received ?
		if ((i > 0) && (String_Line[i - 1] == '\r') && (Character == '\n'))
		{
			// Put a terminating zero at CR location
			String_Line[i - 1] = 0;
			i--;
			return i;
		}
		else
		{
			// Append the character to the line
			String_Line[i] = Character;
			i++;
		}
	}
}

/** Read a whole HTTP request received from the browser.
 * @param File_Descriptor The file descriptor to read from.
 */
static void HTTPReadRequest(int File_Descriptor)
{
	int Length;
	
	// Read the browser "GET / HTTP/1.1" request
	if (HTTPReadLine(File_Descriptor, String_Buffer) == 0)
	{
		printf("Warning : the browser did not send an HTTP GET request.\n");
		return;
	}
	
	// Discard all remaining lines (like Host or User-Agent)
	do
	{
		Length = HTTPReadLine(File_Descriptor, String_Buffer);
	} while (Length > 0);
}

/** Create the HTTP answer to the HTTP GET / request.
 * @param String_File_Path The file path.
 * @param String_Answer On output, contain the whole answer to send to the browser.
 */
static void HTTPCreateRootGetAnswer(char *String_File_Path, char *String_Answer)
{
	int Length, i;
	char *String_File_Name;
	
	// Extract the file name from the path
	Length = strlen(String_File_Path);
	// Go in reverse order until a '/' is found or the string is finished
	for (i = Length - 1; i >= 0; i--)
	{
		if (String_File_Path[i] == '/') break;
	}
	// Was the file name beginning found ?
	if (i >= 0) String_File_Name = &String_File_Path[i + 1]; // +1 to bypass the '/' character
	else String_File_Name = String_File_Path;

	sprintf(String_Answer, "HTTP/1.0 302 Found\r\n"
		"Server: HTTP File Sharing\r\n"
		"Location: /%s\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: 117\r\n\r\n"
		"<html>\n"
		"  <head>HTTP File Sharing by Adrien RICCIARDI</head>\n"
		"  <body>\n"
		"    <p>Downloading file...</p>\n"
		"  </body>\n"
		"</html>", String_File_Name);
}

/** Create the HTTP answer to the HTTP GET /File_Name request.
 * @param String_File_Path The file path.
 * @param String_Answer On output, contain the whole answer to send to the browser.
 * @return -1 if an error occurred,
 * @return 0 if the function succeeded.
 */
static int HTTPCreateFileGetAnswer(const char *String_File_Path, char *String_Answer)
{
	struct stat File_Status;
	
	// Retrieve the file size
	if (stat(String_File_Path, &File_Status) == -1)
	{
		printf("[%s] Error : stat() failed (%s).\n", __func__, strerror(errno));
		return -1;
	}
	
	sprintf(String_Answer, "HTTP/1.0 200 OK\r\n"
		"Server: HTTP File Sharing\r\n"
		"Content-Type: application/octet-stream\r\n"
		"Content-Length: %llu\r\n\r\n", (unsigned long long) File_Status.st_size);
	
	return 0;
}

//-------------------------------------------------------------------------------------------------
// Entry point
//-------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	char *String_File_Path, String_Server_IP_Address[33];
	int File_Descriptor = -1, Server_Socket = -1, Client_Socket = -1, Return_Value = EXIT_FAILURE, Size;

	// Check parameters
	if (argc != 2)
	{
		printf("Error : bad parameters.\nUsage : %s File_To_Send\n", argv[0]);
		goto Exit;
	}
	String_File_Path = argv[1];
	
	// Try to open the file
	File_Descriptor = open(String_File_Path, O_RDONLY);
	if (File_Descriptor == -1)
	{
		printf("Error : failed to open the file '%s' (%s).\n", String_File_Path, strerror(errno));
		goto Exit;
	}
	
	// Start a server
	Server_Socket = NetworkCreateServer(SERVER_DEFAULT_BINDING_PORT);
	if (Server_Socket == -1) goto Exit;
	
	// Display the downloading URL
	if (NetworkGetIPAddress(String_Server_IP_Address) == -1) goto Exit;
	printf("Downloading URL : http://%s:%d\n", String_Server_IP_Address, SERVER_DEFAULT_BINDING_PORT);

	// Wait for a client to connect
	printf("Waiting for a client...\n");
	Client_Socket = NetworkWaitForClient(Server_Socket);
	if (Client_Socket == -1) goto Exit;
	printf("Client connected.\n");
	
	// Read the browser "GET / HTTP/1.1" request
	HTTPReadRequest(Client_Socket);
	
	// Send an HTML answer redirecting to the file to download
	HTTPCreateRootGetAnswer(String_File_Path, String_Buffer);
	Size = strlen(String_Buffer); // Cache the buffer length
	if (write(Client_Socket, String_Buffer, Size) < Size)
	{
		printf("Error : failed to send the HTTP GET / answer to the browser.\n");
		goto Exit;
	}
	
	// The browser will now close the connection and open a new one to get the file
	close(Client_Socket);
	Client_Socket = NetworkWaitForClient(Server_Socket);
	if (Client_Socket == -1) goto Exit;
	
	// Read the browser "GET /<file name> HTTP/1.1" request
	HTTPReadRequest(Client_Socket);
	
	// Send an HTML answer specifying the file to download
	HTTPCreateFileGetAnswer(String_File_Path, String_Buffer);
	Size = strlen(String_Buffer); // Cache the buffer length
	if (write(Client_Socket, String_Buffer, Size) < Size)
	{
		printf("Error : failed to send the HTTP GET /File_Name answer to the browser.\n");
		goto Exit;
	}
	
	// Send the file content
	printf("Sending file...\n");
	while (1)
	{
		// Read a block of data from the file
		Size = read(File_Descriptor, Buffer, sizeof(Buffer));
		if (Size < 0)
		{
			printf("Error when reading the file content (%s).\n", strerror(errno));
			goto Exit;
		}
		else if (Size == 0) break;
		
		// Send it to the browser
		if (write(Client_Socket, Buffer, Size) < Size)
		{
			printf("Error when sending the file content to the browser (%s).\n", strerror(errno));
			goto Exit;
		}
	}
	printf("File successfully sent.\n");
	
	Return_Value = EXIT_SUCCESS;
	
Exit:
	if (File_Descriptor != -1) close(File_Descriptor);
	if (Client_Socket != -1) close(Client_Socket);
	if (Server_Socket != -1) close(Server_Socket);
	return Return_Value;
}
