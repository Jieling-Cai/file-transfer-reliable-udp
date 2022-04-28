# Reliable File Transfer over UDP
Build a file transfer protocol over UDP using the Go-Back-N protocol to provide reliability.

Supported:
(1) Bootstrapping: Server will take the port on which it will listen as a command line argument.
(2) Request: Client sends a request packet (RRQ) to the server. The request will contain the file name that it wants to fetch, and the window size (for the Go-Back-N protocol) that the server should use.
(3) Each DATA packet has a sequence number (starting from zero) and will be fixed size (512 bytes). Only the lastpacket will have a size less than 512 (it could be zero if the file size is a multiple of 512
bytes). This last packet will signal the end of file.
(4) For each data packet, the client sends an ACK which carries the sequence number of the corresponding data packet that was received (e.g., data packet 0 will generate ACK 0 and so on)
(5) The server uses a fixed timeout value of 3 seconds. If it doesn't receive an ACK, it retransmits the entire window. The client will discard any out-of-order packet that it receives and will also not send an ack for it. So retransmissions from server will only happen due to timeouts. After 5 consecutive timeouts (for the same sequence number), the server stops the communication. This fixed timeout resets each time the window moves.
(6) In case the server cannot transfer the file (e.g., file doesn't exist), the server will send an ERROR message (in response to the RRQ message from the client).

 