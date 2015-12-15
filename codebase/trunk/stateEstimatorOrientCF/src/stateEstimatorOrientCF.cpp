
#include "stateEstimatorOrientCF.hpp"


using namespace std;

void complimentaryfilter(double yaw_old,  double pitch_old, double roll_old,  double dt, double* sensorIMU, double* euler_hat)
{



double gyroAngleUpdate_acc_threshold   = 0.002; //@TODO adjust!
double gyroAngleUpdate_acc_weight      = 0.001;


//Rotation of angular velocity vector from Bodyframe to Worldframe, inverted Wronskian (body rates p-q-r to euler rates yaw pitch roll)
double w_euler[3];

double p = sensorIMU[3];
double q = sensorIMU[4];
double r = sensorIMU[5];

w_euler[0] = sin(roll_old)/cos(pitch_old)*q + cos(roll_old)/cos(pitch_old)*r;
w_euler[1] = cos(roll_old)*q - sin(roll_old)*r;
w_euler[2] = p + sin(roll_old)*tan(pitch_old)*q+ cos(roll_old)*tan(pitch_old)*r;

//Integrate gyroscope data
double roll  = roll_old  + (w_euler[2] ) * dt/2; //@TODO why by half?
double pitch = pitch_old + (w_euler[1] ) * dt/2;
double yaw   = yaw_old   + (w_euler[0] ) * dt/2; 

  
//Compensate for drift with accelerometer data if in un-accelerated flight
double imuaccl_abs = sqrt(pow(sensorIMU[0],2) + pow(sensorIMU[1],2) + pow(sensorIMU[2],2));

if ( (imuaccl_abs > (1-gyroAngleUpdate_acc_threshold)*(GRAVITY)) && (imuaccl_abs < (1+gyroAngleUpdate_acc_threshold)*(GRAVITY)))
    {
    double roll_hat_acc = atan(sensorIMU[1]/sensorIMU[2]);    
    roll         = roll * (1-gyroAngleUpdate_acc_weight) + roll_hat_acc * gyroAngleUpdate_acc_weight;
    
    double pitch_hat_acc= asin(sensorIMU[0]/GRAVITY) ;
    pitch        = pitch * (1-gyroAngleUpdate_acc_weight) + pitch_hat_acc *gyroAngleUpdate_acc_weight;
    }

euler_hat[0] = yaw;
euler_hat[1] = pitch;
euler_hat[2] = roll;


}


/*
Implementation of loop function for computations in this specific POD
*/

gboolean podBase_t::gtimerfuncComputations (gpointer data) {
	
	  /* General Infrastructure (maintain this structure!) */  	  
	  stateEstimatorOrientCF_t* podWorker = (stateEstimatorOrientCF_t*) data;
	  std::lock_guard<std::mutex> guard(podMutex);
	

   	   /*--------*/

	  if(podWorker->statusPod.status == POD_OK)
	  {
	  	  /* Computations */	



		// Correct new measurements from bias

		double imu_orig[6];

		imu_orig[0] = podWorker->imudata.accel[0] - podWorker->biases.accel[0];
		imu_orig[1] = podWorker->imudata.accel[1] - podWorker->biases.accel[1];
		imu_orig[2] = podWorker->imudata.accel[2] - podWorker->biases.accel[2]; //printf("%f %f\n",podWorker->imudata.accel[2],podWorker->biases.accel[2]);
		imu_orig[3] = podWorker->imudata.gyro[0] - podWorker->biases.gyro[0];
		imu_orig[4] = podWorker->imudata.gyro[1] - podWorker->biases.gyro[1];
		imu_orig[5] = podWorker->imudata.gyro[2] - podWorker->biases.gyro[2];

		//Rotate measurements from IMU system to body-frame (assuming it is first 90deg yaw, then 180 to take vectorin IMUsyste to represent in bodyframe sys!

		//accels
		double imu_trafo[6];

		if (strcmp(podWorker->imuRawChannel.c_str(),"imuRaw")==0)
		{
			imu_trafo[0] = -imu_orig[1];
			imu_trafo[1] = -imu_orig[0];
			imu_trafo[2] = -imu_orig[2];
		
			//rot
			imu_trafo[3] = -imu_orig[4];
			imu_trafo[4] = -imu_orig[3];
			imu_trafo[5] = -imu_orig[5];
		}
		else
		{
			imu_trafo[0] = imu_orig[0];
			imu_trafo[1] = imu_orig[1];
			imu_trafo[2] = imu_orig[2];
		
			//rot
			imu_trafo[3] = imu_orig[3];
			imu_trafo[4] = imu_orig[4];
			imu_trafo[5] = imu_orig[5];
		}

		
				

		//prepare current estimates
		double yaw_cur, pitch_cur, roll_cur;
		quat2Euler(podWorker->stateVariances.orient, &(yaw_cur), &(pitch_cur),&(roll_cur));

		//feed complimentary filter
		double euler_hat[3];
		int64_t nowCompUpdate = GetTimeStamp();

		double dt = (nowCompUpdate-podWorker->stateVariances.timestampJetson)/(1000000.0);//printf("dt: %f\n",dt);


		if (dt>10) dt=0.015; //initial timestamp
		complimentaryfilter(yaw_cur, pitch_cur, roll_cur,dt,imu_trafo,euler_hat);		

		//transform to quaternions and update stateVariances
		
		Euler2quat(podWorker->stateVariances.orient, &(euler_hat[0]), &(euler_hat[1]), &(euler_hat[2]));
		podWorker->stateVariances.veloOrientBody[0] = imu_trafo[3];
		podWorker->stateVariances.veloOrientBody[1] = imu_trafo[4];
		podWorker->stateVariances.veloOrientBody[2] = imu_trafo[5];

		podWorker->stateVariances.timestampJetson = nowCompUpdate;

		/* Publishing computation result, here status update of drone status */ 

		// statusDrone - publish
		podWorker->lcm.publish ("stateVariancesOrientCF", &podWorker->stateVariances); 	

	  /*---------*/
	 }

	 podWorker->updateComputationInterval();
	 return TRUE;
	 /*---------*/
	}
