/*============================================================================
// Name        : Pheromones.cpp
// Authors     : Brandon Thomas, Abolfazl Razi
// Version     : 2.0
// Lat Update: : 2017-01-15
// Copyright   :
// Description :
//============================================================================*/
#include "Pheromones.hh"

namespace mpi = boost::mpi;
using namespace boost::interprocess;
using boost::lexical_cast;


// TODO: Need lots of error checking here...

Pheromones::Pheromones() {
	swarm_ = 0;
	env_ = 0;
	world_ = 0;
}

Pheromones::~Pheromones() {
	if (swarm_->options.useCluster) {

		cout << "my rank is " << world_->rank() << " starting ~environment()" << endl;
		env_->~environment();
		//MPI_Finalize();
		cout << "~environment() done" << endl;

	}
	else {
		for (auto mq = smq_.begin(); mq != smq_.end(); ++mq) {
			(*mq)->~message_queue();
		}
	}
}

void Pheromones::init(Swarm *s) {
	swarm_ = s;
	unsigned int nSubPar= swarm_->options.swarmSize * swarm_->options.models.size()+1; //razi:number of subparticles, +1 since starts from 1

	// Using MPI
	if (swarm_->options.useCluster && swarm_->options.clusterSoftware == "mpi" || swarm_->options.clusterSoftware == "slurm") {
		// Set up our MPI environment and communicator
		std::cout<<"Pheromones Initialization: MPI\n";
		env_ = new mpi::environment();
		cout << "Defined mpi environment" << endl;
		world_ = new mpi::communicator();
		cout << "Defined world communicator" << endl;
		//  boost::mpi::environment env;
		 // boost::mpi::communicator world_;
		//scout << "Hello World! from process " << world_.rank() << endl;
	}
	else if (swarm_->options.clusterSoftware == "BNF2mpi"){
			cout << "Detected BNF2mpi in Pheromones init()" << endl;
			env_ = new mpi::environment();
			cout << "Defined mpi environment" << endl;
			world_ = new mpi::communicator();
			cout << "Defined world communicator" << endl;
			int myrank = world_->rank();
			cout << "My rank is " << myrank << endl;
	}
	// Using IPC
	else {

		//std::cout << "init ipc" << std::endl;
		if (s->isMaster) {
			std::cout<<"Pheromones Initialization: IPC: Master\n";
			for (unsigned int i = 0; i <= nSubPar ; ++i) {
				//std::cout << "creating: " << std::toString(static_cast<long long int>(i)) << std::endl;
				message_queue *smq;

				try {
					smq = new message_queue(create_only, toString(i).c_str(), 100, 1000);
				} catch (boost::interprocess::interprocess_exception e) {
					message_queue::remove(toString(i).c_str());
					smq = new message_queue(create_only, toString(i).c_str(), 100, 1000);
				}
				smq_.push_back(smq);
			}
		}
		else {
			std::cout<<"Pheromones Initialization: IPC: Slave\n";
			for (unsigned int i = 0; i <= nSubPar; ++i) {
				message_queue *smq = new message_queue(open_only, toString(i).c_str());
				smq_.push_back(smq);
				//std::cout << "opening: " << std::toString(static_cast<long long int>(i)) << " with max size of " << smq_[i]->get_max_msg() << std::endl;
			}
		}
	}

	swarm_->commInit = true;
}

