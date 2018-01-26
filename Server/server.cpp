#include <stdio.h>
#include <iostream>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "../pthreads/include/pthread.h"

#define SERVER_PORT 12345
#define BUFFER_SIZE 4096
#define QUEUE_SIZE 10

#define EXIT_COMMAND "q\0"
#define EXIT_COMMAND2 "Q\0"


// ------------------------------------------------------------
// ThreadData
// Structure used for working with threads
// ------------------------------------------------------------
struct ThreadData
{
	sockaddr_in client;
	SOCKET socket;

	char ipClient[INET_ADDRSTRLEN];
	char sendBuffer[BUFFER_SIZE];
	char receiveBuffer[BUFFER_SIZE];
	char clientNickname[BUFFER_SIZE];
};


// ------------------------------------------------------------
// Global variables
// ------------------------------------------------------------
std::vector<SOCKET> clientList;
pthread_mutex_t clientListMutex;


// ------------------------------------------------------------
// Function declarations
// ------------------------------------------------------------
void* ManageClient(void* data);
void DisconnectClient(SOCKET socket);
void BroadcastMessage(ThreadData* data);
bool IsReceivedMessageValid(int bytesReceived);
bool IsReceivedMessageADisconnectionRequest(ThreadData* data);


// ------------------------------------------------------------
// main
// Starts the server and enters in the main server loop. If
// a connection is received, a new thread is started for
// managing that connection
// ------------------------------------------------------------
int main(int argc, char* argv[])
{
	// Thread list
	std::vector<pthread_t> threadList;

	WSADATA wsaData;
	int wsaError;
	SOCKET socketID, socketClientID;
	int on = 1;
	
	struct sockaddr_in address;
	struct sockaddr_in client;

	char buffer[BUFFER_SIZE];

	wsaError = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaError != 0)
	{
		printf("Could not initialize Winsock\n");
		return 0;
	}

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(SERVER_PORT);

	// Create socket
	socketID = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketID < 0)
	{
		printf("Error creating socket\n");
		WSACleanup();
		return -1;
	}

	setsockopt(socketID, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));

	// Bind port
	if (bind(socketID, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		printf("Error binding port\n");
		WSACleanup();
		return -1;
	}

	// Listen to socket
	if (listen(socketID, QUEUE_SIZE) < 0)
	{
		printf("Listen failed\n");
		WSACleanup();
		return -1;
	}

	printf("\n\n");
	printf("---------- CHAT SERVER ----------");
	printf("\n\n");

	clientListMutex = PTHREAD_MUTEX_INITIALIZER;

	// Server main loop
	while (1)
	{
		socklen_t sock_len = sizeof(client);
		memset(&client, 0, sizeof(client));
		socketClientID = accept(socketID, (struct sockaddr*)&client, &sock_len);

		if (socketClientID == INVALID_SOCKET)
		{
			printf("Can't accept client: %d\n", WSAGetLastError());
		}
		else
		{
			ThreadData* threadData = new ThreadData();
			pthread_t thread;

			threadData->client = client;
			threadData->socket = socketClientID;

			// Lock mutex
			pthread_mutex_lock(&clientListMutex);

			threadList.push_back(thread);
			clientList.push_back(socketClientID);

			// Unlock mutex
			pthread_mutex_unlock(&clientListMutex);

			pthread_create(&thread, NULL, ManageClient, static_cast<void*>(threadData));
		}
	}

	closesocket(socketID);
	WSACleanup();
	return 0;
}


