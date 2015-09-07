# Battle-Game
A server for a text-based battle game built through network programming using Unix sockets and the select command. 

Clients can connect to the server where they are matched up using an algorithm.

### How the Game Works
1. Type in a name and hit enter.
2. Wait for an opponent to begin combat.

Combat Instructions
* Press a for a regular attack or p for a powermove. 
* Regular attacks are weak but guaranteed to hit; powermoves are strong, but not guaranteed to hit, and limited in number. 
* The other available option is to (s)peak something. Only the currently-attacking player can talk.


Winner is declared when the opponent's hitpoints are 0.

* Each player starts a match with between 20 and 30 hitpoints.
* Each player starts a match with between one and three powermoves.
* Damage from a regular attack is 2-6 hitpoints.
* Powermoves have a 50% chance of missing. If they hit, then they cause three times the damage of a regular attack.

#### Technical Notes
* Run make then enter battle, to start the server on port defined in makefile.
* No text is sent to your opponent until you hit enter when speaking. 
* Game is run through command line with stty -icanon to prevent the terminal driver from waiting for complete lines of text.
