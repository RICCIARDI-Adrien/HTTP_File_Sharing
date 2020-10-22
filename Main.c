/** @file Main.c
 * Allow to easily share files by sending a simple URL to the receiver (like the woof program).
 * @author Adrien RICCIARDI
 */
#include <errno.h>
#include <fcntl.h>
#ifndef __APPLE__ // The following file is not present on macOS
	#include <features.h> // Needed by _FILE_OFFSET_BITS macro defined in the makefile so it is available before the headers are included
#endif
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
 * @param Pointer_String_File_Path The file path.
 * @param Pointer_String_Answer On output, contain the whole answer to send to the browser.
 * @param Pointer_File_Size On output, contain the file to send size in bytes.
 * @return -1 if an error occurred,
 * @return 0 if the function succeeded.
 */
static int HTTPCreateFileGetAnswer(const char *Pointer_String_File_Path, char *Pointer_String_Answer, unsigned long long *Pointer_File_Size)
{
	struct stat File_Status;
	
	// Retrieve the file size
	if (stat(Pointer_String_File_Path, &File_Status) == -1)
	{
		printf("[%s] Error : stat() failed (%s).\n", __FUNCTION__, strerror(errno));
		return -1;
	}
	*Pointer_File_Size = File_Status.st_size;
	
	sprintf(Pointer_String_Answer, "HTTP/1.0 200 OK\r\n"
		"Server: HTTP File Sharing\r\n"
		"Content-Type: application/octet-stream\r\n"
		"Content-Length: %llu\r\n\r\n", (unsigned long long) File_Status.st_size);
	
	return 0;
}

/** Display the usage string. */
static void MainDisplayProgramUsage(char *Pointer_String_Program_Name)
{
	printf("Usage : %s [-h | --help] [-k] [-p Port] File_To_Send\n"
		"  -h,--help : display this help.\n"
		"  -k : keep serving the same file, do not exit after the first download. Use Ctrl+C to quit.\n"
		"  -p Port : specify the port the server will bind to.\n"
		"  File_To_Send : the file the server will send.\n", Pointer_String_Program_Name);
}