void Pheromones::sendToSwarm(int senderID, signed int receiverID, int tag, bool block, std::vector<std::string> &message, int messageID) {
	// Using MPI
	if (swarm_->options.useCluster) {
		std::vector<int> receivers;

		int convertedTag;
		// Construct the swarmMessage
		swarmMessage smessage;
		smessage.sender = senderID;

		if(world_->rank()==0){

			convertedTag = tag+(receiverID*10000);

		}else{

			convertedTag = tag+(world_->rank()*10000);

		}


		// Sending to the entire swarm
		if (receiverID == -1) {
			//std::cout << "sending to entire swarm because receiver is -1" << std::endl;

			// TODO: Replace this exchange with a world_->sendrecv()
			smessage.tag = toString(GET_RUNNING_PARTICLES);
			std::cout << "trying to get list of running particles.." << std::endl;
			// First we need to get a list of all running particles from the master


			std::string serializedMessage = serializeSwarmMessage(smessage);
			cout << "RRR sending serializedMessage GET RUNNING PARTICLES 11 = " << serializedMessage << endl;
			world_->send(0, GET_RUNNING_PARTICLES, serializedMessage);
			//std::cout << "trying to receive list of running particles.." << std::endl;
			usleep(10000);

			// This swarmMessage will hold the list of running particles received from master
			serializedMessage.clear();
			world_->recv(0, SEND_RUNNING_PARTICLES, serializedMessage);
			std::cout << "received list of running particles.." << std::endl;
			swarmMessage rsmessage = deserializeSwarmMessage(serializedMessage);

			//for (auto p: runningParticles) {
			for (auto p = rsmessage.message.begin(); p != rsmessage.message.end(); ++p) {
				std::cout << "adding receiver: " << *p << std::endl;
				receivers.push_back(stoi(*p));
			}

		}
		else {

			cout << "RRR sendToSwarm senderID: " << senderID << " receiverID: " << receiverID << " tag: " << tag << " Converted tag: " << convertedTag <<endl;

			std::cout << "Sending tag " << tag << " to: " << receiverID << " converted tag: " << convertedTag << std::endl;
			// If we're not sending to the entire swarm, put only the target pID into the receivers list
			receivers.push_back(receiverID);
		}

		smessage.tag = toString(convertedTag);

		//for (auto m: mpiMessage) {
		for (auto m = message.begin(); m != message.end(); ++m) {
			//std::cout << "adding: " << *m << std::endl;
			smessage.message.push_back(*m);
		}

		std::string smString;
		{
		smString = serializeSwarmMessage(smessage);
		}
		const char *serializedMessage = smString.c_str();
		cout << "send message is serializedMessage = " << serializedMessage << " or smString = " << smString << endl;

		// Loop through receivers and perform the send operation
		for (std::vector<int>::iterator i = receivers.begin(); i != receivers.end(); ++i) {
			// Blocking send
			cout << "receiverID: " << endl;
			cout << "world size: " << world_->size() << endl;
			if(receiverID < world_->size()){

				usleep(10000);


				if (block) {
					std::cout << "attempting a block send from " << senderID << " to " << receiverID << std::endl;
					try{
//					world_->send(receiverID, convertedTag, serializedMessage, smString.length());
						world_->send(receiverID, convertedTag, smString);
					} catch (interprocess_exception& e) {
						cout << e.what( ) << std::endl;
			    	}
					std::cout << "block send from " << senderID << " to " << receiverID << " succeeded" << std::endl;
				}
				// Non-blocking send
				else {
					std::cout << "attempting a non-block send from " << senderID << " to " << receiverID << std::endl;
					//recvRequest_ = world_->isend(receiverID, convertedTag, serializedMessage, smString.length());
					recvRequest_ = world_->isend(receiverID, convertedTag, smString);
					std::cout << "non-block send from " << senderID << " to " << receiverID << " succeeded" << std::endl;
					//recvStatus_ = recvRequest_.wait();
					//std::cout << "tag: " << recvStatus_.tag() << std::endl;
					//std::cout << "error: " << recvStatus_.error() << std::endl;
				}

			}
		}
	}else {
		// Using IPC
		std::vector<int> receivers;

		// Construct the swarmMessage
		swarmMessage smessage;
		smessage.sender = senderID;

		int nPar = 0; //Raquel: added a counter for debugging

		// Sending to the entire swarm
		if (receiverID == -1) {
			//std::cout << "sending to entire swarm because receiver is -1" << std::endl;
			// First we need to get a list of all running particles from the master

			// Set tag to tell master we need a list of running particles
			smessage.tag = toString(GET_RUNNING_PARTICLES);
			//std::cout << "trying to get list of running particles.." << std::endl;

			// Serialize the message
			std::string serializedMessage = serializeSwarmMessage(smessage);
			serializedMessage.resize(1000);

			smq_[0]->send(serializedMessage.data(), sizeof(serializedMessage), 0);

			//std::cout << "trying to receive list of running particles.." << std::endl;

			// This swarmMessage will hold the list of running particles received from master

			message_queue::size_type recvd_size;

			// Receive and de-serialize the message
			std::stringstream iss;
			serializedMessage.clear();
			serializedMessage.resize(1000);
			unsigned int priority;

			smq_[0]->receive(&serializedMessage[0], 1000, recvd_size, priority);

			serializedMessage.resize(recvd_size);
			swarmMessage rsmessage = deserializeSwarmMessage(serializedMessage);

			//std::cout << "received list of running particles.." << std::endl;

			//for (auto p: runningParticles) {
			for (auto p = rsmessage.message.begin(); p != rsmessage.message.end(); ++p) {
				//std::cout << "adding receiver: " << *p << std::endl;
				receivers.push_back(stoi(*p));
				nPar++; //Raquel: added a counter for debugging

			}

			cout << "RAQUEL: Found " << nPar << " particles running." << endl; //Raquel: added for debugging

		}
		else {
			//std::cout << "Sending to: " << receiverID << std::endl;
			// If we're not sending to the entire swarm, put only the target pID into the receivers list
			receivers.push_back(receiverID);
		}

		// Set the message tag as specified by the sender
		smessage.tag = toString(tag);

		// Add the message array to the swarmMessage
		//for (auto m: mpiMessage) {
		for (auto m = message.begin(); m != message.end(); ++m) {
			//std::cout << senderID << " adding: " << *m << std::endl;
			smessage.message.push_back(*m);
		}

		// Set a random messageID
		if (messageID == -1) {
			messageID = rand();
		}

		smessage.id = messageID;

		// Serialize the swarmMessage
		std::string serializedMessage = serializeSwarmMessage(smessage);
		serializedMessage.resize(1000);

		// Loop through receivers and perform the send operation
		for (std::vector<int>::iterator i = receivers.begin(); i != receivers.end(); ++i) {
			std::cout << "### " << senderID << " sending " << smessage.tag << " to " << *i	<< ". ser: " << serializedMessage.data() << std::endl;
			//smq_[*i]->send(serializedMessage.data(), serializedMessage.size(), 0);
			//Raquel: added for debbuging
			bool sendAttempt2 = smq_[*i]->try_send(serializedMessage.data(), serializedMessage.size(), 0);


			if (sendAttempt2 == false){
				cout << "RAQUEL: queue was full when message was sent" << endl;

			}else{ cout << "RAQUEL: sending success" << endl; }
			
			//std::cout << "sent" << std::endl;
			if (block) {
				bool foundMessage = true;
				while (foundMessage) {
					usleep(150000);
					//std::cout << senderID << "loop" << std::endl;

					// TODO: This non-erasing recvMessage might slow down the receiver finding the message since it
					// requires a re-send every time we check

					// If we're blocking, we need to repeatedly check to see if the message still exists in the queue
					if (recvMessage(senderID, receiverID, tag, false, univMessageReceiver, false, messageID)) {
						foundMessage = true;
					}
					else {
						foundMessage = false;
					}
				}
			}
		}
	}
}

