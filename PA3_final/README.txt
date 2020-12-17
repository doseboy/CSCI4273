Name: William Anderson
Date: November 22 2020
Assignment: PA3
Class: CSCI 4273
Professor: Sangtae Ha


INFORMATION ON PROGRAM:
This is a program to run an proxy server that grabs data from the client
and either looks inside its cache for the data or resolves the 
request for the client. It has an automatic timeout of 60s and there is 
no blacklist unfortunately. However, the cache is a BST that adds based
upon the URL.

HOW TO RUN:
1. cd to the directory with webproxy.c in it.
2. make
3. ./webproxy <portno> 
4. Connect with a browser or use a terminal to send a request.
5. make clean when you're done!
