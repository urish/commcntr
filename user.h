typedef struct client Client;

#define FLAG_ADMIN	0x2

struct client {
    Client *next;
    Client *prev;
    char *name;
    int lastcmdtime, connecttime;
    int flags;
    int s;
    int ignorelines;
    char *buf;
};

#define CMD_ALIAS 0x1
#define CMD_ADMIN 0x2

typedef struct cmdlist {
  char *name;
  int minmatch;
  void (*func)(Client*, char*);
  int flags;
  char *args, *desc;
} cmdlist;

extern Client *clients;
extern Client *find_client_by_s (int);
extern void new_client (int);
extern void remove_client (Client *);
extern void sockprintf(int, char*, ...);
extern void send_clients (int, Client*, char*, ...);
extern void client_command (Client*, char*);
extern void destroy_users (char*);

#define CHECK_ADMIN(x) (x->flags & FLAG_ADMIN)
#define USERCMD_FUNC(x) void x (Client *client, char *args)

#define send_client(client, fmt, args...) 	\
    sockprintf(client->s, fmt"\r\n" , ## args);
#define send_all(client, fmt, args...) 	\
    send_clients(0, client, fmt"\r\n" , ## args);
#define send_admins(fmt, args...) 	\
    send_clients(1, NULL, fmt"\r\n" , ## args);
