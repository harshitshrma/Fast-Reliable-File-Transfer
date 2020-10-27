# Fast and Reliable File Transfer

Implemented a socket programming problem consisting of a simple client-server scenario to emulate secure copy command in UNIX.
Used multithreading to reduce the time taken for the file transfer.

-------------------------------------------------------------------------------------------------------------------------------------

Source files:

server.c : This program receives the data file sent by the client in the form of small chunks.

client.c : This program is used to send a file to server by breaking it into smaller files/packets.

Makefile: Contains code to build and run project.

-------------------------------------------------------------------------------------------------------------------------------------

Running the code:

1. Run command: make all

2. Run command: make data (this creates a 1GB data file)

3. Run server first: ./server 

4. Run client next: ./client <server IP address> 

5. Run the commands for changing the network speed or inducing loss in network. Run commands 2 and 3 again to test performance.

    a. sudo ethtool -s eth0 speed 100 (To set the speed to 100Mbps)

    b. sudo tc qdisc add dev eth0 root netem delay 10ms loss 1% (To set the delay to 10ms and loss of 1%)

-------------------------------------------------------------------------------------------------------------------------------------