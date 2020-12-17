Name: William Anderson
Date: September 20 2020
Assignment: PA1
Class: CSCI 4273
Professor: Sangtae Ha


INFORMATION ON PROGRAM:
In this programming assignment I used sockets to send commands
from client to server using UDP protocal. All of this was written 
in C and the localhost IP address was used to test all of this. 
The client can copy and send files to the server and also can copy 
files from the server to the client. Packet size was default 1KB and
the program supports transfer of all files. The client side exits
after every command run. 


HOW TO RUN:
In order to make the executables, type "make" in the command line
at the root directory (udp/ directory)

Once you have done that, type "cd client/" in one terminal and
"cd server/" in another terminal.

Now run "./client <IP of Server> <Port Number>" in the client 
terminal to run the client.

Run "./server <Port Number>" in the server terminal to run the server.

The client side should prompt the user for the following commands:
get [file-name]         >>>Gets a file from the server 
put [file-name]         >>>Copies and sends a file to server
delete [file-name]      >>>Deletes a file in server
ls                      >>>List files in server
exit                    >>>Exit the server

The server will respond with appropriate messages and error codes
for each command.
