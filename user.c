#include <sys/time.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "commcntr.h"

Client *Clients = (Client*)NULL;

USERCMD_FUNC(cmd_global);
USERCMD_FUNC(cmd_msg);
USERCMD_FUNC(cmd_globcmd);
USERCMD_FUNC(cmd_privcmd);
USERCMD_FUNC(cmd_ping);
USERCMD_FUNC(cmd_pong);
USERCMD_FUNC(cmd_list);
USERCMD_FUNC(cmd_nick);
USERCMD_FUNC(cmd_quit);
USERCMD_FUNC(cmd_info);
USERCMD_FUNC(cmd_sinfo);
USERCMD_FUNC(cmd_admin);
USERCMD_FUNC(cmd_kill);
USERCMD_FUNC(cmd_die);
USERCMD_FUNC(cmd_version);
USERCMD_FUNC(cmd_protver);
USERCMD_FUNC(cmd_help);

cmdlist UserCommands [] = {
  {"GLOBAL",	1, cmd_global, 0, "<msg>", "send <msg> to all clients"},
  {"MSG",	1, cmd_msg, 0, "<client> <msg>", "send <client> message <msg>"},
  {"GCMD",	2, cmd_globcmd, 0, "<cmd>", "send <cmd> command to all clients"},
  {"CMD",	1, cmd_privcmd, 0, "<client> <cmd>", "send <cmd> command to <client>"},
  {"PING",	1, cmd_ping, 0, "<client> [msg]", "PING client WITH msg"},
  {"PONG",	2, cmd_pong, 0, "<client> [msg]", "reply a ping command"},
  {"LIST",	1, cmd_list, 0, "", "list online clients"},
  {"NICK",	1, cmd_nick, 0, "<newnick>", "change nick to <newnick>"},
  {"QUIT",	1, cmd_quit, 0, "[msg]", "leave the server"},
  {"INFO",	1, cmd_info, 0, "[client]", "get information about a client"},
  {"SINFO",	1, cmd_sinfo, 0, "", "get server information"},
  {"ADMIN",	1, cmd_admin, 0, "<password>", "get administration priviliges"},
  {"VERSION",	1, cmd_version, 0, "", "get server version"},
  {"PROTVER",	2, cmd_protver, 0, "", "get protocol version"},
  {"DIE",	1, cmd_die, CMD_ADMIN, "[reason]", "terminate the server"},
  {"KILL",	1, cmd_kill, CMD_ADMIN, "<client> <reason>", "kill a client"},
  {"?",		1, cmd_help, CMD_ALIAS, "", "get help"},
  {"HELP",	1, cmd_help, 0, "", "get help"},
};

void sockprintf(int s, char *fmt, ...) {
    va_list argp;
    char buf[BUFSIZ];
    va_start(argp, fmt);
    vsnprintf(buf, sizeof(buf), fmt, argp);
    va_end(argp);
    send(s, buf, strlen(buf), 0);
}

void send_clients (int admins, Client* client, char* fmt, ...) {
    Client *i;
    va_list argp;
    char buf[BUFSIZ];
    int buflen;
    va_start(argp, fmt);
    vsnprintf(buf, sizeof(buf)-2, fmt, argp);
    va_end(argp);
    buflen = strlen(buf);
    for (i = Clients; i; i = i->next)
	if (i != client && i->name && (CHECK_ADMIN(i) || !admins))
	    send(i->s, buf, buflen, 0);
}

int is_valid_nick (unsigned char *nick) {
    int i = strlen(nick);
    if (i < 2 || i > MAX_NICK_LENGTH)
	return 0;
    for (; *nick && *nick > 32 && *nick < 128 && *nick != 64; nick++);
    return !(*nick);
}

void new_client (int s) {
    Client *client = (Client*)malloc(sizeof(Client));
    struct timeval curtime;
    assert(client);
    client->prev = (Client*)NULL;
    if (Clients)
	Clients->prev = client;
    client->next = Clients;
    Clients = client;
    client->s = s;
    client->name = (char*)NULL;
    client->flags = 0;
    client->ignorelines = 0;
    client->buf = (char*)NULL;
    gettimeofday(&curtime, (struct timezone *)NULL);
    client->connecttime = curtime.tv_sec;
}

Client *find_client_by_s(int s) {
    Client *current;
    for (current = Clients; current; current = current->next)
	if (current->s == s) return current;
    return (Client*)NULL;
}

