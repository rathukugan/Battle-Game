#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "battle.h"

#ifndef PORT
    #define PORT 19384
#endif
int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL; //Linked list of all struct clients
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset; //Holds all socket descriptors currently connected
    fd_set rset; //Holds all socket descriptors ready from select

    int i;

    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = 10;
        tv.tv_usec = 0;  /* and microseconds */

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv); 
        if (nready == 0) { //nready is number of clients ready 
            printf("No response from clients in %ld seconds\n", tv.tv_sec);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }
        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                if (i == listenfd) {
                   printf("a new client is connecting\n");
                    len = sizeof(q);
                    if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                        perror("accept");
                        exit(1);
                    }
                    FD_SET(clientfd, &allset); //Add client to master set of all connections
                    if (clientfd > maxfd) {//update master # of fd's
                        maxfd = clientfd;
                    }
                    printf("connection from %s\n", inet_ntoa(q.sin_addr));
                    head = addclient(head, clientfd, q.sin_addr);
                    
                    write(clientfd, "What is your name? ", 19); 
                }
                else {
                    int result = 0;
                    int idle; //Check findmatch for an idle user
                    for (p = head; p != NULL; p = p->next) {
                        if (p->fd == i) {
			    int skip = 0; //Skip data check
                            if (p->name == NULL) {
                                result = login(p, head);
                            }
                            /* If login has not failed */
                            if (result != -1) {
                            	if (p->partner == NULL){
                                    idle = findmatch(p, head);
                            	}
                            	else {
                                    result = battle(p);
				        if (result == 1) { //victory occured
                                            /* Move clients that have finished battle to end of client list */
                                            head = move(p, head);
                                            head = move(p->partner, head);
                                            struct client *oldpartner = p->partner; //Copy partner before its changed
                                            /* clean up so they can be eligible for battle */
                                            (p->partner)->partner = NULL;
                                            p->partner = NULL;                                        
                                            /* Find match for both clients */
                                            int newidle = findmatch(p, head);
                                            if (newidle == -1) {
                                                p->turn = -1;
                                            }
                                            newidle = findmatch(oldpartner, head);
                                            if (newidle == -1) {
                                                oldpartner->turn = -1;
                                            }
					}
                                    skip = 1; //Dont check input, user didnt enter anything new
                            	}
                                /* Handle data from the idle user not in battle */
                                if (p->turn == -1 && skip == 0) {
                                    char buf[256];
                                    int check = read(p->fd, buf, sizeof(buf) - 1);
                                    if (check > 0) {
                                    //do nothing ignore their data
                                    }
                                    else if (check == 0) {
                                        result = -1; //drop
                                    }
                                    else {
                                        perror("read");
                                        result = -1;
                                    }
                                }     
                            }
                            /* We have a dropped client */
                            if (result == -1) {
				int hadpartner = 0;
				struct client *old;
                                int tmp_fd = p->fd;
                                if (p->partner) { //If client to be removed had a partner
                                    hadpartner = 1; //Keep track of this old partner
                                    old = p->partner;
                                }
                                //Remove client and broadcast
                                head = removeclient(head, p->fd);
                                FD_CLR(tmp_fd, &allset); //remove fd from allset
                                close(tmp_fd);
				if (hadpartner == 1) {
                                    head = move(old, head);
                                    int newmatch = findmatch(old, head); //Find match for dropped client's partner
                                    if (newmatch == -1) { //Becomes an idle user
                                        old->turn = -1;
                                    }
                                } 
                            }
                            else if (idle == -1) {
                                p->turn = -1; //To denote the idle client (not in battle).
                            }
                            else {}
                            
                           break;
                        }
                    }

                }
            }
        }

    }
    return 0;
}

/**
 * Buffer incoming data until newline and allocate memory for
 * client's name to be stored in struct.
 */
int login(struct client *p, struct client *top) {
    char buf[256]; //buffer for client data (reading from client)
    char outbuf[512]; //buffer for output to client
    if ((p->name) == NULL) {
            int nbytes;
            int inbuf = 0;          // buffer is empty; has no bytes
            int room = sizeof(buf); // room == capacity of the whole buffer
            char *after = buf;        // start writing at beginning of buf
            while (((nbytes = read(p->fd, after, room)) > 0)) {
                inbuf += nbytes;
                /* Check for buffer overflow */
                if (inbuf >= sizeof(buf)) {
                    return -1; //Drop this client
                }
                int where = find_network_newline(buf, inbuf);
                if (where >= 0) { //we have a full line
                    buf[where] = '\0';
                    buf[where+1] = '\0';
                    printf("Next message: %s", buf);
                    //Store name in client struct
                    p->name = malloc(strlen(buf) + 1);
                    strncpy(p->name, buf, strlen(buf) + 1);
                    //write to client only
                    sprintf(outbuf, "Welcome, %s! Awaiting opponent...\r\n", p->name);
                    write(p->fd, outbuf, strlen(outbuf));
                    //write to everyone someone new has entered
                    sprintf(outbuf, "**%s enters the arena**\r\n", buf); //format outbuf
                    broadcast(top, p, outbuf, strlen(outbuf));

                    inbuf -= (where+2);
                    int i;
                    for (i=0; i< where; i++) {
                        buf[i] = '\0';
                    }
                    memmove(buf,buf+where+2,inbuf);
                    break;
                }
                after = &buf[inbuf];
                room = sizeof(buf) - inbuf;
            }
            if (nbytes == 0)
                return -1;
    }

    return 0;
}

