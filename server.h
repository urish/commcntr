#define assert(expression) \
    ((expression) ? (void)0 : assertion_failed(__FILE__, __FUNCTION__, __LINE__, #expression))

extern void assertion_failed (char*, char*, int, char*);
extern void init_server (int);
extern void init_users (void);
extern void start_server (void);
extern void stop_server (void);
extern void disconnect_client (int);
extern char *getipbysock (int, char*);
int server_uptime;
