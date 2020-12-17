Name: William Anderson
Date: December 10th, 2020
Assignment: PA4
Class: CSCI 4273
Professor: Sangtae Ha


INFORMATION ON PROGRAM:
This is a distributed file server!
The client sends data to the server and the server breaks
it up into 4 parts which are distributed throughout the 4
file servers (DFS1 - DFS4) in a redundant fashion. That way
if one of the servers breaks the information is still 
retrievable and the file can still be reconstructed by the
server. It has three commands: list, get and put. The client
can put a file on the DFS and can get files from the DFS as
well as list the files on the server. The client does a lot
of the work because it constructs the parts sent to the 
server, and also reconstructs them.


HOW TO RUN: 
1. Run the commands below: <br/>
make <br/>
./dfs ./DFS1 10001 & <br/>
./dfs ./DFS2 10002 & <br/>
./dfs ./DFS3 10003 & <br/>
./dfs ./DFS4 10004 & <br/>
<br/>   
2. Run the command below: <br/>
./dfc dfc.conf <br/>
<br/>   
3. Syntax for commands on client side: <br/>
put <em>filename<em/> <br/>
get <em>filename<em/> <br/>
list <br/>
<br/>
4. dfc.conf example: <br/>
   Server DFS1 127.0.0.1:10001<br/>
   Server DFS2 127.0.0.1:10002<br/>
   Server DFS3 127.0.0.1:10003<br/>
   Server DFS4 127.0.0.1:10004<br/>
   Username: Alice<br/>
   Password: SimplePassword<br/>
   <br/>
5. dfs.conf example: <br/>
   Alice:SimplePassword <br/>
   