/**
 * Match client p by going through list of all clients and
 * initialize battle stats and write battle options to clients.
 */
int findmatch(struct client *p, struct client *top) {
    char outbuf[512]; //buffer for output to client
    struct client *c; 
    for (c = top; c; c = c->next) {
        //Find a named client with no partner who has not recently played with p
        if ((c->partner == NULL) && (c->name != NULL) && (c != p) && (p->recent != c)) {
            p->partner = c;
            p->recent = c;
            c->partner = p;
            c->recent = p;
            srand((unsigned) time(NULL)); //Initialize random generator
            //randomize hp between 20 and 30 for both
            p->hp = (rand() % (30+1-20))+20;
            (p->partner)->hp = (rand() % (30+1-20))+20;
            //randomize powermoves between 1-3 for both
            p->power = (rand() % (3+1-1))+1;
            (p->partner)->power = (rand() % (3+1-1))+1;
            //randomize between 0 and 1 to determine who starts first
            int r = rand() % 2;
            if (r == 0) {
                p->turn = 1;
                (p->partner)->turn = 0;
            }
            else { //opponent starts first
                p->turn = 0;
                (p->partner)->turn = 1;
            }
            /* Declare start of battle for both clients */
            sprintf(outbuf, "You engage %s!\r\n", (p->partner)->name);
            write(p->fd, outbuf, strlen(outbuf));
            sprintf(outbuf, "You engage %s!\r\n", p->name);
            write((p->partner)->fd, outbuf, strlen(outbuf));
            
            //Start battle starting with the client with turn 1
            //display stats for both
            stats(p);
            stats(p->partner);
            if (p->turn == 0) { //Display battle options to opponent
                menu(p->partner);
            }
            else {
                menu(p);
            }
            return 0;
        }
    }
    return -1; //no match found
    
}

/**
 * Process data from clients in battle.
 * If client is not active (attacking) discard input and check for drop
 * If client is active check for battle commands.
 */
int battle(struct client *p) {
    //Only turn 1 client should be at this function
    //Otherwise it is invalid input from turn 0 client OR a drop
    char buf[256];
    char outbuf[512];
    if (p->turn == 0) {
        int len = read(p->fd, buf, sizeof(buf) - 1);
        if (len > 0) {
            return 0; //Discard input
        }
        else if (len == 0) { //inactive client has dropped.
            sprintf(outbuf, "--%s dropped. You win!\r\n", p->name);
            write((p->partner)->fd, outbuf, strlen(outbuf));
            return -1; //-1 indicates that it will be removed by removeclient.
        }
        else {
            perror("read");
            return -1;
        }
    } 
    else { //p->turn == 1
        return battlecmd(p); //Process entered battle cmd from client.
        //returns 1 if a victory happened
    }
}

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

/**
 * Allocate memory and add client to list of clients.
 */
struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    p->name = NULL;
    p->partner = NULL;
    p->recent = NULL;
    top = p;
    return top;
}

/**
 * Remove client from list of clients (top).
 */
struct client *removeclient(struct client *top, int fd) {
    struct client **p;
    char outbuf[512]; //buffer for output
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        /* If user has already logged in then alert every client of removal */
        if ((*p)->name) {
            sprintf(outbuf, "**%s leaves**\r\n", (*p)->name);
            broadcast(top, *p, outbuf, strlen(outbuf));
        }
        /* If user was in a battle */
        if ((*p)->partner) {
            sprintf(outbuf, "Awaiting opponent...\r\n");
            write(((*p)->partner)->fd, outbuf, strlen(outbuf));
            /* Clean up end of battle for this client */
            ((*p)->partner)->partner = NULL;
            ((*p)->partner)->recent = NULL;
            printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        }
        free(*p);
        *p = t;
    } 
   else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
    }
    return top;
}

/**
 * Write s to every client in list of all clients (top) 
 * except for one client (unique).
 */
void broadcast(struct client *top, struct client *unique, char *s, int size) {
    struct client *p; 
    for (p = top; p; p = p->next) {
        if (p != unique) {
            write(p->fd, s, size);
        }
    }
    /* should probably check write() return value and perhaps remove client */
}

/**
 * Move client p to the end of client list (head).
 */
struct client * move(struct client *p, struct client *head) {
    struct client *top = head;

    if (top == NULL)
        return top;
    if (top == p) {
        if (top->next == NULL) {
            return head;
        }
        else {
            head = top->next;
        }
    }

    while (top->next != NULL) {
        if (top->next == p) {
            if ((top->next)->next == NULL) { //p is already last do nothing
		return head;
            }
            else {
                top->next = (top->next)->next;
            }
        }
        top = top->next;
    }

    top->next = p;
    p->next = NULL;

    return head;    
}
