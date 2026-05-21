# IPC Pipes and Sockets

Main Idea will be how processors talk to eachother

Key Idea: Communication between processes and across the world looks like File I/O

Introduce Pipes and Sockets

Introduce TCP/IP Connections setup for Webserver

Recall: Creating Processess with Fork()
How Does it Work?


Uniformity - Everything is a file in unix. You can talk to files, devices, inter processes communication, etc, all like a file.

Allows simple composition of programs fin | grep | wc

- Open everything before use
- Byte oriented, even if blocks are transferred
- Kernel buffered reads
- Kernel buffered writes
- Excplicit close

Putting it all together: web server

Server Proces
-------------
Kernel
-------------
Hardware

We can imagine the server process starts up and the first thing it does is open sockets to listen to incoming requests
1. read() -> syscall -> kernel -> wait

2. Data comes in from the hrdware -> generates interrupt 

3. read into request buffer

4. parse request

5. file read -> syscall()

6. go to disk interface, do the same thing. 

7. format reply with like http socket with write and then send it outgoing back to hardware

One thing to note: stalled on read both for network and disk. The kernel buffered everything.

Kernel buffer write, when we write our data it goes into the kernele and is buffer immediately

Recall C High Level API - Streams

FILE *fopen(const char* filename, const char* mode);
They return a pointer to a FILE data structure 

Today: Communication Between Processes
- What if processes wish to communicate with one another?
- Why? Shared tsak, cooperative venture with security implications
- Process abstraction designed to discourage inter process communication
- So, must do something special
- This is called interprocess communication

Producer (writer) and consumer (reader) may be distinct processes

Simple option: use a file

write(wfd, wbuf, wlen)

Process A -> Persistent Storage -> Process B

n = read(rfd, rbuf, rmax);

Why might this be wasteful?
- It is slow because in order for communication, which is in memory, you need to go out to disk and back
- So this idea of writing to some file descriptor and reading from one file descriptor is the standard unix mechanism... so whatever we come up with will be different

Q: How many instructions do you lose by waiting for a disk to pull data?
Guess: My guess is okay, a call to disk is maybe 1 ms, and we can assume like an instruction takes maybe an average of 1 clock cycles. Assume a 1GHZ machine with 1,000,000 clocks per ms. Maybe 1,000,000 instructions or something?
Answer: Yeah around 1,000,000

Shared Memory: Better Option?
- Topic for another Day
- You can do thinks like shared data structures, shared linked lists, etc.
- Uncontrolled but fast

Communication Between Processes (Another option)
Suppose we ask kernel to help?
- Consider an in memory queue
- Accesssed via system calls

write(wfd, wbuf, wlen);
PRocess A -> In memory Queue -> Process B
n = read(rfd, rbuf, rmax);

data written by A is held in memory until B reads it
- same interface as we use for files
- internally more efficient
Questions:
- how to set it up?
- what if A generates data faster than B can consume it
- and fice versa what if B consumes data too fast?

If A is generating data too fast, you need to wait
The way you solve synchronization problems, you wait

So if A tries to write on a full queue, sleep, and if B tries to read on an empty queue, sleep.

One Example of this Pattern: POSIX/Unix PIPE

Write
Proc A -> UNIX Pipe -> Proc B
                        Read

Memory buffer is finite:
- if producer A tries to write when full it blocks
- If consume B tries to read when empty it blocks

int pipe(int fileds[2]);
- Allocates two new file descriptors in the process
- Writes to fileds[1] read from fileds[0]
- Implemented as a fixed size queue

How do we know if there's data in the pipe? 
Guess: Some condition variable and a wakeup call
Answer: When process A goes to write, the kernel checks if theres a read waiting and it wakes up the thread for reading. So the kernel is the orchestrator. This is an advantage of being an internal kernel interface

The pipe is just a queue inside of kernel memory whose interfaces use sys calls read and write


Single process Pipe example

#include <unistd.h>
int main(int argc, char*argv[]) {
    char *msg = "message";
    char buf[BUFSIZE];
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        fprintf(stderr," PIPE FAILED"); return exit_failure;
    }

    ssize_t writelen = write(pipe_fd[1], msg, strlen(msg)+1);
    orintf("Sent)"

    ssize_t readlen = read(pipe_fd[0]), buf, BUFSIZE
    print("read", msg)

    close(pipe_fd[0]);
    close(pipe_fd[1]);
}

this is buffered in kernel space

Pipes Between Processes

Pipe gives two file descriptors
Then fork(), we have a parent process and a child process who share a pipe (remember that file descriptors re copied over in a process)
So now the parent and child can read and write the pipe... but this is kind of goofy

