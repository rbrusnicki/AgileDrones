#ifndef FRAMEWORK_H
#define FRAMEWORK_H

/*
framework.hpp provides an abstract base class podBase_t to derive specific POD classes from (like e.g. a motorcommander POD). podBase_t has member functions that provide most functionality that is needed to integrate the specific PODs into the inter-POD-LCM-communication-network. It also lists constants used by multiple PODs.
*/

#include <sys/time.h>

#include <iostream>
#include <thread>
#include <mutex>
#include <cmath>
#include <map>
#include <typeinfo>

#include <glib.h>
#include <inttypes.h>


#include <lcm/lcm.h>
#include <lcmtypes/lcmtypes.hpp>
#include <lcm/lcm-cpp.hpp>

/* GENERAL PARAMETERS, potentially needed in multiple PODs */ 


//Drone general parameters
#define QUADMASS   0.946 	//[kg] @TODO this is with one battery
#define GRAVITY    9.81
#define QUADBISQRT 675.0 	//sqrt(1/quad.b)

//Drone Motor parameters
#define ZERORPMPWM 1000		//PWM value for no rotation of motors (0 RPM)
#define MAXPWM 1900		//PWM max value
#define IDLERPMPWM 1100		//PWM value to see motors spinning slowly
/*
#define AOMEGA2TOPWM 0.000275   //pwm = AOMEGA2TOPWM*(w)^2+COMEGA2TOPWM 
#define COMEGA2TOPWM 1062 
*/
#define AOMEGA2TOPWM 0.0003852   //pwm = AOMEGA2TOPWM*(w-BOMEGA2TOPWM)^2+COMEGA2TOPWM
#define BOMEGA2TOPWM 330.8 
#define COMEGA2TOPWM 1113 


#define ALPHAT 2.2e-6

#define TAKEOFFINTERVAL 1000	// time that drone is in takeoff mode after starting flight before transitioning to full flight mode [milliseconds]


//Safety regulations
#define MAXPOS_CRITICAL 9.0  		//max position range before critical status is issued[m]
#define MAXPOS_FATAL 	10.0 		//max position range before fatal status is issued[m]

#define MAXALT_CRITICAL 4.0		//max altitude before critical status is issued (note, this is positive, against convention of Z pointing downwards)[m]
#define MAXALT_FATAL 	5.0 		//max altitude before fatal status is issued (note, this is positive, against convention of Z pointing downwards)[m]

#define MAXORIENT_RP_CRITICAL 1.0	//max roll pitch angle critical ... [rad]
#define MAXORIENT_RP_FATAL    1.2	//max roll pitch angle fatal ... [rad]

#define MAXORIENT_Y_CRITICAL 3.0	//max yaw angle critical ... [rad]
#define MAXORIENT_Y_FATAL    3.14	//yaw angle  fatal ...[rad]

#define MAXVELPOS_CRITICAL   4.0	//max velocity critical ... [m/s]
#define MAXVELPOS_FATAL      5.0	//max velocity fatal ... [m/s]

#define MAXVELORIENT_PQ_CRITICAL   300.0	//max angular velocity roll pitch angle critical ... [rad/s]//@TODO choose bound, filter accel
#define MAXVELORIENT_PQ_FATAL      400.0	//max angular velocity roll pitch angle [rad/s]//@TODO choose bound, filter accel

#define MAXVELORIENT_R_CRITICAL    100.0	//max angular velocity yaw angle critical ...[rad/s]//@TODO choose bound, filter accel
#define MAXVELORIENT_R_FATAL       150.0	//max angular velocity yaw angle fatal ... [rad/s]//@TODO choose bound, filter accel

#define MAXACC_CRITICAL    	   300.0	//max acceleration critical ... ~3g [m/s^2] //@TODO choose bound, filter accel
#define MAXACC_FATAL       	   400.0	//max acceleration fatal ~4g [m/s^2]	    //@TODO choose bound, filter accel 