/*--------------------------*/





/*
Implementation of loop function for publishing statusPod
*/

gboolean podBase_t::gtimerfuncStatusPod (gpointer data) {

	  /*General Infrastructure (maintain this infrastructure!)*/
	  stateEstimatorOrientCF_t* podWorker = (stateEstimatorOrientCF_t*) data;
	  /*---------*/
	

  	  
	  /*Computation statusPOD*/	  	  
	  if (podWorker->computationInterval > MAXPODDELAY_X*podWorker->callInterval*MS2US)	
	  {
		printf("stateEstimatorOrientCF: delay occured of % " PRId64 "us!\n",
						podWorker->computationInterval);
		podWorker->statusPod.status = POD_FATAL;
 	  }
	  else if (!podWorker->checkMessagesUptodate())
	  {
		podWorker->statusPod.status = POD_CRITICAL;
	  }
	  else
	  {
		if(podWorker->isGotInitialFeatures == -1){
			printf("Waiting for IMU Calibration to end...\n");
			podWorker->isGotInitialFeatures = 0;
		}
		else if(podWorker->isGotInitialFeatures == 0)
		{
			if (podWorker->features.featureDirectionVersor[0][0] != 0 || 
			    podWorker->features.featureDirectionVersor[0][1] != 0 ||
			    podWorker->features.featureDirectionVersor[0][2] != 0   )
			{		
		 	   	printf("IMU calibration ok! Continuing...\n");
			   	podWorker->isGotInitialFeatures == 1;
				
				podWorker->biases.accel[0] = podWorker->features.featureDirectionVersor[0][0];
				podWorker->biases.accel[1] = podWorker->features.featureDirectionVersor[0][1];
				podWorker->biases.accel[2] = (podWorker->features.featureDirectionVersor[0][2]-GRAVITY);

				podWorker->biases.gyro[0] = podWorker->stateVariances.imuBiasGyro[0];
				podWorker->biases.gyro[1] = podWorker->stateVariances.imuBiasGyro[1];
				podWorker->biases.gyro[2] = podWorker->stateVariances.imuBiasGyro[2];
				
				//set estimates to intial value
				podWorker->stateVariances.orient[0] = 1.0;
				podWorker->stateVariances.orient[1] = 0.0;				
				podWorker->stateVariances.orient[2] = 0.0;				
				podWorker->stateVariances.orient[3] = 0.0;		

				podWorker->stateVariances.position[0] = 0.0;
				podWorker->stateVariances.position[1] = 0.0;		
				podWorker->stateVariances.position[2] = 0.0;

				podWorker->stateVariances.veloPositionBody[0] = 0.0;								
				podWorker->stateVariances.veloPositionBody[1] = 0.0;								
				podWorker->stateVariances.veloPositionBody[2] = 0.0;

				podWorker->stateVariances.veloOrientBody[0] = 0.0;								
				podWorker->stateVariances.veloOrientBody[1] = 0.0;								
				podWorker->stateVariances.veloOrientBody[2] = 0.0;																
				

				//after getting the initial versors, unsubscribe
				podWorker->unsubscribe("stateVariancesOrientCF");		
			
				podWorker->isGotInitialFeatures = true;			

				podWorker->statusPod.status = POD_OK;
	 	
			}
			else if (strcmp(podWorker->imuRawChannel.c_str(),"imuRawSim")==0)
				{
				printf("IMU calibration ok! Continuing...it's only simulated!\n");
			   	podWorker->isGotInitialFeatures == 1;
				
				podWorker->biases.accel[0] = 0.0;
				podWorker->biases.accel[1] = 0.0;
				podWorker->biases.accel[2] = 0.0;

				podWorker->biases.gyro[0] = 0.0;
				podWorker->biases.gyro[1] = 0.0;
				podWorker->biases.gyro[2] = 0.0;
				
				//set estimates to intial value
				podWorker->stateVariances.orient[0] = 1.0;
				podWorker->stateVariances.orient[1] = 0.0;				
				podWorker->stateVariances.orient[2] = 0.0;				
				podWorker->stateVariances.orient[3] = 0.0;		

				podWorker->stateVariances.position[0] = 0.0;
				podWorker->stateVariances.position[1] = 0.0;		
				podWorker->stateVariances.position[2] = 0.0;

				podWorker->stateVariances.veloPositionBody[0] = 0.0;								
				podWorker->stateVariances.veloPositionBody[1] = 0.0;								
				podWorker->stateVariances.veloPositionBody[2] = 0.0;

				podWorker->stateVariances.veloOrientBody[0] = 0.0;								
				podWorker->stateVariances.veloOrientBody[1] = 0.0;								
				podWorker->stateVariances.veloOrientBody[2] = 0.0;																
				

				//after getting the initial versors, unsubscribe
				podWorker->unsubscribe("stateVariancesOrientCF");
				podWorker->unsubscribe("features");				
			
				podWorker->isGotInitialFeatures = true;			

				podWorker->statusPod.status = POD_OK;
				};
		};


		

	  };
	
	/*---------*/

	/*Publishing statusPOD*/
	 podWorker->publishStatus(podWorker->statusPod.status);
	/*---------*/

	return TRUE;
};
/*--------------------------*/