What you typically do is generate the pipe then fork, and depending on which process you;re in you close one fd in one process and the other in the other.

So say you have a pipe, you fork the process, and then the parent closes the read in fd, and the parent closes the write in fd, so the parent writes and the child reads


Once we have communication, need a protocol
- Include syntax and semantics
- Described formally by a state machine
- Across network may need a way to translate between different represenations for numbers, strings, etc.
- Such translation typically part of a remote procedure call (RPC)


Web Server Request/Reply Protocol

Client -> Request -> Web Server -> Reply -> Client

Client-Server Protocols: Cross Network IPC
Many clients accessing a common server
File servers, www, FTP, databases

Q: How does a server keep track of many clients?
A: IP Address + Port

What is a network connectoin?
- Bidirectoinal stream of bytes between two processes on possibly different machines

The Socket Abstraction: The Endpoint for Communication
Key idea: Communication across the world loks like file i/o
write(wfd, wbuf, wlen);
process a <-> socket <-> socket <-> proccess b
port vs. socket? port describes unique commnuication, socket is a data structure

Sockets: Endpoint for Communication
- Queues to temporarily hold results
Connection: Two Sockets Connected Over the Network
- How to open()?
- what is the namespace?
- how are they connected

Sockets: More Details
- Socket: an abstraction for one endpoint of network connection
- Sockets came from berkeley
- Same abstraction for any kind of network
    - Local
    - The internet
    - Things nobody uses anymore , osi, appletalk, etc.

Sockets look a lot like pipes!

- Looks just like a file with a file descriptor
- Corresponds to a network connection (two queues)
- write adds to output queue
- read removes from it input queue
- some operations do not work, eg lseek

How can we use sockets to support real appliations?
- A bidirectional byte stream isnt useful on its own
- may need messaging facility to partition stream into chunks
- may need rpc facility to translate one environment to another and provide the abstraction of a functionc all over the network

Simple Example: Echo server
 

What assumptions are we making?
- Reliable
    - Write to a file => read it back
    - write to a tcp socket
- In Order (sequential stream)
    - this is because of TCP IP
    - big advantage
- When ready?
    - file read gets whatever is there at the time
    - assumes writing already took place
    - blocks if nothing has arrived yet
    - like pipes

Socket Creation
- File systems provide a collection of permanent objects in a structured name space
    - processes open, read/write/close them
    - files exist indepdendently of processes
    - easy to name what file to open

- Pipes: one way communication between processes on the same physical machine
    - single queue
    - created transiently by a call to pipe
    - passed from parent to children

- Sockets: two way communication between processes on same or different machine
    - two queues (two in each direciton)
    - processes on separate machines, no common ancestor
    - how do we naem the objects we are opening?

Namespaces for Communication Over IP
- Hostname
    - www.eecs.berkeley.edu
- IP Address
    - 128.32.244.172 (IPv4, 32-bitInteger)
    - 2607:f140:0:81::f (IPv6, 128-bit integer)
- Port Number
    - 0-1023 are system ports
    - 1024-49151 are registered ports
    - 49152-65535 are dynamic or private

Connection Setup over TCP/IP

Client:
- Special kind of socket: server socket
    - Has file descriptor
    - Can't read or write
- Two operations:
    - listen
    - accept
- Client sockets --request connection--> server socket -> new socket
    - then the client socket connections to the new socket, not the server socket
- 5tuple identifies each connection:
    - source ip address
    - destination ip address
    - source port number
    - destination port number
    - protocol
- often client port "randomly assigned"
- server port often is well known 
    - 80 (web), 443 (secure web), 25 (sendmail), etc.

Sockets in concept
- Server:
    - create server socket
    - bind it to an address (host:port)
    - listen for connection
    - accept syscall()
- Client:
    - create client socket
    - connect it to server
- client connection sockets <=> server connection socket

Client Protocol

char *host_name, *port_name;

// create a socket
struct addrinfo *server = lookup_host(host_name, port_name);
int sock_fd = socket(server->ai_family, server)
... finish tihs
// connect to speicified host and port

//carry out client server protocol

// clean up on temrination

server protocol (v1)

fill this in too:
// create socket to listen for client connections

// bind socket to specific port

// start listening for new client onections

while (1) {

    // accept a new client connection, obtaining a new socket
    ...
}

server should handle each connection in a separate process to protect themselves

server protocol (v2)
// socket setup code elided
while (1) [
    // accept a new client connection
    fill in code
]

concurrent server
so far, in the sevrer: listen will queue request, buffering present elsewhere

a concurrent server can handle and service a new connection before the preivous client disconnects