//CALLINTERVALS PODS: inverse rate at which glib mainloops of PODS are run [ms]
#define MAXPODDELAY_X 1.8 	 // POD computation interval can take at max MAXPODDELAY_X-times the POD's callinterval before timeout (error) is issued

#define CALLINTERVAL_DEFAULT 10
#define CALLINTERVAL_EXAMPLEPOD 10
#define CALLINTERVAL_TESTSENDER 10

#define CALLINTERVAL_IMUACQUISITION 8
#define CALLINTERVAL_STATEESTIMATORORIENTV1 10
#define CALLINTERVAL_STATEESTIMATORORIENTCF 10
#define CALLINTERVAL_DETECTORVIS 20
#define CALLINTERVAL_POSEESTIMVIS 20
#define CALLINTERVAL_REMOTECONTROLLER 20
#define CALLINTERVAL_WATCHDOG 10
#define CALLINTERVAL_CONTROLLERPDORIENT 10
#define CALLINTERVAL_CONTROLLERPDPOSE 10
#define CALLINTERVAL_CONTROLLERSOCORIENT 10
#define CALLINTERVAL_CONTROLLERSOCPOSE 10
#define CALLINTERVAL_MOTORCOMMANDER 10
#define CALLINTERVAL_SIMULATOR 5


//multi-POD communication, timing
#define MAXAGEMSGS_X 1.9  	 // stored message from a subscribed channel is considered out of date if age of last stored message is older than MAXAGEMSGS_X-times the expected receive interval of that message
#define MS2US 1000		 // conversion us to ms


//SENSOR STUFF
#define CALIBINTERVAL_IMU 2000	// length of calibration interval for IMU [milliseconds]


//MODES, FLAGS, ...
enum controlMode_t {CMODE_FATAL=-3,CMODE_CRITICAL,CMODE_NULL, CMODE_PIDORIENT, CMODE_PIDPOSE, CMODE_SOC};
enum statusDrone_t {DRONE_FATAL=-3, DRONE_CRITICAL, DRONE_WAITPODS, DRONE_IDLE, DRONE_TAKEOFF, DRONE_FLY};
enum statusPod_t   {POD_FATAL=-3, POD_CRITICAL, POD_INITING, POD_OK};
enum controllerAdjustmode_t   {CADJUSTMODE_NULL=0, CADJUSTMODE_P, CADJUSTMODE_D,CADJUSTMODE_BIAS};

using namespace std;


/*
Get timestamp
*/
int64_t GetTimeStamp() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(int64_t)1000000+tv.tv_usec;
};


/*
GEOMETRY
*/

static void quat2Euler(double* q, double *yaw, double *pitch, double *roll)
{
*roll = atan2(2*(q[0]*q[1]+q[2]*q[3]),(1-2*(pow(q[1],2)+pow(q[2],2))));
*pitch = asin(2*(q[0]*q[2] - q[3]*q[1] ) );
*yaw = atan2(2*(q[0]*q[3]+q[1]*q[2]),(1-2*(pow(q[2],2)+pow(q[3],2))));
}



static void Euler2quat(double* q, double *yaw, double *pitch, double *roll) {
    // Assuming the angles are in radians.
    double cy = cos(*yaw/2);
    double sy = sin(*yaw/2);
    double cp = cos(*pitch/2);
    double sp = sin(*pitch/2);
    double cr = cos(*roll/2);
    double sr = sin(*roll/2);

    q[0] = cy*cp*cr + sy*sp*sr;
    q[1] = sr*cp*cy - cr*sp*sy;
    q[2] = cr*sp*cy + sr*cp*sy;
    q[3] = cr*cp*sy - sr*sp*cy;
  }



std::mutex podMutex; //see comment in podBase_t::podMutex

//Forward declarations
class podBase_t;

//Classes

class podBase_t {
/*
describes base class to be used to derive new POD-classes (watchdog, controllers, ...)
*/

public: 

