/*
 * Simulator.h
 *
 * Generic Distributed Client Simulation with two implementation provided
 *
 * GossipSimulator
 *  - Utilizes a trivial "gossip" protocol to flood the buddy network
 *    with status information every minute. State converges relatively quickly
 *    with high probabiliity
 *
 * HeartbeatSimulator
 *  - Utilizes a trivial round robin "heartbeating" protocol to keep buddy network
 *    up-to-date with latest status information.
 */

#ifndef _CLIENT_SIMULATOR_H_
#define _CLIENT_SIMULATOR_H_


#include <iostream>
#include "time.h"

#include "ClientTypes.h"
#include "Stats.h"
#include "Client.h"


/*
 * class Simulator
 *
 * Our base simulator template. Derived templates supply a Client implementation
 * and override "void run(void)"
 *
 */
template<class ClientType, uint32_t nodeCount, uint32_t buddyCount, uint32_t timespan> 
  class ClientSimulator {
  
 public:
 
 ClientSimulator()
 : messageQueue_(new MessageQueue()),
   stats_(new SimulatorStatistics())
 { 
   srand(time(NULL));
   initialize();   
 }


 ~ClientSimulator() {

   for (int i = 0; i < nodeCount; i++) {
     delete clients_[i];
   }
   
   delete messageQueue_;
   delete stats_;
 }

 // Our main event loop, implemented by derived template classes.
 virtual void run(void) = 0;
 
 protected:
 
 
 void initialize(void) {

   std::cout << "Initializing Clients...";
   flush(std::cout);

   // Client construction
   for (uint32_t i = 0; i < nodeCount; i++) {

     // Sleep period is random between 0 - 3999
     uint32_t initialSleepPeriod = rand() % 4000;

     // Construct a client with a random initial state and insert in into our sleep schedule
     ClientState initialState = (*this).generateRandomState();     
     clients_[i] = new ClientType(i, buddyCount, nodeCount, initialSleepPeriod, initialState, messageQueue_, stats_);
     sleepSchedule_[initialSleepPeriod].insert(i);
     
     // Add the initial state "switch" to our stats package
     (*stats_).addStateSwitch(clients_[i]->getClientId(), 0, clients_[i]->getState() );

     // Update our canonical state map
     clientState_[i] = initialState;
     
     // Update our online/offine sets
     if (initialState == ONLINE) {
       onlineClients_.insert(i);
     } else {
       offlineClients_.insert(i);
     }
   }

   std::cout << ".Done!" << std::endl;
   std::cout << "Generating buddy lists...";
   flush(std::cout);

   // Visit every node, populating it with "buddies"
   for (uint32_t j = 0; j < nodeCount; j++) {
     
     if (j % 100 == 0) {
       std::cout << ".";
       flush(std::cout);
     }

     while (clients_[j]->getBuddyCount() < buddyCount) {
       
       clientId_t buddyId = rand() % nodeCount;
       
       if (clients_[j]->addBuddy( buddyId, clients_[buddyId]->getState() ) ) {
	 clients_[buddyId]->addObserver( j );
       }
     }
   }

   std::cout << ".Done!" << std::endl;
 }
 
 ClientState generateRandomState(void) {
   if (rand() % 2 == 0) {
     return ONLINE;
   }

   return OFFLINE;
 }

 // In-Memory messaging dispatch
 void dispatchMessage( const ClientMessage& message ) {
   clients_[message.recipientId]->handleMessage(message);
 }

 // Add messages to the in-memory queue for "dispatch"
 void dispatchPendingMessages(void) {
   while ( !(*messageQueue_).empty() ) {
     
     (*stats_).incrementMessagesSent();
     
     // Drop message with 5% probabilty
     if ( (rand() % 100) < 5 ) {
       (*stats_).incrementMessagesDropped();
       (*messageQueue_).pop();
     } else {
       (*this).dispatchMessage( (*messageQueue_).front() );
       (*messageQueue_).pop();
     }
   }
 }

 // Switch client's state (ONLINE->OFFLINE | OFFLINE->ONLINE)
 void switchClientState(const clientId_t& clientId, const uint32_t& timestamp) {

   // Switch the client's state
   clients_[clientId]->switchState(timestamp);
   
   // Set our sleep schedule
   uint32_t sleepDuration = (rand() % 4000) + 1;
   sleepSchedule_[timestamp + sleepDuration].insert(clients_[clientId]->getClientId());

   (*stats_).addSleepTime(sleepDuration);
   (*stats_).incrementSleepStates();

   // Update our global state table
   clientState_[clients_[clientId]->getClientId()] = clients_[clientId]->getState();

   // Update our online and offline sets
   if (clients_[clientId]->getState() == ONLINE) {
     offlineClients_.erase(clientId);
     onlineClients_.insert(clientId);
   } else {
     onlineClients_.erase(clientId);
     offlineClients_.insert(clientId);
   }

   // Update our global stats
   (*stats_).addStateSwitch(clients_[clientId]->getClientId(), timestamp, clients_[clientId]->getState());
 }

 public:
 ClientType* clients_[nodeCount]; 
 
 ClientSet onlineClients_;
 ClientSet offlineClients_;
 
 MessageQueue* messageQueue_;
 SimulatorStatistics* stats_;

 ClientStateMap clientState_;

 std::hash_map<uint32_t, std::hash_set<clientId_t> > sleepSchedule_;

};

