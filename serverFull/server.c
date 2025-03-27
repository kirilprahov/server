#include "WorkingPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>


#define BACKLOG 10
#define HTML_FILE "static/index.html"
int serverRunning = 1;
int sockfd;

void handle_request(int client_fd);
int start_server(struct addrinfo hints, struct addrinfo **res, char *port);
void handle_shutdown(int signum);
void send_headers(int fd, int code, const char *status, const char *type);
void send_400(int fd);
void send_403(int fd);
void send_404(int fd);
void send_405(int fd);

int main(int argc, char *argv[])
{
	int threadCount = sysconf(_SC_NPROCESSORS_ONLN);
	pthread_t th[threadCount];
	char *port = "8080";
	if (argc == 3 && strcmp(argv[1], "--http-port") == 0)
	{
		port = argv[2];
		printf("Starting server on port %s\n", port);
	}
	else
	{
		printf("Starting server on port 8080\n");
	}

	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	struct addrinfo hints, *res;
	int new_fd;

	signal(SIGINT, handle_shutdown);
	signal(SIGTERM, handle_shutdown);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (start_server(hints, &res, port) == -1)
	{
		perror("start_server failed");
		exit(1);
	}

	for (int i = 0; i < threadCount; i++)
	{
		if (pthread_create(&th[i], NULL, &startThread, NULL) != 0) {
			perror("Failed to create a thread");
		}
	}

	while (serverRunning)
	{
		addr_size = sizeof(their_addr);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1)
		{
			perror("accept failed");
			continue;
		}
		printf("Client conected\n");
		Task t = {
			.taskFunction = &handle_request,
			.client_fd = new_fd,
		};
		submitTask(t);
	}

	close(sockfd);
	return 0;
}
int start_server(struct addrinfo hints, struct addrinfo **res, char *port)
{
	if (getaddrinfo(NULL, port, &hints, res) != 0)
	{
		perror("getaddrinfo failed");
		return -1;
	}
	sockfd = socket((*res)->ai_family, (*res)->ai_socktype, (*res)->ai_protocol);
	if (sockfd == -1)
	{
		perror("socket failed");
		freeaddrinfo(*res);
		return -1;
	}
	if (bind(sockfd, (*res)->ai_addr, (*res)->ai_addrlen) == -1)
	{
		perror("bind failed");
		close(sockfd);
		freeaddrinfo(*res);
		return -1;
	}

	freeaddrinfo(*res);

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen failed");
		close(sockfd);
		return -1;
	}
	printf("Server is listening\n");
	return 0;
}

void handle_request(int client_fd) {
	printf("started to work\n");
	char buffer[4096];
	ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read <= 0) return;
	buffer[bytes_read] = '\0';


	char method[8], path[1024], version[16];
	if (sscanf(buffer, "%7s %1023s %15s", method, path, version) != 3) {
		send_400(client_fd);
		return;
	}


	if (strcmp(method, "GET") != 0) {
		send_405(client_fd);
		return;
	}


	char *req_path = path[0] == '/' ? path + 1 : path;
	if (strlen(req_path) == 0) req_path = "index.html";

	if (strstr(req_path, "..")) {
		send_403(client_fd);
		return;
	}
	char file_path[2048];
	snprintf(file_path, sizeof(file_path), "static/%s", req_path);

	FILE *file = fopen(file_path, "r");
	if (!file) {
		send_404(client_fd);
		return;
	}

	printf("sending\n");
	send_headers(client_fd, 200, "OK", "text/html");
	char file_buf[1024];
	size_t bytes;
	while ((bytes = fread(file_buf, 1, sizeof(file_buf), file)) > 0) {
		send(client_fd, file_buf, bytes, 0);
	}

	fclose(file);
	close(client_fd);

}

void handle_shutdown(int signum)
{
	(void)signum;
	serverRunning = 0;
	if (sockfd != -1) {
		close(sockfd);
		printf("[+] Server socket closed.\n");
	}

}

void send_headers(int fd, int code, const char *status, const char *type) {
	char header[256];
	snprintf(header, sizeof(header),
	         "HTTP/1.1 %d %s\r\n"
	         "Content-Type: %s\r\n"
	         "Connection: close\r\n"
	         "\r\n", code, status, type);
	send(fd, header, strlen(header), 0);
}

void send_400(int fd) {
	send_headers(fd, 400, "Bad Request", "text/plain");
	send(fd, "400 Bad Request\n", 16, 0);
}

void send_403(int fd) {
	send_headers(fd, 403, "Forbidden", "text/plain");
	send(fd, "403 Forbidden\n", 15, 0);
}

void send_404(int fd) {
	send_headers(fd, 404, "Not Found", "text/plain");
	send(fd, "404 Not Found\n", 15, 0);
}

void send_405(int fd) {
	send_headers(fd, 405, "Method Not Allowed", "text/plain");
	send(fd, "405 Method Not Allowed\n", 25, 0);
}