  string podName;
  
  agile::statusPod_t statusPod;   				//stores status of this POD
  agile::statusPod_t statusWatchdog;   				//stores status of watchdogPOD
  agile::statusDrone_t statusDrone;   				//stores status of Drone
  int64_t callInterval, timestampJetsonLastComputation, computationInterval; 	//stores callInterval for POD (for computation loop and statusUpdate loop), timestamp of last completed computation and the time passed since the last computation (computationInterval)
   //std::mutex podMutex; //Note: mutex put outside of class, as mutex-member requires implementation of class' copyconstructor. or requires a static mutex which makes all mutexs the same for all instances of that class! @TODO does that make this the same mutex variable for all PODS?!						
	
  //Communication
  lcm::LCM lcm;							//lcm object for lcm communication

  struct messageContainer_t {					//contains timestamp when most recent message was received, the expected interval of receiving this message, a pointer to the stored message and a pointer to the lcm subscription
	int64_t timestampJetsonLastReceived;
	int64_t receiveIntervalExpected;
	void* message;
	lcm::Subscription* subscription;
	};

  //messageAdmin to manage the  subscriptions of this POD to multiple channels
  typedef std::map<string, messageContainer_t> messageAdmin_t;
  messageAdmin_t messageAdmin;			//map that contains messageContainers that can be retrieved using the channelName of the message


  /*
  subscribe the POD to a given channel
  input: channelName, expected message receive interval in milliseconds, pointer where message of that channel is stored to, pointer to message handler function.
  Call might look like this: podWorker.subscribe("testdata", CALLINTERVAL_TESTSENDER, 	 &(podWorker.testdata), 	&podBase_t::handleMessage<agile::pose_t>);
  */
  template <class MessageType, class MessageHandlerClass>
  void subscribe(const std::string& channel, int64_t receiveIntervalExpected, void* messageStoredinPod,  		
               void (MessageHandlerClass::*handlerMethod)(const lcm::ReceiveBuffer* rbuf, const std::string& channel, const MessageType* msg)
    	       )
		{ 
		this->messageAdmin[channel].timestampJetsonLastReceived = 0;
		this->messageAdmin[channel].message 			= messageStoredinPod;
		this->messageAdmin[channel].receiveIntervalExpected 	= receiveIntervalExpected;
		this->messageAdmin[channel].subscription 		= this->lcm.subscribe(channel, handlerMethod, this);
		this->messageAdmin[channel].subscription->setQueueCapacity(1);
		};

  /*
  unsubscribe the POD from a given channel
  input: channelName
  */
   void unsubscribe(const std::string& channel)
		{
		printf("Unsubscribe from %s\n",channel.c_str());
		//Unsubscribe subscription
		this->lcm.unsubscribe(this->messageAdmin[channel].subscription);
		//delete entry in messageAdmin
		messageAdmin_t::iterator iterator = this->messageAdmin.find(channel); 
		if( iterator != this->messageAdmin.end() )
			this->messageAdmin.erase( iterator );
		else printf( "messageAdmin: subscription to unsubscribe from not found!\n" ) ;
		};

  //Constructor
  podBase_t(string podName, int64_t callInterval)
	{           
 	this->podName = podName; 
	this->computationInterval = 0;
	this->timestampJetsonLastComputation = GetTimeStamp();
	this->callInterval = callInterval;	
	this->statusWatchdog.status = POD_CRITICAL;
	this->statusDrone.status = DRONE_WAITPODS;
	this->subscribe("statusWatchdog",CALLINTERVAL_WATCHDOG,	 &(this->statusWatchdog), 	&podBase_t::handleMessage<agile::statusPod_t>);
	this->subscribe("statusDrone",CALLINTERVAL_WATCHDOG,	 &(this->statusDrone), 	&podBase_t::handleMessage<agile::statusDrone_t>);		
	};