/*
 * class GossipSimulator
 *
 * "Gossip" Simulator provides and event loop and Client implementation
 * that implements network flooding protocol
 *
 */

template<uint32_t nodeCount, uint32_t buddyCount, uint32_t timespan>
  class GossipSimulator : public ClientSimulator<GossipClient, nodeCount, buddyCount, timespan> {
  
 public:

 GossipSimulator() 
   : ClientSimulator<GossipClient, nodeCount, buddyCount, timespan>() {}

 virtual void run(void) {
    
    uint32_t timeElapsed = 0;
    uint32_t convergenceSpan = 1200;

    // Our simulated time event loop.  One iteration == one second of sim time
    while (timeElapsed < timespan) {

      // "Gossip" every minute
      if (timeElapsed % 60 == 0) {
	
	// In the GossipClient, runTasks kicks off gossip 
	for (ClientSet::const_iterator i = (*this).onlineClients_.begin(); i != (*this).onlineClients_.end(); i++) {
	  (*this).clients_[*i]->runTasks(timeElapsed);       
	}
	
	// Dispatch all messages
	(*this).dispatchPendingMessages();	
      }
      
      // Grab the clients that are waking up at this time and switch their states
      std::hash_set<clientId_t> wakingClients = (*this).sleepSchedule_[timeElapsed];
      
      for (std::hash_set<clientId_t>::const_iterator i = wakingClients.begin(); i != wakingClients.end(); i++) {
	(*this).switchClientState(*i, timeElapsed);
      }
      
      // Clear stale part of sleep schedule
      (*this).sleepSchedule_.erase(timeElapsed - 1);

      timeElapsed++;
      
      if (timeElapsed % 10000 == 0) {
	std::cout << timeElapsed << " seconds elapsed" << std::endl; 
      }
      
    }
    
    std::cout << "Total Presence Updates: " << (*this).stats_->getPresenceUpdatesCount() << std::endl;
    std::cout << "Total Messages Sent: " << (*this).stats_->getTotalMessagesSentCount() << std::endl;
    std::cout << "Total Messages Dropped: " << (*this).stats_->getTotalMessagesDroppedCount() << std::endl;
    std::cout << "Messages / Second: " << (double)(*this).stats_->getTotalMessagesSentCount() / (double)timeElapsed << std::endl;
    std::cout << "Average Time to Converge: " << ((*this).stats_->getPresenceUpdatesCount() == 0 ? 0 : (*this).stats_->getTotalConvergenceTime()/(*this).stats_->getPresenceUpdatesCount()) << std::endl;
    std::cout << "Average Sleep Time: " << ((*this).stats_->getTotalSleepStates() == 0 ? 0 : (*this).stats_->getTotalSleepTime()/(*this).stats_->getTotalSleepStates()) << std::endl;
    
    /*
     *
     * Consistency check
     *
     * Disable state switching and turn all clients ONLINE
     * All buddy state tables should converge over time
     *
     */

    //Switch all clients on
    for (uint32_t i = 0; i < nodeCount; i++) {
      
      clientId_t clientId = i;
      
      if (!(*this).clients_[clientId]->isOnline()) {
	(*this).switchClientState(clientId, timeElapsed);
      }
    }
    
    while (timeElapsed < timespan + convergenceSpan) {
      
      if (timeElapsed % 60 == 0) {
	
	for (ClientSet::const_iterator i = (*this).onlineClients_.begin(); i != (*this).onlineClients_.end(); i++) {
	  (*this).clients_[*i]->runTasks(timeElapsed);       
	}
	
	(*this).dispatchPendingMessages();
      }
      
      timeElapsed++;      
    }
    
    for (uint32_t clientId = 0; clientId < nodeCount; clientId++) {
      (*this).clients_[clientId]->VerifyState((*this).clientState_);
    }

    std::cout << "Total Buddy Records: " << (*this).stats_->getTotalBuddyRecords() << std::endl;
    std::cout << "Total Correct Buddy Records: " << (*this).stats_->getTotalCorrectBuddyRecords() << std::endl;
    std::cout << "Accuracy Rate: " << (float)(*this).stats_->getTotalCorrectBuddyRecords()/(float)(*this).stats_->getTotalBuddyRecords() << std::endl;
    
 }
  
};

  
template<uint32_t nodeCount, uint32_t buddyCount, uint32_t timespan> 
  class HeartbeatSimulator : public ClientSimulator<HeartbeatClient, nodeCount, buddyCount, timespan> {

 public:
 
 virtual void run(void) {
   
   uint32_t timeElapsed = 0;
   uint32_t convergenceSpan = 2200;

   while (timeElapsed < timespan) {
     
     //     for (ClientSet::const_iterator i = (*this).onlineClients_.begin(); i != (*this).onlineClients_.end(); i++) {
     for (int i = 0; i < nodeCount; i++) {
       clientId_t clientId = i;
       
       if (!((*this).clients_[clientId]->isOnline()) ) {
	 continue;
       }
       
       (*this).clients_[clientId]->runTasks(timeElapsed);
       (*this).dispatchPendingMessages();
     }

     std::hash_set<clientId_t> wakingClients = (*this).sleepSchedule_[timeElapsed];
     
     for (std::hash_set<clientId_t>::const_iterator i = wakingClients.begin(); i != wakingClients.end(); i++) {
       (*this).switchClientState(*i, timeElapsed);
     }
     
     (*this).sleepSchedule_.erase(timeElapsed - 1);
     timeElapsed++;
     
     if (timeElapsed % 10000 == 0) {
       std::cout << timeElapsed << " seconds elapsed" << std::endl; 
     }
   }

   std::cout << "Total Presence Updates: " << (*this).stats_->getPresenceUpdatesCount() << std::endl;
   std::cout << "Total Messages Sent: " << (*this).stats_->getTotalMessagesSentCount() << std::endl;
   std::cout << "Total Messages Dropped: " << (*this).stats_->getTotalMessagesDroppedCount() << std::endl;
   std::cout << "Messages / Second: " << (double)(*this).stats_->getTotalMessagesSentCount() / (double)timeElapsed << std::endl;
   std::cout << "Average Time to Converge: " << ((*this).stats_->getPresenceUpdatesCount() == 0 ? 0 : (*this).stats_->getTotalConvergenceTime()/(*this).stats_->getPresenceUpdatesCount()) << std::endl;
   std::cout << "Average Sleep Time: " << ((*this).stats_->getTotalSleepStates() == 0 ? 0 : (*this).stats_->getTotalSleepTime()/(*this).stats_->getTotalSleepStates()) << std::endl;
   
   std::cout << "Converging Clients...";
   flush(std::cout);

   for (uint32_t i = 0; i < nodeCount; i++) {
     
     clientId_t clientId = i;
       
     if (! (*this).clients_[clientId]->isOnline()) {
       (*this).switchClientState(clientId, 0);
     }
   }
       
   while (timeElapsed < timespan + convergenceSpan) {
     
     if (timeElapsed % 100 == 0) {
       std::cout << ".";
       flush(std::cout);
     }

     for (uint32_t i = 0; i < nodeCount; i++) {
       clientId_t clientId = i;
       (*this).clients_[clientId]->runTasks(timeElapsed);
       (*this).dispatchPendingMessages();
     }
     
     timeElapsed++;
   }
   
   std::cout << ".Done!" << std::endl;
   
   for (uint32_t clientId = 0; clientId < nodeCount; clientId++) {
     (*this).clients_[clientId]->VerifyState((*this).clientState_);
   }
   
   std::cout << "Total Buddy Records: " << (*this).stats_->getTotalBuddyRecords() << std::endl;
   std::cout << "Total Correct Buddy Records: " << (*this).stats_->getTotalCorrectBuddyRecords() << std::endl;
   std::cout << "Accuracy Rate: " << (float)(*this).stats_->getTotalCorrectBuddyRecords()/(float)(*this).stats_->getTotalBuddyRecords() << std::endl; 
 }
  
};
  

#endif // _CLIENT_SIMULATOR_H_
