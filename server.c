#include "commcntr.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

int server_uptime;
int server_sock;
fd_set server_rfd;
#if IPv6
int server_sock6;
#endif IPv6

#define check_error(var, string)	\
    if (var < 0) {			\
	perror("[Error] " string);	\
	exit(1);			\
    }

char* getipbysock (int s, char *dst) {
    union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#if IPv6
	struct sockaddr_in6 sin6;
#endif /* IPv6 */
    } addr;
    int socksize;
    socksize = sizeof(addr);
    getpeername(s, &addr.sa, &socksize);
#if IPv6
    inet_ntop(addr.sa.sa_family, (addr.sa.sa_family == AF_INET ?
	      (void*)&addr.sin.sin_addr : (void*)&addr.sin6.sin6_addr), dst, 128);
#else /* IPv6 */
    strcpy(dst, inet_ntoa(addr.sin.sin_addr));
#endif /* IPv6 */
    return dst;
}

void assertion_failed (char *file, char *function, int line, char *assertion) {
    fprintf(stderr, "[Error] Assertion \"%s\" failed at function %s file %s:%d.\n", assertion, function, file, line);
    send_all((Client*)NULL, "MSG @SERVER Server Terminating: %s(): Assertion \"%s\" failed at %s:%d.", function, assertion, file, line);
    destroy_users("Server error: assertion failed.");
    abort();
}

void server_signal (int sig) {
    static int segvlevel = 0;
    switch (sig) {
	case SIGTERM:
	case SIGHUP:
	case SIGQUIT:
	    send_all((Client*)NULL, "MSG @SERVER Server terminating: recived signal %d.", sig);
	    destroy_users((char*)NULL);
	    stop_server();
	    exit(0);
	case SIGSEGV:
	case SIGBUS:
	switch (segvlevel++) {
	    case 0:
		fprintf(stderr, "[Error] %s. Core dumped.\n", sig == SIGSEGV ? "Segmentation fault" : "Bus error");
		send_all((Client*)NULL, "MSG @SERVER Server terminating: %s.", sig == SIGSEGV ? "Segmentation fault" : "Bus error");
		abort();
	    case 1:
		abort();
	}
    }
}

void init_server (int port) {
    int rc;
    struct sockaddr_in saddr;
#if IPv6
    struct sockaddr_in6 saddr6;
#endif
    server_uptime = time((time_t*)NULL);
    /* Create an ip socket, bind it, and listen */
    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    check_error(server_sock, "socket");
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = INADDR_ANY;
    rc = bind(server_sock, (struct sockaddr *)&saddr, sizeof(saddr));
    check_error(rc, "bind");
    rc = listen(server_sock, -1);
    check_error(rc, "listen");
    FD_SET(server_sock, &server_rfd);
#if IPv6
    /* Create an ipv6 socket, bind it, and listen */
    server_sock6 = socket(PF_INET6, SOCK_STREAM, 0);
    check_error(server_sock6, "ipv6: socket");
    saddr6.sin6_family = AF_INET6;
    saddr6.sin6_port = htons(port);
    saddr6.sin6_addr = in6addr_any;
    rc = bind(server_sock6, (struct sockaddr *)&saddr6, sizeof(saddr6));
    check_error(rc, "ipv6: bind");
    rc = listen(server_sock6, -1);
    check_error(rc, "ipv6: listen");
    FD_SET(server_sock6, &server_rfd);
#endif IPv6
    /* handle strange signals */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGSEGV, server_signal);
    signal(SIGBUS, server_signal);
    signal(SIGTERM, server_signal);
    signal(SIGHUP, server_signal);
    signal(SIGQUIT, server_signal);
}

void accept_connection (int sock) {
    struct sockaddr addr;
    int addrlen = sizeof(addr), s;
    s = accept(sock, &addr, &addrlen);
    fcntl(s, F_SETFL, O_NONBLOCK);
    FD_SET(s, &server_rfd);
    new_client(s);
}

void disconnect_client (int sock) {
    FD_CLR(sock, &server_rfd);
    close(sock);
}

void read_client_data (int sock) {
    char buf[BUFLEN+1], *sptr = buf, *ptr = buf;
    int rc;
    Client *client = find_client_by_s(sock);
    if (!client) {
	close(sock);
	return;
    }
    if (client->buf) {
	strcpy(buf, client->buf);
	ptr = &buf[strlen(buf)];
    }
    rc = read(sock, ptr, BUFLEN - ((int)ptr - (int)buf));
    if (rc < 1) {
	if (client->name)
	    send_all(client, "QUIT %s Read error %d: %s", client->name, errno, strerror(errno));
	remove_client(client);
	return;
    }
    ptr[rc] = (char)NULL;
    while (*ptr) {
	if (*ptr == '\r')
	    *ptr = (char)NULL;
        if (*ptr == '\n') {
	    *ptr = (char)NULL;
	    if (client->ignorelines)
		client->ignorelines--;
	    else
        	client_command(client, sptr);
	    sptr = ptr+1;
	}
        ptr++;
    }
    if ((sptr == buf) && ((int)ptr - (int)buf) == BUFLEN) {
        if (!client->ignorelines)
    	    send_client(client, "ERROR 108 %d line too long.", BUFLEN);
	*sptr = (char)NULL;
	client->ignorelines=1;
    }
    if (*sptr) {
	if (!client->buf) {
	    client->buf = (char*)malloc((BUFLEN + 1) * sizeof(char));
	    assert(client->buf);
	}
	strcpy(client->buf, sptr);
    } else if (client->buf) {
	free(client->buf);
	client->buf = (char*)NULL;
    }
}

void start_server () {
    fd_set o_rfd;
    int dtablesize = getdtablesize(), i;
    while (1) {
	o_rfd = server_rfd;
	select(dtablesize, &o_rfd, (fd_set*)NULL, (fd_set*)NULL, (struct timeval *)NULL);
	if (FD_ISSET(server_sock, &o_rfd)) {
	    accept_connection(server_sock);
	    continue;
	}
#if IPv6
	if (FD_ISSET(server_sock6, &o_rfd)) {
	    accept_connection(server_sock6);
	    continue;
	}
#endif IPv6
	for (i = 0; i < dtablesize; i++)
	    if (FD_ISSET(i, &o_rfd))
		read_client_data(i);
    }
}

void stop_server() {
    FD_CLR(server_sock, &server_rfd);
    close(server_sock);
#if IPv6
    FD_CLR(server_sock6, &server_rfd);
    close(server_sock6);
#endif
}
