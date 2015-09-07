#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "battle.h"
/* Helper battle functions */

/**
 * Write battle command options to attacking client p and
 * write to opponent the waiting status.
 */
void menu(struct client *p) {
    //p is attacking, opponent is waiting
    char outbuf[512];
    sprintf(outbuf, "Waiting for %s to strike...\r\n", p->name);
    write((p->partner)->fd, outbuf, strlen(outbuf));

    /* Remove power move option from menu if necessary */
    if (p->power > 0) {
        sprintf(outbuf, "(a)ttack\n(p)owermove\n(s)peak something\r\n");
    }
    else {
        sprintf(outbuf, "(a)ttack\n(s)peak something\r\n");
    }
    write(p->fd, outbuf, strlen(outbuf));
}

/**
 * Write hitpoints, powermoves and opponent hp to client p
 */
void stats(struct client *p) {
    char outbuf[512];

    sprintf(outbuf, "Your hitpoints: %d\r\n", p->hp);
    write(p->fd, outbuf, strlen(outbuf));
    
    sprintf(outbuf, "Your powermoves: %d\r\n\n", p->power);
    write(p->fd, outbuf, strlen(outbuf));
    /* Display opponent hitpoints */
    sprintf(outbuf, "%s\'s hitpoints: %d\r\n\n", (p->partner)->name, (p->partner)->hp);
    write(p->fd, outbuf, strlen(outbuf));
}

/**
 * Process 'a' battle command where client p is attacking.
 */
int regatk(struct client *p){
    char outbuf[512];
    srand((unsigned) time(NULL)); //Initialize random number generator
    
    //randomize regular attack between 2-6
    int atk = (rand() % (6+1-2))+2;
    (p->partner)->hp -= atk;
    /* Check for game winning condition, opponent hp < 0 */
    if (((p->partner)->hp) <= 0) {
        return victory(p); //-1 to indicate a victory for battlecmd function
    }
    else {
        sprintf(outbuf, "\nYou hit %s for %d damage!\r\n", (p->partner)->name, atk);
        write(p->fd, outbuf, strlen(outbuf));
        //write to opponent how much they lost
        sprintf(outbuf, "%s hits you for %d damage!\r\n", p->name, atk);
        write((p->partner)->fd, outbuf, strlen(outbuf));
    }

    return 0;
}

/**
 * Process 'p' battle command where client p is attacking.
 */
int poweratk(struct client *p){
    char outbuf[512];
    srand((unsigned) time(NULL));
    
    p->power -= 1;
    int atk = (rand() % (6+1-2))+2;
    int chance = rand() % 2; //50% chance of hitting, rand value 0 to 1
    if (chance == 1) {
        (p->partner)->hp -= 3*atk;
    }
    else {
        //attack misses nothing happens to hp
        atk = 0;
    }

    if (((p->partner)->hp ) <= 0) {
        return victory(p); //-1 to indicate a victory for battlecmd function
    }
    else {
        if (atk == 0) { //powermove missed
            sprintf(outbuf, "\nYou missed!\r\n");
            write(p->fd, outbuf, strlen(outbuf));

            sprintf(outbuf, "%s missed you!\r\n", p->name);
            write((p->partner)->fd, outbuf, strlen(outbuf));
        }
        else {
            sprintf(outbuf, "\nYou hit %s for %d damage!\r\n", (p->partner)->name, 3*atk);
            write(p->fd, outbuf, strlen(outbuf));
            
            //write to opponent how much they lost
            sprintf(outbuf, "%s powermoves you for %d damage!\r\n", p->name, 3*atk);
            write((p->partner)->fd, outbuf, strlen(outbuf));
        }
    }

    return 0;

}

/**
 * Process 's' battle command where client p is attacking and wants to speak.
 */