//-------------------------------------------------------------------------------------------------
// Entry point
//-------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	char *Pointer_String_File_Path = NULL, String_Server_IP_Address[33];
	int i, File_Descriptor = -1, Server_Socket = -1, Client_Socket = -1, Size, Server_Port = SERVER_DEFAULT_BINDING_PORT, Is_Multiple_Downloads_Enabled = 0, Previous_Percentage, New_Percentage;
	unsigned long long File_Size, Sent_File_Bytes_Count;
	
	// Display banner
	printf("+--------------------------------+\n");
	printf("|       HTTP file sharing        |\n");
	printf("| (C) 2015-2020 Adrien RICCIARDI |\n");
	printf("+--------------------------------+\n\n");

	// Check parameters
	for (i = 1; i < argc; i++)
	{
		// Handle "help" argument
		if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
		{
			MainDisplayProgramUsage(argv[0]);
			return EXIT_SUCCESS;
		}
		// Handle "keep going" argument
		else if (strcmp(argv[i], "-k") == 0) Is_Multiple_Downloads_Enabled = 1;
		// Handle "set server port" argument
		else if (strcmp(argv[i], "-p") == 0)
		{
			// Is another argument following this one ?
			if (i == argc - 1) // This is the last command-line argument
			{
				printf("Error : port number is missing after -p command-line argument.\n");
				goto Exit_Error;
			}
			
			// Retrieve port value
			if (sscanf(argv[i + 1], "%d", &Server_Port) != 1)
			{
				printf("Error : invalid port number.\n");
				goto Exit_Error;
			}
			if ((Server_Port < 0) || (Server_Port > 65535))
			{
				printf("Error : port value must be within 0 and 65535.\n");
				goto Exit_Error;
			}
			
			// Bypass next argument as it was the port number
			i++;
		}
		// Handle "file to send" argument
		else Pointer_String_File_Path = argv[i];
	}
	
	// Make sure a file has been provided
	if (Pointer_String_File_Path == NULL)
	{
		MainDisplayProgramUsage(argv[0]);
		goto Exit_Error;
	}
	
	// Create server
	do
	{
		// Reset needed variables in case looping is enabled
		Previous_Percentage = -1; // Set previous percentage to an invalid value to make sure it is displayed the first time
		Sent_File_Bytes_Count = 0;
		
		// Try to open the file
		File_Descriptor = open(Pointer_String_File_Path, O_RDONLY);
		if (File_Descriptor == -1)
		{
			printf("Error : failed to open the file '%s' (%s).\n", Pointer_String_File_Path, strerror(errno));
			goto Exit_Error;
		}
	
		// Start a server
		Server_Socket = NetworkCreateServer((unsigned short) Server_Port);
		if (Server_Socket == -1) goto Exit_Error;
	
		// Display the downloading URL
		if (NetworkGetIPAddress(String_Server_IP_Address) == -1) goto Exit_Error;
		printf("Downloading URL : http://%s:%d\n", String_Server_IP_Address, Server_Port);

		// Wait for a client to connect
		printf("Waiting for a client...\n");
		Client_Socket = NetworkWaitForClient(Server_Socket);
		if (Client_Socket == -1) goto Exit_Error;
		printf("Client connected.\n");
	
		// Read the browser "GET / HTTP/1.1" request
		HTTPReadRequest(Client_Socket);
	
		// Send an HTTP answer redirecting to the file to download
		HTTPCreateRootGetAnswer(Pointer_String_File_Path, String_Buffer);
		Size = strlen(String_Buffer); // Cache the buffer length
		if (write(Client_Socket, String_Buffer, Size) < Size)
		{
			printf("Error : failed to send the HTTP GET / answer to the browser.\n");
			goto Exit_Error;
		}
		
		// The browser will now close the connection and open a new one to get the file
		close(Client_Socket);
		Client_Socket = NetworkWaitForClient(Server_Socket);
		if (Client_Socket == -1) goto Exit_Error;
		
		// Read the browser "GET /<file name> HTTP/1.1" request
		HTTPReadRequest(Client_Socket);
		
		// Send an HTTP answer specifying the file to download
		HTTPCreateFileGetAnswer(Pointer_String_File_Path, String_Buffer, &File_Size);
		Size = strlen(String_Buffer); // Cache the buffer length
		if (write(Client_Socket, String_Buffer, Size) < Size)
		{
			printf("Error : failed to send the HTTP GET /File_Name answer to the browser.\n");
			goto Exit_Error;
		}
		
		// Send the file content
		do
		{
			// Read a block of data from the file
			Size = read(File_Descriptor, Buffer, sizeof(Buffer));
			if (Size < 0)
			{
				printf("\nError when reading the file content (%s).\n", strerror(errno));
				goto Exit_Error;
			}
			
			// Send it to the browser
			if (Size > 0)
			{
				if (write(Client_Socket, Buffer, Size) < Size)
				{
					printf("\nError when sending the file content to the browser (%s).\n", strerror(errno));
					goto Exit_Error;
				}
			}
			
			// Display sending percentage
			Sent_File_Bytes_Count += Size;
			New_Percentage = 100 * Sent_File_Bytes_Count / File_Size;
			if (New_Percentage != Previous_Percentage) // Display percentage only when it changes to avoid loosing performances with too many console prints
			{
				printf("Sending file... %d%%\r", New_Percentage);
				fflush(stdout);
				Previous_Percentage = New_Percentage;
			}
		} while (Size > 0);
		
		// Release all resources
		close(File_Descriptor);
		close(Client_Socket);
		close(Server_Socket);
		
		printf("\nFile successfully sent.\n");
	} while (Is_Multiple_Downloads_Enabled);
	
	// Everything went fine
	return EXIT_SUCCESS;
	
Exit_Error:
	if (File_Descriptor != -1) close(File_Descriptor);
	if (Client_Socket != -1) close(Client_Socket);
	if (Server_Socket != -1) close(Server_Socket);
	return EXIT_FAILURE;
}