Client *find_client_by_name(char *name) {
    Client *current;
    for (current=Clients; current; current = current->next)
	if (current->name)
	    if (strcasecmp(current->name,name) == 0)
		return current;
    return (Client*)NULL;
}

void remove_client (Client *client) {
    if (client == Clients)
	Clients = client->next;
    if (client->prev)
	client->prev->next = client->next;
    if (client->next)
	client->next->prev = client->prev;
    if (client->buf)
	free(client->buf);
    if (client->name)
	free(client->name);
    disconnect_client(client->s);
    free(client);
}

void destroy_users (char *reason) {
    Client *i, *next;
    for (i = Clients; i; i = next) {
	if (i->name)
	    free(i->name);
	next = i->next;
	if (reason) 
	    send_client(i, "BYE %s", reason);
	disconnect_client(i->s);
	free(i);
    }
} 

int strcountcasecmp (char *str, char *str2) {
    int count;
    for (count = 0; toupper(str[count]) == toupper(str2[count]) && str[count]; count++);
    return str2[count] ? 0 : count;
}

void client_command (Client *client, char* buf) {
    struct timeval curtime;
    char *cmd;
    int i;
    if (!client) return;
    gettimeofday(&curtime, (struct timezone *)NULL);
    client->lastcmdtime = curtime.tv_sec;
    cmd = strsep(&buf, "\t ");
    if (!buf)
	buf = "";
    for (i = 0; i < sizeof(UserCommands) / sizeof(UserCommands[0]); i++)
	if (strcountcasecmp(UserCommands[i].name, cmd) >= UserCommands[i].minmatch) {
    	    UserCommands[i].func(client, buf);
	    return;
        }
    send_client(client, "ERROR 103 %s invalid command.", cmd);
}

/* commands */
#define check_register(x) \
	if (!x->name) { \
	    send_client(x, "ERROR 100 You are not registered."); \
	    return;	\
	}

#define check_admin(x) \
	if (!CHECK_ADMIN(x)) { \
	    send_client(client, "ERROR 107 Permission denied."); \
	    if (x->name) \
		send_admins("MSG @SERVER Client %s tried to use admin-only command.", x->name);\
	    return; 	\
	}
	    

#define check_args(client, x, y, z) \
	if (!(x)) { \
	    send_client(client, "ERROR 101 %s Not enough args.", z); \
	    return; \
	} \
	if (!(y)) { \
	    send_client(client, "ERROR 102 %s too many args.", z); \
	    return; \
	}

void cmd_client2client(char *cmd, Client *client, char *args) {
    Client* tmp;
    char *user;
    user = strsep(&args, " \t");
    check_register(client);
    check_args(client, *user && args, 1, cmd);
    if ((strcmp(user, "@SERVER") == 0 || strcmp(user, "@") == 0) 
	 && strcmp(cmd, "PING") == 0) {
	send_client(client, "PONG @SERVER %s", args);
	return;
    }
    if (!(tmp = find_client_by_name (user))) {
	send_client(client, "ERROR 104 %s no such user.", user);
	return;
    }
    send_client(tmp, "%s %s %s", cmd, client->name, args);
}

USERCMD_FUNC(cmd_ping) {
    cmd_client2client("PING", client, args);
}

USERCMD_FUNC(cmd_pong) {
    cmd_client2client("PONG", client, args);
}

USERCMD_FUNC(cmd_msg) {
    cmd_client2client("MSG", client, args);
}

USERCMD_FUNC(cmd_privcmd) {
    cmd_client2client("CMD", client, args);
}

USERCMD_FUNC(cmd_quit) {
    send_client(client, "BYE");
    if (client->name) 
	send_all(client, "QUIT %s %s", client->name, *args?args:"Client exited.");
    remove_client(client);
}

USERCMD_FUNC(cmd_admin) {
    check_register(client);
    check_args(client, *args, 1, "ADMIN");
    if (strcmp(args, admin_pass)) {
	send_client(client, "ERROR 107 Permission denied.");
	send_admins("MSG @SERVER Client %s to identify for admin, password: %s.", client->name, args);
	return;
    }
    client->flags |= FLAG_ADMIN;
    send_client(client, "ADMIN 200 You are now a system administrator.");
}

USERCMD_FUNC(cmd_die) {
    check_register(client);
    check_admin(client);
    send_admins("MSG @SERVER Server terminated by %s (%s).", 
			        client->name, args?args:"no reason");
    destroy_users(args);
    stop_server();
    exit(0);
}