int speak(struct client *p) {
    char buf[256]; //buffer for client data (reading from client)
    char outbuf[512]; //buffer for output to client
    int nbytes;
    int inbuf = 0;          // buffer is empty; has no bytes
    int room = sizeof(buf); // room == capacity of the whole buffer
    char *after = buf;        // start writing at beginning of buf
    while (((nbytes = read(p->fd, after, room)) > 0)) {
        inbuf += nbytes;
        /* Check for buffer overflow */
        if (inbuf >= sizeof(buf)) {
            sprintf(outbuf, "\nServer cant handle this lengthy message..\r\n");
            write(p->fd, outbuf, strlen(outbuf));
            return -1;
        }

        int where = find_network_newline(buf, inbuf);
        if (where >= 0) { // OK. we have a full line
            buf[where] = '\0';
            buf[where+1] = '\0';
            printf("Next message: %s", buf);

            sprintf(outbuf, "You speak:%s\r\n\n", buf);
            write(p->fd, outbuf, strlen(outbuf));
            /* Write the message to opponent */
            sprintf(outbuf, "%s takes a break to tell you:\n%s\r\n\n", (p->partner)->name, buf);
            write((p->partner)->fd, outbuf, strlen(outbuf));

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
    if (nbytes == 0) { //Socket closed during speak
        return -1; //To be removed by removeclient
    }

    return 0;

}

/**
 * Display game over messages, prepare clients for next battle.
 */
int victory(struct client *p){
    char outbuf[512];
    /* Display game over messages to battling clients */
    sprintf(outbuf, "You are no match for %s. You scurry away...\r\n", p->name);
    write((p->partner)->fd, outbuf, strlen(outbuf));
    sprintf(outbuf, "%s gives up. You win!\r\n", (p->partner)->name);
    write(p->fd, outbuf, strlen(outbuf));
    
    /* Prepare clients for next battles */
    sprintf(outbuf, "Awaiting opponent...\r\n");
    write(p->fd, outbuf, strlen(outbuf));
    write((p->partner)->fd, outbuf, strlen(outbuf));

    return -1;
}

/**
 * Process battle commands and check for client drop.
 * Returns 0 on normal process, 1 for victory, -1 on error
 */
int battlecmd(struct client *p) { //process p battle command
    char buf[256]; //buffer for client data (reading from client
    char outbuf[512]; //buffer for output to client
    int atkresult;
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        if (len > 1) { //discard old lengthy buffers.
            int i;
            for (i=0; i<len; i++)
                buf[i] = '\0';
        }
        buf[len] = '\0';
        /* Check for valid attacking commands 'a' or 'p' */
        if (buf[len-1] == 'a' || ((buf[len-1] == 'p') && (p->power != 0))) {
            if (buf[len-1] == 'a') {
                atkresult = regatk(p);
            }
            else {
                atkresult = poweratk(p);
            }
            if (atkresult == -1)
                return 1; //1 to indicate victory 
            /* Change client turns */
            p->turn = 0;
            (p->partner)->turn = 1;
            /* Display data for next turn*/
            stats(p);
            stats(p->partner);
            menu(p->partner);

        }
        else if (buf[len - 1] == 's'){
            sprintf(outbuf, "\nSpeak: ");
            write(p->fd, outbuf, strlen(outbuf));
            /* Call function to buffer for data */
            int check = speak(p);
            if (check == -1) { //We had a buffer overflow, drop client
                sprintf(outbuf, "--%s dropped. You win!\r\n", p->name);
                write((p->partner)->fd, outbuf, strlen(outbuf));
                printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
                return -1; //Client will be removed by removeclient
            }
            else { /* Speak successful, continue the active client's turn */
            	stats(p);
            	stats(p->partner);
            	menu(p); //Still p turn and partner still waiting
            }
        }
        else {} //do nothing on invalid cmd, including when powermove = 0
        
        return 0;
    }
    else if (len == 0) {
        //socket is closed
        sprintf(outbuf, "--%s dropped. You win!\r\n", p->name);
        write((p->partner)->fd, outbuf, strlen(outbuf));
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        return -1; //Client will be removed by removeclient
    } 
    else { // shouldn't happen
        perror("read");
        return -1;
    }
}

/**
 * Find network newline '\r' in string.
 */
int find_network_newline(char *buf, int inbuf) {
  int i = 0;
  while (i <= inbuf) {
      if (buf[i] == '\r') {
        return i;
      }
      i++;
  }

  return -1; // '\r' not found
}
