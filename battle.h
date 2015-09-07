#ifndef BATTLE_H
#define BATTLE_H

struct client {
    int fd;  /* client socket for reading/writing */
    struct in_addr ipaddr;   /* client ip address */
    char *name;   /* Name of client entered upon login */
    struct client *partner;  /* client's battle partner */
    struct client *recent;  /* client's recent battle partner */
    int hp; /* Hitpoints between 20-30 */
    int power; /* Number of powermoves remaining, between 1-3  */
    int turn; /* 1 if it is the clients turn otherwise 0, -1 if not in battle */
    struct client *next; /* Next client in the list of all clients */
};

/* Allocate memory and add client to list of all clients. */
struct client *addclient(struct client *top, int fd, struct in_addr addr);

/* free memory and remove client and display message to all other clients. */
struct client *removeclient(struct client *top, int fd);

/* Write string s to every client except for client p. */
void broadcast(struct client *top, struct client *p, char *s, int size);

/* Move p to end of linked list (top) */
struct client *move(struct client *p, struct client *head);

/* Get name of client and broadcast the name to all other clients. */
int login(struct client *p, struct client *top);

/* Find another client not in a battle, and start a battle. */
int findmatch(struct client *p, struct client *top);

/* Process client data for battle. */
int battle(struct client *p);

/* Find \r in a string */
int find_network_newline(char *buf, int inbuf);

/* Write battle options to active client p and waiting message for opponent. */
void menu(struct client *p);

/* Display hp, powermoves and opponent hp to client p. */
void stats(struct client *p);

/* Process a regular attack, subtract hp and display to clients in battle.  */
int regatk(struct client *p);

/* Process a powermove, subtract hp and display to clients in battle.  */
int poweratk(struct client *p);

/* Buffer data and display message to opposing client of p.  */
int speak(struct client *p);

/* Buffer data and display message to opposing client of p.  */
int victory(struct client *p);

/* Write winning/losing messages and clean up so they can be matched for another battle.  */
int battlecmd(struct client *p);

/* Setup of listening for a socket.  */
int bindandlisten(void);

#endif


