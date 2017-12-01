/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
    this->localtime = 0;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr)); // increment by 1 MessageHdr
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long)); // increment by 1 MessageHdr and 1 buffer char

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
#ifdef DEBUGLOG
    log->LOG(&memberNode->addr, "finish node");
#endif

    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size) {
	/*
	 * Your code goes here
	 */
    // get the message
    assert(size > 0 && size >= sizeof(MessageHdr));
    MessageHdr* msg = (MessageHdr*) data;
    Address* addr = (Address*) (msg + 1);
    size -= sizeof(MessageHdr) + sizeof(Address) + 1;
    data += sizeof(MessageHdr) + sizeof(Address) + 1;
//#ifdef DEBUGLOG
//    string temp = "receive message from " + addr->getAddress();
//    log->LOG(&memberNode->addr, temp.c_str());
//#endif
    switch(msg->msgType) {
        case JOINREQ:
            updateMemberList(addr, (long*) data, localtime);
            sendMemberList(addr, JOINREP);
            break;
        case JOINREP:
            memberNode->inGroup = 1;
            handleGossip(addr, data, size);
            break;
        case GOSSIP: // periodical message exchange
            handleGossip(addr, data, size);
            break;
        default:
#ifdef DEBUGLOG
            log->LOG(&memberNode->addr, "Undefined message type found!");
#endif

    }


}



// send heartbeat to other nodes
void MP1Node::heartBeating() {
    // select p nodes to send heartbeat
    int p = 10;
    double prob = p*1.0/memberNode->memberList.size();
    for(vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        if(rand()*1.0/RAND_MAX < prob) {
            Address a;
            memcpy(a.addr, &it->id, sizeof(int));
            memcpy(&a.addr[4], &it->port, sizeof(short));
            sendMemberList(&a, GOSSIP);
        }
    }

}

void MP1Node::sendMemberList(Address* dst, MsgTypes type) {
    MessageHdr *msg;
    // msg is composed of the header, the sender's address, each memberlist entry(address, heartbeat)
    size_t memEntSize = sizeof(memberNode->addr.addr)+sizeof(long);
    size_t msgsize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) + 1 + memEntSize * memberNode->memberList.size();
    msg = (MessageHdr *) malloc(msgsize * sizeof(char));
    msg->msgType = type;
    char* msgTemp = (char*)msg + sizeof(MessageHdr);
    memcpy(msgTemp, memberNode->addr.addr, sizeof(memberNode->addr.addr));
    msgTemp += sizeof(memberNode->addr.addr) + 1;
    for(vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        memcpy(msgTemp, &it->id, sizeof(int));
        msgTemp += sizeof(int);
        memcpy(msgTemp, &it->port, sizeof(short));
        msgTemp += sizeof(short);
        memcpy(msgTemp, &it->heartbeat, sizeof(long));
        msgTemp += sizeof(long);
    }

//#ifdef DEBUGLOG
//    string t = "send message to "  + dst->getAddress();
//    log->LOG(&memberNode->addr, t.c_str());
//#endif

    // send JOINREQ message to introducer member
    emulNet->ENsend(&memberNode->addr, dst, (char *)msg, msgsize);

    free(msg);

}




// if a new member return true
bool MP1Node::updateMemberList(Address* src, long* heartbeat, long timestamp) {

    bool isNewMember = true;
    for(vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        Address addr = getMemberAddress(*it);
        if(addr == *src) {
            isNewMember = false;
            // if certain node has failed, we do not update it
            if(it->heartbeat != -1 && it->heartbeat < *heartbeat) {
                it->heartbeat = *heartbeat;
                it->timestamp = timestamp;
            }
            break;
        }
    }

    if(*heartbeat != -1 && isNewMember) {

        char id[4];
        memcpy(id,src->addr, sizeof(int));
        memberNode->memberList.push_back(MemberListEntry(*((int*) id), src->addr[4], *heartbeat, timestamp));
        log->logNodeAdd(&memberNode->addr, src);
        memberNode->nnb++;

    }
    return isNewMember;
}

// id = char[3]char[2]char[1]char[0] because of small endian
// id = 0 0 0 1 while the actual storage is 1, 0, 0, 0, should be translated to addr = 1, 0, 0, 0
Address MP1Node::getMemberAddress(MemberListEntry& member) {
    Address a;

    memcpy(a.addr, &member.id, sizeof(int));

    memcpy(&a.addr[4], &member.port, sizeof(short));
    return a;
}

bool MP1Node::handleGossip(Address* src, char* data, size_t size) {
    size_t memEntSize = sizeof(memberNode->addr.addr)+sizeof(long);
    char* endOfMsg = data+size;
    if(size%memEntSize != 0) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Message contaiminated");
#endif
        return false;
    }
    while(data < endOfMsg) {


        Address* addr = (Address*) data;
        data += sizeof(Address);
        long* hb = (long*) data;
        data += sizeof(long);

        updateMemberList(addr, hb, localtime);
    }

}

void MP1Node::checkFailure() {
    for(vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {


        if(it->timestamp < localtime - TFAIL) {
            it->heartbeat = -1;
        }

        if(it -> timestamp < localtime - TREMOVE) {

            Address a = getMemberAddress(*it);
            log->logNodeRemove(&memberNode->addr, &a);

            memberNode->memberList.erase(it);
            it--;
        }
    }
}



/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */

    localtime++;
    updateMemberList(&memberNode->addr, &localtime, localtime);
    checkFailure();
    if(localtime % TGOSSIP == 0)
        heartBeating();

}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