int Pheromones::recvMessage(signed int senderID, const int receiverID, int tag, bool block, swarmMsgHolder &messageHolder, bool eraseMessage, int messageID) {
	int numMessages = 0;
	std::string serializedMessage; //Raquel: moved this from inside the loop to here so it's not redeclared
	serializedMessage.resize(1000);
	swarmMessage smessage;

	if (swarm_->options.useCluster) {
		//swarmMessage smessage;
		//std::string serializedMessage;
		cout << "RRR recvMessage senderID: " << senderID << " receiverID: " << receiverID << " tag: " << tag <<endl;
		int convertedTag;
		std::string msg;

		while (1) {
			//std::cout << "rcv loop" << std::endl;
			serializedMessage.resize(1000);
			usleep(10000);
			if (boost::optional<boost::mpi::status> recvStatus = world_->iprobe(senderID, boost::mpi::any_tag)) {
				cout << "status tag: " << recvStatus->tag() << std::endl;
				cout << "status source: " << recvStatus->source() << std::endl;

				boost::optional<int> msgLength = recvStatus->count<char>();
				char smChar[*msgLength];
				//char smChar[1000];

				//cout << "recvStatus->tag() " << recvStatus->tag() << " DONE_BREEDING " << DONE_BREEDING << endl;

				//if (block) {
				cout << "trying a blocking receive to receiverID " << receiverID << " from senderID " << senderID << " real source: " << recvStatus->source() << endl;
				cout << "Tag: " << tag << "; smChar length: " << sizeof(smChar) << "; *msgLength: " << *msgLength << endl;
				//world_->recv(senderID, tag, smChar, *msgLength);
				//world_->recv(senderID, recvStatus->tag(), smChar, *msgLength);
					world_->recv(senderID, recvStatus->tag(), msg);
				//cout <<"RRR received message is smChar = " << smChar << endl;
				cout <<"RRR received message is = " << msg << endl;

				cout << "Message size is " << msg.length() << endl;

				//world_->recv(boost::mpi::any_source, tag, *msgLength);
				block = false;
				cout << "RRR passed receive step." << endl;


				//smessage = deserializeSwarmMessage(std::string(smChar));
				if(msg.length()>0){
					smessage = deserializeSwarmMessage(msg);


				}else{
					cout << "MESSAGE EMPTY!!! Reconstructing from tag.." << endl;
					smessage.tag = std::to_string(recvStatus->tag());
					smessage.sender = senderID;
					smessage.id = messageID;
				}

				cout << "RRR deserialized message" << endl;
				serializedMessage.clear();
				cout << "Cleaned serialized message" << endl;
				// If we have any messages in our messageHolder, let's process them
				//std::cout << "messageholder not empty: " << smessage.tag << ":" << smessage.sender << std::endl;
				if(recvStatus->source()==0){

					convertedTag=stoi(smessage.tag)-(world_->rank()*10000);


				}else{

					convertedTag=stoi(smessage.tag)-(recvStatus->source()*10000);
				}
				// Make sure our message matches the sender, tag, and id we requested
				if ( (tag == -1 || tag == convertedTag) && (senderID == -1 || senderID == smessage.sender) && (messageID == -1 || messageID == smessage.id)) {
					// Insert the message into our message holder and increment numMessages
					std::cout << "inserting " << std::endl;
					messageHolder.insert(std::pair<int, swarmMessage>(convertedTag, smessage));

					++numMessages;

					// If user doesn't want to erase the message, put it back in the queue
					if (!eraseMessage) {
						std::cout << "Not erasing message" << std::endl;
						sendToSwarm(senderID, receiverID, convertedTag, false, smessage.message);
					}

					// Clear out the smessage for next use
					clearSwarmMessage(smessage);
				}
				else {
					//std::cout << "putting it back in the queue..." << std::endl;
					sendToSwarm(senderID, receiverID, convertedTag, false, smessage.message);
				}
			}
			else if (!block) {
				break;
			}
		}
	}
	else {
		//cout << "RAQUEL entering recvMessage entering while loop" << endl;
		//cout << "RAQUEL receiver ID: " << receiverID << " senderID: " << senderID << endl;
		unsigned int priority = 0;

		while(1) {
			//cout << "RAQUEL inside recvMessage entered while loop" << endl;

			message_queue::size_type recvd_size;

			//std::cout << "rcv loop" << std::endl;
			if (block) {
//std::cout << "trying a blocking receive to " << receiverID << " from " << senderID << std::endl;

				smq_[receiverID]->receive(&serializedMessage[0], 1000, recvd_size, priority);


				block = false;
//std::cout << "blocking receive to " << receiverID << " from " << senderID << " succeeded with recv_size of " << recvd_size << " and message of " << serializedMessage << std::endl;
			}
			else {

//std::cout << "trying a non-blocking receive to " << receiverID << " from " << senderID << std::endl;
				bool hasMessage = smq_[receiverID]->try_receive(&serializedMessage[0], 1000, recvd_size, priority);

//std::cout << "non-blocking receive to " << receiverID << " from " << senderID << " succeeded with recv_size of " << recvd_size << " and bool of " << hasMessage << std::endl;
				if (!hasMessage) {
//std::cout << "breaking" << std::endl;
					break;
				}
			}
			// If we have any messages in our messageHolder, let's process them

			// De-serialize the smessage
			serializedMessage.resize(recvd_size);
			swarmMessage smessage = deserializeSwarmMessage(serializedMessage);

			serializedMessage.clear();
			serializedMessage.resize(1000);

sleep(1);

			//razi: uncomment later
			//std::cout << "smessage not empty: " << smessage.tag << std::endl;
			//std::cout << "tag: " << tag << ":" << smessage.tag << std::endl;
			//std::cout << "sender: " << tag << ":" << smessage.sender << std::endl;
			//std::cout << "id: " << tag << ":" << smessage.id << std::endl;

			// Make sure our message matches the sender, tag, and id we requested
			if ( (tag == -1 || stoi(smessage.tag) == tag) && (senderID == -1 || senderID == smessage.sender) && (messageID == -1 || messageID == smessage.id)) {
				// Insert the message into our message holder and increment numMessages
				messageHolder.insert(std::pair<int, swarmMessage>(stoi(smessage.tag), smessage));
//std::cout << "storing pair with tag of " << stoi(smessage.tag) << std::endl;
				++numMessages;

				// If user doesn't want to erase the message, put it back in the queue
				if (!eraseMessage) {
					//std::cout << "noerase" << std::endl;
					sendToSwarm(senderID, receiverID, tag, false, smessage.message);
				}
			}
			else {
				// If this isn't our message, put it back in the queue. This will go SLOW
				// unless we use a different queue for every particle

//std::cout << "putting it back in the queue..." << std::endl;
				sendToSwarm(smessage.sender, receiverID, stoi(smessage.tag), false, smessage.message, smessage.id);

				if (!block) {
					break;
				}
			}
		}
	}
	//cout << "RAQUEL inside recvMessage exiting with num messages: " << numMessages << endl;

	return numMessages;





}

