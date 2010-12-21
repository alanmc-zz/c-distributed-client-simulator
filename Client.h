/*
 * Client.h
 *
 * Generic Distributed Client base class with two implementation provided
 *
 * GossipClient
 *  - Utilizes a trivial "gossip" protocol to flood the buddy network
 *    with status information every minute. State converges relatively quickly
 *    with high probabiliity
 *
 * HeartbeatClient
 *  - Utilizes a trivial round robin "heartbeating" protocol to keep buddy network
 *    up-to-date with latest status information.
 */

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "ClientTypes.h"
#include "Stats.h"

#include <iostream>
#include <vector>

class Client {

 public:
 Client(const clientId_t& clientId, 
	const uint32_t& buddyCount, 
	const uint32_t& nodeCount, 
	const uint32_t& initialSleepPeriod,
	const ClientState& initialState,
	MessageQueue* messageQueue,
	SimulatorStatistics* stats)
   : clientId_(clientId),
     buddyCount_(buddyCount),
     observerCount_(0),
     nodeCount_(nodeCount),
     sleepPeriod_(initialSleepPeriod),
     state_(initialState),
     messageQueue_(messageQueue),
     stats_(stats)
  { }

  virtual ClientState switchState(const uint32_t timestamp) {
    if (state_ == ONLINE) {
      state_ = OFFLINE;
    } else {
      state_ = ONLINE;
    }
    
    return state_;
  }

  bool addBuddy(const clientId_t& buddyId, const ClientState& buddyState) {

    if (clientId_ == buddyId || buddiesSet_.find(buddyId) != buddiesSet_.end()) {
      return false;
    }

    buddies_.push_back(buddyId);
    buddiesSet_.insert(buddyId);
    buddyState_[buddyId] = buddyState;

    return true;
  }

  bool addObserver(const clientId_t& clientId) {

    if (clientId_ == clientId || observersSet_.find(clientId) != observersSet_.end()) {
      return false;
    }

    observers_.push_back(clientId);
    observersSet_.insert(clientId);
    observerCount_++;
    return true;
  }

  void VerifyState(ClientStateMap& stateMap) {
    
    uint32_t totalRecords = 0;
    uint32_t correctRecords = 0;
    
    for (ClientStateMap::const_iterator i = buddyState_.begin(); i != buddyState_.end(); i++) {
      
      (*stats_).incrementTotalBuddyRecords();
      
      if ( stateMap[(*i).first] == (*i).second ) {
	(*stats_).incrementTotalCorrectBuddyRecords();
      } 

    }
  }

  inline clientId_t getClientId(void) const {
    return clientId_;
  }

  inline ClientState getState(void) const {
    return state_;
  }
    
  inline size_t getBuddyCount(void) const {
    return buddiesSet_.size();
  }

  inline bool isOnline(void) const {
    return state_ == ONLINE;
  }

  inline uint32_t getSleepPeriod(void) const {
    return sleepPeriod_;
  }

  inline void setSleepPeriod(const uint32_t& sleepPeriod) {
    sleepPeriod_ = sleepPeriod;
  }

  virtual void handleMessage(const ClientMessage& message) = 0;
  virtual void runTasks(const uint32_t& timestamp) = 0;

 protected:
  
  ClientMessage createMessage(const clientId_t& recipientId, 
			      const ClientMessageType& type,
			      const uint32_t& timestamp,
			      const uint32_t& gossipId,
			      ClientSet& clientChain) {

    ClientMessage message;
    message.recipientId = recipientId;
    message.senderId = clientId_;
    message.gossipId = gossipId;
    message.messageType = type;
    message.clientChain = clientChain;
    message.timestamp = timestamp;
    return message;
  }
  
 protected:
  clientId_t clientId_;

  uint32_t buddyCount_;
  uint32_t nodeCount_;
  uint32_t sleepPeriod_;
  uint32_t observerCount_;

  ClientState state_;

  ClientList buddies_;
  ClientList observers_;

  ClientSet buddiesSet_;
  ClientSet observersSet_;

  ClientStateMap buddyState_;