// ------------------------------------------------------------
// ManageClient
// Manages a client connection. First, it will ask the client
// for a nickname, and then will start receiving messages
// and broadcasting them to all other clients
// ------------------------------------------------------------
void* ManageClient(void* data)
{
	ThreadData* threadData = static_cast<ThreadData*>(data);
	int bytesReceived = 0;

	inet_ntop(AF_INET, &threadData->client.sin_addr, threadData->ipClient, sizeof(threadData->ipClient));

	// Ask for nickname
	strcpy_s(threadData->sendBuffer, "[SERVER]: Please enter your nickname: ");
	send(threadData->socket, threadData->sendBuffer, strlen(threadData->sendBuffer) + 1, 0);

	// Receive nickname
	bytesReceived = recv(threadData->socket, threadData->receiveBuffer, BUFFER_SIZE, 0);
	if (IsReceivedMessageValid(bytesReceived))
	{
		strcpy_s(threadData->clientNickname, threadData->receiveBuffer);
		printf(">%s has joined!\n", threadData->clientNickname);
	}

	// Send welcome message to client
	strcpy_s(threadData->sendBuffer, "[SERVER]: Welcome to the chat, ");
	strcat_s(threadData->sendBuffer, threadData->clientNickname);
	send(threadData->socket, threadData->sendBuffer, strlen(threadData->sendBuffer) + 1, 0);

	// Notify other clients that a new client has connected
	strcpy_s(threadData->sendBuffer, "[SERVER]: ");
	strcat_s(threadData->sendBuffer, threadData->clientNickname);
	strcat_s(threadData->sendBuffer, " has joined!");

	BroadcastMessage(threadData);

	// Main loop
	while (1)
	{
		// Reset buffers
		memset(&threadData->sendBuffer, 0, sizeof(threadData->sendBuffer));
		memset(&threadData->receiveBuffer, 0, sizeof(threadData->receiveBuffer));

		// Receive message from client
		bytesReceived = recv(threadData->socket, threadData->receiveBuffer, BUFFER_SIZE, 0);

		// Valid message
		if (IsReceivedMessageValid(bytesReceived))
		{
			// Disconnection request
			if (IsReceivedMessageADisconnectionRequest(threadData))
			{
				printf("%s has left\n", threadData->clientNickname);
				closesocket(threadData->socket);

				strcpy_s(threadData->sendBuffer, "[SERVER]: ");
				strcat_s(threadData->sendBuffer, threadData->clientNickname);
				strcat_s(threadData->sendBuffer, " has left");

				BroadcastMessage(threadData);

				DisconnectClient(threadData->socket);

				pthread_exit(0);
				return 0;
			}
			else
			{
				printf(">[%s]: %s\n", threadData->clientNickname, threadData->receiveBuffer);

				strcpy_s(threadData->sendBuffer, "[");
				strcat_s(threadData->sendBuffer, threadData->clientNickname);
				strcat_s(threadData->sendBuffer, "]: ");
				strcat_s(threadData->sendBuffer, threadData->receiveBuffer);

				BroadcastMessage(threadData);
			}
		}
		// Not valid message (connection error)
		else
		{
			printf("Connection error, %s has left\n", threadData->clientNickname);
			closesocket(threadData->socket);

			strcpy_s(threadData->sendBuffer, "[SERVER]: ");
			strcat_s(threadData->sendBuffer, threadData->clientNickname);
			strcat_s(threadData->sendBuffer, " has left due to a connection error");

			BroadcastMessage(threadData);

			DisconnectClient(threadData->socket);

			pthread_exit(0);
			return 0;
		}
	}

	pthread_exit(0);
	return 0;
}


// ------------------------------------------------------------
// DisconnectClient
// Searches for the received socket in the clients list, and
// erases it if found
// ------------------------------------------------------------
void DisconnectClient(SOCKET socket)
{
	// Lock mutex
	pthread_mutex_lock(&clientListMutex);

	// Delete client from list
	for (std::vector<SOCKET>::iterator it = clientList.begin(); it != clientList.end(); ++it)
	{
		if (*it != socket)
		{
			clientList.erase(it);
			break;
		}
	}

	// Unlock mutex
	pthread_mutex_unlock(&clientListMutex);
}


// ------------------------------------------------------------
// BroadcastMessage
// Sends the message to every other client
// ------------------------------------------------------------
void BroadcastMessage(ThreadData* data)
{
	// Lock mutex
	pthread_mutex_lock(&clientListMutex);

	// Broadcast message
	for (std::vector<SOCKET>::iterator it = clientList.begin(); it != clientList.end(); ++it)
	{
		if (*it != data->socket)
		{
			send(*it, data->sendBuffer, strlen(data->sendBuffer) + 1, 0);
		}
	}

	// Unlock mutex
	pthread_mutex_unlock(&clientListMutex);
}


// ------------------------------------------------------------
// IsReceivedMessageValid
// Checks if received bytes are greater than 0. If not, there
// has been a connection error
// ------------------------------------------------------------
bool IsReceivedMessageValid(int bytesReceived)
{
	return bytesReceived > 0;
}


// ------------------------------------------------------------
// IsReceivedMessageADisconnectionRequest
// Checks if the received message is an exit command
// ------------------------------------------------------------
bool IsReceivedMessageADisconnectionRequest(ThreadData* data)
{
	return strcmp(data->receiveBuffer, EXIT_COMMAND) == 0 || strcmp(data->receiveBuffer, EXIT_COMMAND2) == 0;
}