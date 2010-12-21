#ifndef _STATS_H_
#define _STATS_H_

#include "ClientTypes.h"

class SimulatorStatistics {

 public:
  SimulatorStatistics() {
    totalConvergenceTime_ = 0;
    totalPresenceUpdates_ = 0;
    totalMessagesSent_ = 0;
    totalDroppedMessages_ = 0;
    totalBuddyRecords_ = 0;
    totalCorrectBuddyRecords_ = 0;
    totalSleepTime_ = 0;
  }

  void addConvergenceTime(const uint32_t& t) {
    totalConvergenceTime_ += t;
  }

  void addSleepTime(const uint32_t& t) {
    totalSleepTime_ += t;
  }

  void incrementSleepStates() {
    totalSleepStates_ ++;
  }

  void incrementPresenceUpdates(void) {
    totalPresenceUpdates_++;
  }

  void incrementMessagesSent(void) {
    totalMessagesSent_++;
  }

  void incrementMessagesDropped(void) {
    totalDroppedMessages_++;
  }

  void incrementTotalBuddyRecords(void) {
    totalBuddyRecords_++;
  }

  void incrementTotalCorrectBuddyRecords(void) {
    totalCorrectBuddyRecords_++;
  }

  void addStateSwitch(const clientId_t& clientId, 
		      const uint32_t& timestamp, 
		      const ClientState& state) {
    stateSwitches_[clientId] = timestamp;
    state_[clientId] = state;
  }

  uint32_t getLastStateSwitch(const clientId_t& clientId) {
    if (stateSwitches_.find(clientId) == stateSwitches_.end()) {
      stateSwitches_[clientId] = 0;
    }
    
    return stateSwitches_[clientId];
  }

  inline ClientState getLastState(const clientId_t& clientId) {    
    return state_[clientId];
  }

  inline uint32_t getPresenceUpdatesCount(void) const {
    return totalPresenceUpdates_;
  }

  inline uint32_t getTotalConvergenceTime(void) const {
    return totalConvergenceTime_;
  }

  inline uint32_t getTotalMessagesSentCount(void) const {
    return totalMessagesSent_;
  }

  inline uint32_t getTotalMessagesDroppedCount(void) const {
    return totalDroppedMessages_;
  }

  inline uint32_t getTotalBuddyRecords(void) const {
    return totalBuddyRecords_;
  }

  inline uint32_t getTotalCorrectBuddyRecords(void) const {
    return totalCorrectBuddyRecords_;
  }

  inline uint32_t getTotalSleepTime(void) const {
    return totalSleepTime_;
  }

  inline uint32_t getTotalSleepStates(void) const {
    return totalSleepStates_;
  }

 private:
  uint32_t totalConvergenceTime_;
  uint32_t totalPresenceUpdates_;
  uint32_t totalMessagesSent_;
  uint32_t totalDroppedMessages_;
  uint32_t totalBuddyRecords_;
  uint32_t totalCorrectBuddyRecords_;
  uint32_t totalSleepTime_;
  uint32_t totalSleepStates_;

  std::hash_map<clientId_t, uint32_t> stateSwitches_;
  std::hash_map<clientId_t, ClientState> state_;
};

#endif // _STATS_H_
