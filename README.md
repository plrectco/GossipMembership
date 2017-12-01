## Gossip membership Protocol
### Objective

The  project is composed of three layers.

1. Emulated Network 
2. P2P Layer
3. Application

The first layer is the infrastructure that this "distributed" network lies on.
It essentially stores the message in local buffer and deliver it between ENsend
and ENrecv.

The second layer is where we required to implent the membership protocol.

The third layer is mainly a testing application for the membership protocol.

### Algorithm design 
I use Gossip membership protocol to design this layer. The basic idea is 

1. heartbeat periodically 
1. gossip membership list 
1. merge membership list 
1. fail time and remove time

Each node is asked to send to some nodes a list of "member" or node it knows
that live in the network. The list of members including the address of each
member and their heartbeat count receive by this node.

When one node A receive a gossip message from node B, it merges it with A's own
membership list. The merge process basically means if the heartbeat number of
certain node in the gossip message is greater than the heartbeats that A
currently record, it will update it with the greater heartbeat values. Otherwise
it does not modify it.

For each member in the membership list, it is marked as failed if the heartbeat
haven't been updated for Tfail time. It is removed if it is not updated for
Tremove time.

### Implementation
There is one node called introducer that is the node that introduce every node
to the network. Every node need to send JIONREQ mession to introducer at the
beginning. 

1. bool MP1Node::recvCallBack(void \*env, char \*data, int size)
Check receive message type, handle the receive message
2. bool updateMemberList(Address \*src, long \*heartbeat, long timestamp);
update the membershiplist of the current node with ONE piece of info about a
node. if the corresponding member in the membership list has a heartbeat of -1,
don't update it because it is labelled as failed. It add a new member to the
membership list if it is never seen and the heartbeat is not -1.
3. void heartBeating();
periodically heartbeat to other nodes
5. bool handleGossip(Address \*src, char \*data, size\_t size);
Handle gossip message
6. void checkFailure();
check if one node is failed. If it exceeds the time limit of failure, set the
heartbeat of it to -1. Remove it from the list if it exceeds the limit of
removal.
4. void sendMemberList(Address \*dst, MsgType type);
This is a helper function to serialize the membership list to a message and send
it to the dst,

Three types of message:
1. JOINREQ: message to notify the introducer. JOINREQ = MsgHeader+1 buffer char
+ Address + heartbeat
2. JOINREP:  message to confirm one node that it is now in the network. It
returns a message of the same format of GOSSIP
3. GOSSIP: the peridical message to notify other nodes the membership list of
one node. GOSSIP = MsgHeader+1 buffer char+ Address + <MembershipEntry>\*size of
membership list. <MembershipEntry> = Address of the member + heartbeat. 
### On Accuracy and Completeness
Once a member is set to -1, it does not recover even if other node has sent a newer
heartbeat about it. This is what this algorithm can be improved. Due to the
limited entry provided by MembershipEntry class, unfortunately, we cannot set it
to fail and track its old heartbeat at the same time.

This however gurantee 100% completeness but not 100% accuracy. 

### Reference
[Specification](mp1_specifications.pdf)
[Paper about Gossip Protocol](GossipFD.pdf)

### Bugs that has been fixed
- should comment debug info otherwise it cannot submit
- should be careful with pointer casting. Forexample (char\*)((MessageHdr\*)
  data+1)+1, jump by sizeof(MessageHdr) bytes then jump by 1 byte.
- int is stored in small endian, that is the lower significant bit stores in
  lowe address\(this however does not affect our project, just a note\).