  /*
  updates timestamp of last completed computation and the time passed since the last computation (computationInterval)
  */
  void updateComputationInterval() 		
	{
	int64_t now = GetTimeStamp();	
	this->computationInterval = now - this->timestampJetsonLastComputation;	
	this->timestampJetsonLastComputation = now;
	};

  /*
  inits timestamp of last completed computation and the computationInterval
  */
  void initComputationInterval() 		
	{
	int64_t now = GetTimeStamp();	
	this->computationInterval = 0;
	this->timestampJetsonLastComputation = now;
	};

  /*
  updates status object of POD and publishes status message
  input: status
  */
  void publishStatus(int16_t status) 
	{
	this->statusPod.timestampJetson = GetTimeStamp();	
 	this->statusPod.status = status;	
	this->statusPod.podName = this->podName;
	string podName = this->statusPod.podName;
	podName[0] = toupper(podName[0]);
  	this->lcm.publish ("status" + podName, &(this->statusPod));
	};

  /*
  updates timestamp when most recent message was received on given channel
  input: channelName
  */
  void updateMessageLastReceived(string channelName)
	{
	this->messageAdmin[channelName].timestampJetsonLastReceived = GetTimeStamp();
	};

  /*
  checks if all stored messages are sufficiently uptodate
  input: callInterval (of POD)
  output: boolean result of check
  */
  bool checkMessagesUptodate()
	  {
		bool allUptodate = true;
		int64_t currentTimestamp = GetTimeStamp();
		int64_t updateDelta;

		messageAdmin_t::iterator iterator = this->messageAdmin.begin();
		messageAdmin_t::iterator end = this->messageAdmin.end();

		while (((allUptodate) && (iterator != end)))
		{
		  if (this->statusDrone.status == DRONE_WAITPODS) updateDelta = 0; //drone still waiting for all PODs to be up an running
		  else		   updateDelta = currentTimestamp - iterator->second.timestampJetsonLastReceived;

		   allUptodate = (updateDelta <  MAXAGEMSGS_X*iterator->second.receiveIntervalExpected*MS2US) ;	
		   if (!allUptodate) printf("message -%s- delayed in POD -%s- at time %" PRId64"\n",iterator->first.c_str(),this->podName.c_str(), currentTimestamp);	
		    ++iterator;		    
		}		
	  return allUptodate;
	  };


  /*
  is called by lcm object on receiving a message during lcm.handle() and stores the received message in the POD object
  input: --is default lcm-function header --
  note: the subscribe-function needs a function pointer to this function. Therefore, the template needs to be resolved to be able to provide a function pointer (this is not possible for template functions. subscribe-function might be called like this: podWorker.subscribe("testdata", 	 &(podWorker.testdata), 	&podBase_t::handleMessage<agile::pose_t>);
  */
  template<class msgType_t> 
  void handleMessage(const lcm::ReceiveBuffer* rbuf,
		        const std::string& chan, 
		        const msgType_t* msg)
  {				
		std::lock_guard<std::mutex> guard(podMutex);
		//store mesage
		* ( (msgType_t*)((this)->messageAdmin[chan.c_str()].message )) = *msg;
		//update messageadmin that message was received
		this->updateMessageLastReceived(chan.c_str()); 
 
  };




  /*
  returns name of POD
  */
	
  string returnPodName()
  {	
	return  podName;
  };

  //abstract functions
  //---------

  /*
  abstract function for loop function for computations in this specific POD, requires implementation in yourPOD.cpp!
   */
  static gboolean gtimerfuncComputations (gpointer data); 

  /*
  abstract function for loop function for publishing statusPod, requires implementation in yourPOD.cpp!
   */
  static gboolean gtimerfuncStatusPod (gpointer data);
  
};


/*
Implementation of listener function
*/

void listen(podBase_t* podWorker)
{

while (true)
	{
	podWorker->lcm.handle(); //listens to all channels subscribed to by this lcm object.
	}
}



#endif
