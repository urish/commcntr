#define	PORT		6970		/* Port to listen on */
#define	BUFLEN		1024
#define MAX_NICK_LENGTH	16		/* Maximum characters allowed in nick */
#define ADMINPASS	"quack"		/* Password for /ADMIN command */
#define IPv6		1		/* IPv6 Support */
#define DO_FORK		1		/* go to background */
#define QUIET		1		/* don't print startup messages. */

/* define those if u want the server to chroot, setuid and setgid upon its
   startup. This is good if running server as root. */
#define SETUID	65534
#define SETGID	65534
//#define CHROOT	"/tmp"