  MessageQueue* messageQueue_;
  SimulatorStatistics* stats_;
};


class GossipClient : public Client{

 public:

 GossipClient(const clientId_t& clientId, 
	      const uint32_t& buddyCount, 
	      const uint32_t& nodeCount, 
	      const uint32_t& initialSleepPeriod,
	      const ClientState& initialState, 
	      MessageQueue* messageQueue,
	      SimulatorStatistics* stats)
   : Client(clientId, buddyCount, nodeCount, initialSleepPeriod, initialState, messageQueue, stats),
     lastGossipRequest_(0),
     messagesSent_(0)
  { }


  virtual void handleMessage(const ClientMessage& message) {

    // OFFLINE clients don't respond to messages
    if ( !(*this).isOnline() ) {
      return;
    }

    // Check if this is a new gossip cycle.  If so, clean up a bit.
    if (lastGossipRequest_ != message.gossipId) {
      gossipedNodes_.clear();
      messagesSent_ = 0;
      lastGossipRequest_ = message.gossipId;
      
      // At beginning of every gossip phase we assume all clients to be OFFLINE
      for (ClientStateMap::iterator i = buddyState_.begin(); i != buddyState_.end(); i++) {
	buddyState_[ (*i).first ] = OFFLINE;

	if ( (*stats_).getLastState( (*i).first ) == OFFLINE ) {
	  (*stats_).incrementPresenceUpdates();
	  uint32_t senderSwitchTime = (*stats_).getLastStateSwitch(message.senderId);
	  uint32_t delta = message.timestamp - senderSwitchTime;
	  (*stats_).addConvergenceTime(delta);
	}

      }
    }
    
    // Can only forward a maxiumu of 5 messages/minute
    if (messagesSent_ >= 5 ) {
      return;
    }
    
    
    // Select a random buddy
    clientId_t randomNode = rand() % observers_.size();
    
    // Shouldn't be possible to have yourself as a buddy, by check anyway
    while (observers_[randomNode] == clientId_) {
      randomNode = rand() % buddies_.size();
    }
    
    // Insert the gossiped client chain into our known gossiped nodes
    ClientSet clientChain = message.clientChain;
    gossipedNodes_.insert(clientChain.begin(), clientChain.end());

    // Anyone that has forward the gossip chain along is ONLINE
    for (ClientStateMap::iterator i = buddyState_.begin(); i != buddyState_.end(); i++) {

      // If this is a state switch, record it in our stats package
      if (buddyState_[ (*i).first ] != ONLINE) {

	if ( (*stats_).getLastState( (*i).first ) == ONLINE ) {
	  (*stats_).incrementPresenceUpdates();
	  uint32_t senderSwitchTime = (*stats_).getLastStateSwitch(message.senderId);
	  uint32_t delta = message.timestamp - senderSwitchTime;
	  (*stats_).addConvergenceTime(delta);
	}

      }
      
      buddyState_[ (*i).first ] = ONLINE;
    }
    
    // Insert self into the gossiped client chain
    clientChain.insert(clientId_);    

    // Forward it along
    (*messageQueue_).push( createMessage(observers_[randomNode],
					 GOSSIP,
					 message.timestamp,
					 message.gossipId,
					 clientChain) );
    messagesSent_++;
  }

  virtual void runTasks(const uint32_t& timestamp) {
    
    // OFFLINE clients can't run tasks
    if ( !(*this).isOnline() ) {
      return;
    } 
    
    // Pick two random buddies to start our gossip chain
    messagesSent_ = 2;
    gossipedNodes_.clear();

    clientId_t randomNode1 = rand() % observers_.size();
    clientId_t randomNode2 = rand() % observers_.size();

    while (observers_[randomNode1] == clientId_) {
      randomNode1 = rand() % observers_.size();
    }

    while (buddies_[randomNode2] == clientId_ || randomNode2 == randomNode1) {
      randomNode2 = rand() % buddies_.size();
    }
    
    // Start the gossip chain with ourselves and the current time
    lastGossipRequest_ = timestamp;
    ClientSet clientChain;
    clientChain.insert(clientId_);

    // Send the messages
    (*messageQueue_).push( createMessage(observers_[randomNode1],
					 GOSSIP,
					 timestamp,
					 timestamp,
					 clientChain) );

    (*messageQueue_).push( createMessage(observers_[randomNode2],
    					 GOSSIP,
					 timestamp,
					 timestamp,
					 clientChain) );
  }
      