void Pheromones::clearSwarmMessage(swarmMessage& sm) {
	sm.tag.clear();
	sm.id = 0;
	sm.sender = 0;
	sm.message.clear();
}

std::string Pheromones::serializeSwarmMessage(swarmMessage sm) {
	std::stringstream oss;
	{
	boost::archive::text_oarchive oa(oss);
	oa << sm;
	}
	std::string serializedMessage(oss.str());

	return serializedMessage;
}

Pheromones::swarmMessage Pheromones::deserializeSwarmMessage(std::string sm) {


	swarmMessage smessage;

	//char ch = sm.back();
	//cout << "Last character @" << ch << "@" << endl;
	if(sm.empty()){
	//if(ch=='\0'){
		//sm.back() = '\n';
		sm="\n";
		cout << "String empty" << endl;

	}else{

		//cout << "last character is not empty, erasing" << endl;
		//sm.erase(sm.size()-1);
		//ch = sm.back();
		//cout << "new last character @" << ch << "@"<< endl;
	}



	cout << "serializing message start SM: " << sm << endl;
	{
	std::stringstream iss;
	iss.str(sm);
	boost::archive::text_iarchive ia(iss);
	cout << "done serializing message" << endl;
	ia >> smessage;
	}
	return smessage;
}

int Pheromones::getRank() {
	return world_->rank();
}