USERCMD_FUNC(cmd_kill) {
    Client *tmp;
    char *nick;
    check_register(client);
    check_admin(client);
    nick = strsep(&args, " \t");
    check_args(client, *nick && args, 1, "KILL");
    if (!(tmp = find_client_by_name (nick))) {
	send_client(client, "ERROR 104 %s no such user.", nick);
	return;
    }
    send_client(tmp, "BYE Killed by %s (%s)", client->name, args);
    if (tmp->name) 
	send_all(tmp, "QUIT %s %s", tmp->name, args);
    send_admins("MSG @SERVER Kill: %s killed %s (%s)", client->name, tmp->name,
		args);
    remove_client(tmp);
}

USERCMD_FUNC(cmd_nick) {
    char *newnick;
    Client *tmp;
    newnick = strsep(&args, " \t");
    check_args(client, *newnick, !args, "NICK");
    if (!is_valid_nick(newnick)) {
	send_client(client, "ERROR 105 %s Invalid nick.", newnick);
	return;
    }
    tmp = find_client_by_name(newnick);
    if (tmp && tmp != client) {
	send_client(client, "ERROR 106 %s Nick is already in use.", newnick);
	return;
    }
    if (client->name) {
	send_all((Client*)NULL, "NICK %s %s", client->name, newnick);
	free(client->name);
	client->name = (char*)strdup(newnick);
	assert(client->name);
    } else {
	send_client(client, "USER %s", newnick);
	send_all(client, "JOIN %s", newnick, args);
	client->name = (char*)strdup(newnick);
	assert(client->name);
    }
}

USERCMD_FUNC(cmd_version) {
    send_client(client, "VERSION %s", VERSIONTEXT);
}

USERCMD_FUNC(cmd_help) {
    int i;
    send_client(client, "HELP %s", VERSIONTEXT);
    for (i = 0; i < sizeof(UserCommands) / sizeof(UserCommands[0]); i++) {
	if (!(UserCommands[i].flags & CMD_ALIAS || 
	     (UserCommands[i].flags & CMD_ADMIN && !CHECK_ADMIN(client))))
	    send_client(client, "HELP %-8s %-16s %s", UserCommands[i].name, UserCommands[i].args, UserCommands[i].desc);
    }
}					

USERCMD_FUNC(cmd_global) {
    check_register(client);
    check_args(client, *args, 1, "GLOBAL");
    send_all(client, "GLOBAL %s %s", client->name, args);
}

USERCMD_FUNC(cmd_globcmd) {
    check_register(client);
    check_args(client, *args, 1, "GCMD");
    send_all(client, "GCMD %s %s", client->name, args);
}

USERCMD_FUNC(cmd_list) {
    Client *current;
    char ip[128];
    check_args(client, 1, !*args, "LIST");
    for (current = Clients; current; current = current -> next) {
	if (!current->name) 
	    continue;
	getipbysock(current->s, ip);
    	send_client(client, "LIST %s FD 0 IP %s", current->name, ip);
    }
    send_client(client, "LIST END");
}

USERCMD_FUNC(cmd_info) {
    Client *clnt;
    struct timeval curtime;
    char *name;
    char ip[128];
    check_register(client);
    name = strsep(&args, " \t");
    check_args(client, 1, !args, "INFO");
    if (!*name) {
	cmd_sinfo (client, "");
	return;
    }
    clnt = find_client_by_name(name);
    if (clnt == (Client*)NULL) {
	send_client(client, "ERROR 104 %s no such user.", name);
	return;
    }
    gettimeofday(&curtime, (struct timezone *)NULL);
    send_client(client, "INFO NICK %s IP %s IDLE %ld CONNECTED %d",
		clnt->name, getipbysock(clnt->s, ip), 
		curtime.tv_sec - clnt->lastcmdtime, clnt->connecttime);
}

USERCMD_FUNC(cmd_sinfo) {
    Client *clnt;
    int ccount = 0, cncount = 0, idletime = 0;
    struct timeval curtime;
    check_register(client);
    gettimeofday(&curtime, (struct timezone *)NULL);
    for (clnt = Clients; clnt; clnt = clnt -> next)
        if (clnt->name) {
	    ccount++;
	    idletime += curtime.tv_sec - clnt->lastcmdtime;
	} else
	    cncount++;
    idletime /= ccount;
    send_client(client, "SINFO CLIENTS %d UNKNOWN %d IDLE %d UPTIME %ld", ccount, 
		cncount, idletime, time((time_t*)NULL) - server_uptime);
}

USERCMD_FUNC(cmd_protver) {
    send_client(client, "PROTVER %d", protver);
}

