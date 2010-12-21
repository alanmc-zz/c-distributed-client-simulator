#include <iostream>
#include "ClientSimulator.h"
#include "Client.h"

int main(int argc, char* argv[], char* envp[]) {
  // Run the simulator for our "heartbeat" protocol
  //HeartbeatSimulator<1000, 10, 60*60> simulator;
  //simulator.run();


  // Run the simulator for our "gossip" protocol
  GossipSimulator<1000, 20, 60*60*24*30*3> simulator;
  simulator.run();
}