/*
main to initialize and start the POD
*/

int main (int argc, char** argv) {

  /* General Infrastructure: setup (maintain this structure!)  */  

  // 1) Create the app
  stateEstimatorOrientCF_t podWorker = stateEstimatorOrientCF_t("stateEstimatorOrientCF",CALLINTERVAL_STATEESTIMATORORIENTCF); 	 

  if (argc<2) {
		printf("Please provide imuRaw-Channel used for estimation!\n");
		return EXIT_FAILURE;
		};

  podWorker.imuRawChannel = argv[1];

  // 2) Create LCM
  if (!podWorker.lcm.good()) 
    return 1;

  // 3) Subscribe this POD to channels
  podWorker.subscribe(podWorker.imuRawChannel.c_str(), CALLINTERVAL_IMUACQUISITION, &(podWorker.imudata), 	
	&podBase_t::handleMessage<agile::imuRaw_t>);

  podWorker.subscribe("stateVariancesOrientCF", CALLINTERVAL_IMUACQUISITION, 
		&(podWorker.stateVariances), &podBase_t::handleMessage<agile::stateVariances_t>);

    podWorker.subscribe("features", CALLINTERVAL_IMUACQUISITION, &(podWorker.features),
		&podBase_t::handleMessage<agile::features_t>);
  /*---------*/

  


  /*  POD-specific init procedures  */
  // Update and publish status of Pod (this POD is the first to start)
  printf("Initializing stateEstimatorOrientCF...\n");
  podWorker.publishStatus(POD_INITING);	


  printf("Initializing stateEstimatorOrientCF... DONE\n");
  /*---------*/





  /*  General Infrastructure: loops (maintain this structure!)  */

  //  POD-wait for watchdogPOD to be up and running (POD_OK)  before continue

  printf("Waiting for watchdog to be POD_OK...\n");
  while (podWorker.statusWatchdog.status!=POD_OK)
  {
	podWorker.lcm.handle();
  }
  printf("Watchdog POD_OK! Continuing...\n");

  
  podWorker.initComputationInterval();

  //Create message listener thead	
  std::thread listenerThread(listen,&podWorker);

  // Create mainloop, add loop functions, run
  GMainLoop* mainloop = g_main_loop_new (NULL, FALSE);

  guint timerComputations = g_timeout_add (podWorker.callInterval, podBase_t::gtimerfuncComputations, (gpointer)&podWorker);  
  guint timerStatusPod =    g_timeout_add (podWorker.callInterval, podBase_t::gtimerfuncStatusPod,    (gpointer)&podWorker);	

  g_main_loop_run (mainloop);

  /*---------*/

  return 0;
}






