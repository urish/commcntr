CC		= gcc
DEBUG		= 
CFLAGS		= -Wall ${DEBUG:YES=-g}
OUTPUT		= commcntr
LIBS		= 
INCLUDE		= config.h
SRCS		= commcntr.c server.c user.c
OBJS		= ${SRCS:.c=.o}
MAKE		= make
NAME		= AiCP Daemon

all:	${OUTPUT}

remake:	clean all

clean:  ${OUTPUT}-clean

${OUTPUT}-clean:
	@echo \* Cleaning for ${NAME} ...
	@rm -f *.o ${OUTPUT}

${OUTPUT}: ${OBJS}
	@echo -n \* Building ${NAME} 
	@if [ "${DEBUG}" != "YES" ]; then echo "..."; else echo " (debugging version) ..."; fi
	@${CC} -rdynamic -o ${OUTPUT} ${OBJS} ${LIBS}
	@if [ "${DEBUG}" != "YES" ]; then strip ${OUTPUT}; fi

.c.o:	${INCLUDE}
	@echo \* " " Building File $<...
	@${CC} ${CFLAGS} -c $<
