#include <stdio.h>
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "../pthreads/include/pthread.h"

#define SERVER_PORT "12345"
#define BUFFER_SIZE 4096

#define EXIT_COMMAND "q\0"
#define EXIT_COMMAND2 "Q\0"


// ------------------------------------------------------------
// Global variables
// ------------------------------------------------------------
bool connected = false;
char sendBuffer[BUFFER_SIZE] = "";
char receiveBuffer[BUFFER_SIZE] = "";
char clientNickname[BUFFER_SIZE] = "";


// ------------------------------------------------------------
// Function declarations
// ------------------------------------------------------------
void SendMessageToServer(SOCKET socketfd);
void* ReceiveMessageFromServer(void* socketfd);
bool IsSendMessageValid();
bool IsSendMessageADisconnectionRequest();


// ------------------------------------------------------------
// main
// Connects to server and starts a thread for receiving 
// messages from the server
// ------------------------------------------------------------
int main(int argc, char* argv[])
{
	WSADATA wsaData;
	int wsaError;

	SOCKET socketfd;

	if (argc < 2)
	{
		return -1;
	}

	wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaError != 0)
	{
		printf("Could not initialize Winsock\n");
		return 0;
	}

	struct addrinfo hints;
	struct addrinfo* serverInfo;
	struct addrinfo* serverAddress;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int error = getaddrinfo(argv[1], SERVER_PORT, &hints, &serverInfo);
	if (error != 0)
	{
		printf("getaddrinfo failed");
		WSACleanup();
		return -1;
	}

	// Connect to server
	for (serverAddress = serverInfo; (serverAddress != NULL); serverAddress = serverAddress->ai_next)
	{
		socketfd = socket(serverAddress->ai_family, serverAddress->ai_socktype, serverAddress->ai_protocol);

		if (socketfd != INVALID_SOCKET)
		{
			int iResult = connect(socketfd, serverAddress->ai_addr, (int)serverAddress->ai_addrlen);

			if (iResult == SOCKET_ERROR)
			{
				closesocket(socketfd);
				socketfd = INVALID_SOCKET;
			}
			else
			{
				break;
			}
		}
	}

	freeaddrinfo(serverInfo);

	// Communicate with server
	if (socketfd != INVALID_SOCKET)
	{
		connected = true;

		printf("\n\n");
		printf("---------- CHAT CLIENT ----------");
		printf("\n\n");

		// Receive nickname request from server
		recv(socketfd, receiveBuffer, BUFFER_SIZE, 0);
		printf(">%s", receiveBuffer);
		memset(&receiveBuffer, 0, sizeof(receiveBuffer));

		// Send nickname to server
		gets_s(sendBuffer, _countof(sendBuffer));
		if (IsSendMessageValid())
		{
			strcpy_s(clientNickname, sendBuffer);
			send(socketfd, sendBuffer, strlen(sendBuffer) + 1, 0);
		}

		// Receive welcome message from server
		recv(socketfd, receiveBuffer, BUFFER_SIZE, 0);
		printf(">%s\n", receiveBuffer);

		// Create thread for receiving messages
		pthread_t thread;
		pthread_create(&thread, NULL, ReceiveMessageFromServer, reinterpret_cast<void*>(socketfd));

		while (connected)
		{
			SendMessageToServer(socketfd);
		}

		closesocket(socketfd);
	}

	WSACleanup();
	return 0;
}


// ------------------------------------------------------------
// SendMessageToServer
// Receives input from keyboard and sends it to the server
// If the input is an exit command, client disconnects
// ------------------------------------------------------------
void SendMessageToServer(SOCKET socketfd)
{
	// Reset variables
	memset(&sendBuffer, 0, sizeof(sendBuffer));
	memset(&receiveBuffer, 0, sizeof(receiveBuffer));

	// Receive input from keyboard
	printf(">[%s]: ", clientNickname);
	gets_s(sendBuffer, _countof(sendBuffer));

	// Send message to server
	if (IsSendMessageValid())
	{
		send(socketfd, sendBuffer, strlen(sendBuffer) + 1, 0);
	}

	// Check if client is disconnecting
	if (IsSendMessageADisconnectionRequest())
	{
		printf(">Disconnecting...\n\n");
		connected = false;
	}
}


// ------------------------------------------------------------
// ReceiveMessageFromServer
// While the client is connected, it will receive messages
// from server. In case of receiving 0 or less bytes, it will
// disconnect (connection error)
// ------------------------------------------------------------
void* ReceiveMessageFromServer(void* socketfd)
{
	int bytesReceived = 0;

	while (connected)
	{
		bytesReceived = recv(reinterpret_cast<SOCKET>(socketfd), receiveBuffer, BUFFER_SIZE, 0);
	
		// Disconnect on error
		if (bytesReceived <= 0)
		{
			printf(">Connection error, disconnecting...\n\n");
			connected = false;

			pthread_exit(0);
			return 0;
		}

		// Print received message
		printf("\r>%s\n", receiveBuffer);
		printf(">[%s]: ", clientNickname);
	}
}


// ------------------------------------------------------------
// IsSendMessageValid
// Checks if the message to send is not empty
// ------------------------------------------------------------
bool IsSendMessageValid()
{
	return sendBuffer[0] != '\0';
}


// ------------------------------------------------------------
// IsSendMessageADisconnectionRequest
// Checks if the message to send is an exit command
// ------------------------------------------------------------
bool IsSendMessageADisconnectionRequest()
{
	return strcmp(sendBuffer, EXIT_COMMAND) == 0 || strcmp(sendBuffer, EXIT_COMMAND2) == 0;
}