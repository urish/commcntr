#include <stdlib.h>
#include <unistd.h>
#include "commcntr.h"

static int server_port = PORT, do_fork = DO_FORK;
char *admin_pass = ADMINPASS;

void process_args (int argc, char *argv[]) {
    int argp = 1, args, i;
    while (argc - argp) {
	if (argv[argp][0] != '-') 
	    break;
	i = 1;
	args = argp++;
	while (argv[args][i]) {
	    switch (argv[args][i++]) {
	    case 'p':
		if (argc - argp)
		    admin_pass = argv[argp++];
		break;
	    case 'h':
	        fprintf(stderr, "Usage: %s [-dfhqV] [-p adminpass] [port]\n", argv[0]);
	        exit(1);
	    case 'V':
	        printf("%s\n", VERSIONTEXT);
	        exit(1);
	    case 'd':
	        do_fork--;
	        break;
	    case 'f':
	        do_fork++;
	        break;
	    case 'q':
	        fclose(stdout);
	        break;
	    default:
	        fprintf(stderr, "[Warning] unknown command line option '-%c' ignored.\n", argv[args][i-1]);
	    }
	}
    }
    if (argc - argp) {
        if (!(server_port=atol(argv[argp])))
    	    printf("[Warning] invalid port number %s, using default port %d.\n", argv[argp], server_port=PORT);
	if (argc - ++argp)
	    printf("[Warning] extra args ignored.\n");
    }
}

int main (int argc, char **argv	) {
    int pid;
#if QUIET
    fclose(stdout);
#endif /* QUIET */
    process_args(argc, argv);
    printf("[1;31mù[1;37mí[1;31mù[0m %s\n",VERSIONTEXT);
    printf("[1;31mù[1;37mí[1;31mù[0m Listening on port %d...\n", server_port);
    init_server(server_port);
#ifdef CHROOT
    printf("[1;31mù[1;37mí[1;31mù[0m changing root directory to %s.\n", CHROOT);
    if (chroot(CHROOT) < 0) {
	perror("[Error] chroot");
	return 0;
    }
#endif /* SETUID */
#ifdef SETGID
    printf("[1;31mù[1;37mí[1;31mù[0m Setting gid to %d.\n", SETGID);
    setgid(SETGID);
    if (setgid(SETGID) < 0) {
	perror("[Error] setgid");
	return 0;
    }
#endif /* SETGID */
#ifdef SETUID
    printf("[1;31mù[1;37mí[1;31mù[0m Setting uid to %d.\n", SETUID);
    if (setuid(SETUID) < 0) {
	perror("[Error] setuid");
	return 0;
    }
#endif /* SETUID */
    if (do_fork > 0)
	switch ((pid = fork())) {
	case -1:
	    perror("[Error] fork");
	    return 1;
	case 0:
	    break;
	default:
	    printf("[1;31mù[1;37mí[1;31mù[0m Going to background (pid = %d)...\n", pid);
	    return 0;
	}
    start_server();
    return 1;
}