 private:
  
  uint32_t lastGossipRequest_;
  uint32_t messagesSent_;

  std::hash_map<clientId_t, uint32_t> lastBuddyUpdate_;

  ClientSet gossipedNodes_;
};

class HeartbeatClient : public Client{

 public:
 HeartbeatClient(const clientId_t& clientId, 
		 const uint32_t& buddyCount, 
		 const uint32_t& nodeCount, 
		 const uint32_t& initialSleepPeriod,
		 const ClientState& initialState, 
		 MessageQueue* messageQueue,
		 SimulatorStatistics* stats)
   : Client(clientId, buddyCount, nodeCount, initialSleepPeriod, initialState, messageQueue, stats),
     nextObserver_(0),
     lastMessageTimestamp_(0)

  { }

  virtual ClientState switchState(const uint32_t timestamp) {
    if (state_ == ONLINE) {
      state_ = OFFLINE;
    } else {
      state_ = ONLINE;
    }
    
    return state_;
  }

  virtual void handleMessage(const ClientMessage& message) {

    if ( !(*this).isOnline() ) {
      return;
    }
    
    if (buddyState_[message.senderId] == OFFLINE) {
      (*stats_).incrementPresenceUpdates();
      
      uint32_t senderSwitchTime = (*stats_).getLastStateSwitch(message.senderId);
      ClientState senderState = (*stats_).getLastState(message.senderId);
      uint32_t delta = message.timestamp - senderSwitchTime;
      uint32_t lastBuddyUpdate = lastBuddyUpdate_[message.senderId];
      
      (*stats_).addConvergenceTime(delta);
    }
    
    buddyState_[message.senderId] = ONLINE;
    lastBuddyUpdate_[message.senderId] = message.timestamp;
  }
  
  virtual void runTasks(const uint32_t& timestamp) {
    
    if ( !(*this).isOnline() ) {
      return;
    } 
    
    if (timestamp - lastMessageTimestamp_ > 11) {

      ClientSet nil;
      (*messageQueue_).push( (*this).createMessage(observers_[nextObserver_], HEARTBEAT, timestamp, 0, nil) );
      
      lastMessageTimestamp_ = timestamp;
      
      if (++nextObserver_ >= observers_.size()) {
	nextObserver_ = 0;
      }

    }
    
    for (ClientList::const_iterator i = buddies_.begin(); i != buddies_.end(); i++) {
      
      if (buddyState_[*i] == OFFLINE) {
	continue;
      }

      uint32_t lastBuddyUpdate = 0;
    
      if (lastBuddyUpdate_.find(*i) != lastBuddyUpdate_.end()) {
	lastBuddyUpdate = lastBuddyUpdate_[*i];
      }
      
      uint32_t lastUpdateDelta = timestamp - lastBuddyUpdate;
      
      if (lastUpdateDelta > (observers_.size() * 12 * 3)) {
	  
	(*stats_).incrementPresenceUpdates();
	
	uint32_t senderSwitchTime = (*stats_).getLastStateSwitch(*i);
	ClientState senderState = (*stats_).getLastState(*i);
	
	uint32_t delta = timestamp - senderSwitchTime;
	
	(*stats_).addConvergenceTime(delta);
	buddyState_[*i] = OFFLINE;
      }
    }
  }
      
 private:
  
  uint32_t nextObserver_;
  uint32_t sleepPeriod_;
  uint32_t lastMessageTimestamp_;

  uint32_t lastSleepStart_;
  uint32_t lastSleepEnd_;

  uint32_t lastSleepStart1_;
  uint32_t lastSleepEnd1_;

  uint32_t lastSleepStart2_;
  uint32_t lastSleepEnd2_;

  std::hash_map<clientId_t, uint32_t> lastBuddyUpdate_;

};


#endif // _CLIENT_H_
