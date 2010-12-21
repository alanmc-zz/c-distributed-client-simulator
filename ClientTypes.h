/*
 * ClientTypes.h
 * 
 * General data types used by the simulators and clients
 *
 */

#ifndef _CLIENT_TYPES_H_
#define _CLIENT_TYPES_H_

#include <iostream>
#include <queue>
#include "hash_map"
#include "hash_set"

enum ClientState {
  ONLINE,
  OFFLINE
};

enum ClientMessageType {
  HEARTBEAT,
  DISCOVERY,
  GOSSIP
};

typedef uint32_t clientId_t;
typedef std::vector<clientId_t> ClientList;
typedef std::hash_set<clientId_t> ClientSet;
typedef std::hash_map<clientId_t, ClientState> ClientStateMap;


struct ClientMessage {
  clientId_t recipientId;
  clientId_t senderId;
  uint32_t timestamp;
  uint32_t gossipId;
  ClientMessageType messageType;
  ClientSet clientChain;
};

typedef std::queue<ClientMessage> MessageQueue;

#endif // _CLIENT_TYPES_H_
