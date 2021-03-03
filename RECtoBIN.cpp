/*
====================================================================================================
This script was developed with close assistance by the FLIR Systems Support Team, and is adapted 
from the example AcquisitionMultipleThread.cpp.

This program initializes and configures FLIR cameras in a hardware trigger setup. 
Image acquisition runs in parallel threads and saves data to binary file. 
The .tmp output file has to be converted to AVI with a different code TMPtoAVI.
Note that FrameRate and imageHeight and imageWidth are hardcoded from recording settings.

Install Spinnaker SDK before using this script.

MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com

Sourcecode: https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR
====================================================================================================
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <ctime>
#include <fstream>
#include <assert.h>
#include <time.h>
#include <cmath>

#ifndef _WIN32
#include <pthread.h>
#endif


using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;
//using std::cout;
//using Spinnaker::Exception;

const string triggerCam = "20323052";
const double exposureTime = 5000.0; // in microseconds
const string directory = "E:\\FlirCamera\\";
const int numBuffers = 200;

// Directory to save data to.
const string csDestinationDirectory = "";
const string csDestinationDirectory1 = "";

// Image Height and width
vector<size_t> imageHeight;
vector<size_t> imageWidth;

vector<string> filenames(3);

vector<ofstream> cameraFiles(3);
ofstream csvFile;
int cameraCnt;

// Pixel format 
vector<PixelFormatEnums> pixelFormat;

enum triggerType
{
	SOFTWARE,
	HARDWARE
};
// standard initialization
triggerType chosenTrigger = SOFTWARE;

/*
=================
These functions getCurrentDateTime and removeSpaces get the system time and transform it to readable format. The output string is used as timestamp for new filenames.
=================
*/
string removeSpaces(string word) {
	string newWord;
	for (int i = 0; i < word.length(); i++) {
		if (word[i] != ' ') {
			newWord += word[i];
		}
	}

	return newWord;
}

string getCurrentDateTime()
{
	stringstream currentDateTime;
	// current date/time based on current system
	time_t ttNow = time(0);
	tm * ptmNow;

    // year
	ptmNow = localtime(&ttNow);
	currentDateTime << 1900 + ptmNow->tm_year;
	//month
	if (ptmNow->tm_mon < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << 1 + ptmNow->tm_mon;
	else
		currentDateTime << (1 + ptmNow->tm_mon);
	//day
	if (ptmNow->tm_mday < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_mday << " ";
	else
		currentDateTime << ptmNow->tm_mday << " ";

	//hour
	if (ptmNow->tm_hour < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_hour;
	else
		currentDateTime << ptmNow->tm_hour;

	//min
	if (ptmNow->tm_min < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_min;
	else
		currentDateTime << ptmNow->tm_min;

	//sec
	if (ptmNow->tm_sec < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_sec;
	else
		currentDateTime << ptmNow->tm_sec;
	string test = currentDateTime.str();

	//str.erase(remove_if(str.begin(), str.end(), isspace), str.end());
	test = removeSpaces(test);
	cout << test << endl;
	return test;
}

/*
=================
The function CreateFiles works within a for loop and creates .tmp binary files for each camera, as well as a single csv logging sheet csvFile. The files are saved in *directory* as initialised above.
=================
*/
int CreateFiles(string serialNumber, int i)
{
    int result = 0;

    cout << endl << "*** CREATING FILES ***" << endl << endl;

    stringstream sstream;
	stringstream sstream1;
	string tmpFilename;
	string tmpFilename1;

    sstream << csDestinationDirectory << directory + getCurrentDateTime() + "_" + serialNumber << "_file" << i << ".tmp"; 
    sstream >> tmpFilename;
    filenames.push_back(tmpFilename);

    cout << "File " << tmpFilename << " initialized" << endl;

    // Create temporary files
    cameraFiles[i].open(tmpFilename.c_str(), ios_base::out | ios_base::binary);

    if (i == 0)
    {
		sstream1 << csDestinationDirectory << directory + getCurrentDateTime() + "_" + serialNumber << "_file_" << i << ".csv";
        sstream1 >> tmpFilename1;
    	filenames.push_back(tmpFilename1);

    	cout << "CSV file: " << tmpFilename1 << " initialized" << endl << endl;

    	// Create temporary files
    	csvFile.open(tmpFilename1);
    	csvFile << "Frame ID" << "," << "Timestamp" << "," <<" Serial number"  << endl;    		
    }
    return result;
}

/*
=================
The function ConfigureTrigger turns off trigger mode and then configures the trigger source for each camera. Once the trigger source has been selected, trigger mode is then enabled, which has the camera capture only a single image upon the execution of the chosen trigger. Note that chosenTrigger SOFTWARE/HARDWARE is chosen from outside the function. See resources here: https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/
=================
*/
int ConfigureTrigger(INodeMap & nodeMap)
{
	int result = 0;

	cout << endl << "*** CONFIGURING TRIGGER ***" << endl << endl;

	if (chosenTrigger == SOFTWARE)
	{
		cout << "Setting Software trigger:" << endl;
	}
	else if (chosenTrigger == HARDWARE)
	{
		cout << "Setting Hardware trigger:" << endl;
	}

	try
	{
		// Ensure trigger mode off
		CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
		if (!IsAvailable(ptrTriggerMode) || !IsReadable(ptrTriggerMode))
		{
			cout << "Unable to disable trigger mode (node retrieval). Aborting..." << endl;
			return -1;
		}

		CEnumEntryPtr ptrTriggerModeOff = ptrTriggerMode->GetEntryByName("Off");
		if (!IsAvailable(ptrTriggerModeOff) || !IsReadable(ptrTriggerModeOff))
		{
			cout << "Unable to disable trigger mode (enum entry retrieval). Aborting..." << endl;
			return -1;
		}

		ptrTriggerMode->SetIntValue(ptrTriggerModeOff->GetValue());

		cout << "1. Trigger mode disabled" << endl;

		// Select trigger source
		CEnumerationPtr ptrTriggerSource = nodeMap.GetNode("TriggerSource");
		if (!IsAvailable(ptrTriggerSource) || !IsWritable(ptrTriggerSource))
		{
			cout << "Unable to set trigger mode (node retrieval). Aborting..." << endl;
			return -1;
		}

		if (chosenTrigger == SOFTWARE)
		{
			// Set trigger mode to software
			CEnumEntryPtr ptrTriggerSourceSoftware = ptrTriggerSource->GetEntryByName("Software");
			if (!IsAvailable(ptrTriggerSourceSoftware) || !IsReadable(ptrTriggerSourceSoftware))
			{
				cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
				return -1;
			}

			ptrTriggerSource->SetIntValue(ptrTriggerSourceSoftware->GetValue());

			cout << "2. Trigger source set to software" << endl;
		}
		else if (chosenTrigger == HARDWARE)
		{
			// Set trigger mode to hardware ('Line3')
			CEnumEntryPtr ptrTriggerSourceHardware = ptrTriggerSource->GetEntryByName("Line3");
			if (!IsAvailable(ptrTriggerSourceHardware) || !IsReadable(ptrTriggerSourceHardware))
			{
				cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
				return -1;
			}

			ptrTriggerSource->SetIntValue(ptrTriggerSourceHardware->GetValue());

			cout << "2. Trigger source set to hardware" << endl;

			//Turn TriggerMode to ON for Hardware Trigger
			CEnumEntryPtr ptrTriggerModeOn = ptrTriggerMode->GetEntryByName("On");
			if (!IsAvailable(ptrTriggerModeOn) || !IsReadable(ptrTriggerModeOn))
			{
				cout << "Unable to enable trigger mode (enum entry retrieval). Aborting..." << endl;
				return -1;
			}

			ptrTriggerMode->SetIntValue(ptrTriggerModeOn->GetValue());

			cout << "3. Trigger mode activated" << endl;
		}
	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}
	return result;
}

/*
=================
The function ConfigureExposure sets the Exposure setting for the cameras. The Exposure time will affect the overall brightness of the image, but also the framerate. Note that *exposureTime* is set from outside the function. To reach a Frame Rate of 200fps each frame canot take longer than 1/200 = 5ms, thus restricting the exposure time to 5000 microseconds.
=================
*/
int ConfigureExposure(INodeMap& nodeMap)
{
	int result = 0;

	cout << endl << "*** CONFIGURING EXPOSURE ***" << endl << endl;

	try
	{
		// Turn off automatic exposure mode
		CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
		if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
		{
			cout << "Unable to disable automatic exposure (node retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Off");
		if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
		{
			cout << "Unable to disable automatic exposure (enum entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());


		// Set exposure time manually; exposure time recorded in microseconds
		CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
		if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
		{
			cout << "Unable to set exposure time. Aborting..." << endl << endl;
			return -1;
		}

		// Ensure desired exposure time does not exceed the maximum
		const double exposureTimeMax = ptrExposureTime->GetMax();
		double exposureTimeToSet = exposureTime; // Exposure time will limit FPS !!

		if (exposureTimeToSet > exposureTimeMax)
		{
			exposureTimeToSet = exposureTimeMax;
		}

		ptrExposureTime->SetValue(exposureTimeToSet);

		cout << std::fixed << "Exposure time set to " << exposureTimeToSet << " microseconds"  << endl;

		//checking the frame rate
        cout << endl << "*** CONFIGURING FRAMERATE ***" << endl << endl;
		CFloatPtr ptrAcquisitionFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
		if (!IsAvailable(ptrAcquisitionFrameRate) || !IsReadable(ptrAcquisitionFrameRate))
		{
			cout << "Unable to get node AcquisitionFrameRate. Aborting..." << endl << endl;
			return -1;
		}
		double testAcqFrameRate = ptrAcquisitionFrameRate->GetValue();
		cout << "Acquisition Frame Rate is  : " << testAcqFrameRate << endl;
		cout << "Maximum Acquisition Frame Rate is  : " << ptrAcquisitionFrameRate->GetMax() << endl;

		//checking the resulting FrameRate on the system after setting th exposure time:
		CFloatPtr ptrResultingAcquisitionFrameRate = nodeMap.GetNode("AcquisitionResultingFrameRate");
		if (!IsAvailable(ptrResultingAcquisitionFrameRate) || !IsReadable(ptrResultingAcquisitionFrameRate))
		{
			cout << "Unable to get node ResultingAcquisitionFrameRate. Aborting..." << endl << endl;
			return -1;
		}
		double testResultingAcqFrameRate = ptrResultingAcquisitionFrameRate->GetValue();
		cout << "Resulting acquisition Frame Rate is  : " << std::fixed << round(testResultingAcqFrameRate) << endl;
	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}
	return result;
}

/*
=================
The function ConfigureStrobe sets the triggering mode between cameras by selecting the strobe signal from one camera as the trigger input for the other cameras. The Output line and line mode settings are affected by the trigger settings above. See resources here: https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/
=================
*/
int ConfigureStrobe(CameraPtr pCam, INodeMap& nodeMap)
{
	int result = 0;
	string serialNumber = "";


	cout << endl << "*** CONFIGURING STROBE ***" << endl << endl;

	try
	{
		//Line Selector to an Output Line
		Spinnaker::GenApi::CEnumerationPtr ptrLineSelector = nodeMap.GetNode("LineSelector");
		if (!IsAvailable(ptrLineSelector) || !IsWritable(ptrLineSelector))
		{
			cout << "Unable to set Lineselector  (node retrieval). Aborting..." << endl << endl;
			return -1;
		}
		Spinnaker::GenApi::CEnumEntryPtr ptrLine2 = ptrLineSelector->GetEntryByName("Line2");
		if (!IsAvailable(ptrLine2) || !IsReadable(ptrLine2))
		{
			cout << "Unable to get Line2  (node retrieval). Aborting..." << endl << endl;
			return -1;
		}
		ptrLineSelector->SetIntValue(ptrLine2->GetValue());

		cout << "Selected LineSelector is Line:  " << ptrLineSelector->GetCurrentEntry()->GetSymbolic() << endl;

		// Line Mode
		CEnumerationPtr ptrLineMode = nodeMap.GetNode("LineMode");
		if (!IsAvailable(ptrLineMode) || !IsWritable(ptrLineMode))
		{
			cout << "Unable to get Line Mode(node retrieval). Aborting..." << endl << endl;
			return -1;
		}
		CEnumEntryPtr ptrOutput = ptrLineMode->GetEntryByName("Output");
		if (!IsAvailable(ptrOutput) || !IsReadable(ptrOutput))
		{
			cout << "Unable to get Output  (node retrieval). Aborting..." << endl << endl;
			return -1;
		}
		ptrLineMode->SetIntValue(ptrOutput->GetValue());
		cout << "Selected LineMode is:  " << ptrLineMode->GetCurrentEntry()->GetSymbolic() << endl;

		//  LineSource 
		CEnumerationPtr ptrLineSource = nodeMap.GetNode("LineSource");
		if (!IsAvailable(ptrLineSource) || !IsWritable(ptrLineSource))
		{
			cout << "Unable to get Line Source (node retrieval). Aborting..." << endl << endl;
			return -1;
		}
		CEnumEntryPtr ptrEntryLineSource = ptrLineSource->GetEntryByName("ExposureActive");
		if (!IsAvailable(ptrEntryLineSource) || !IsReadable(ptrEntryLineSource))
		{
			cout << "Unable to get Output  (node retrieval). Aborting..." << endl << endl;
			return -1;
		}
		ptrLineSource->SetIntValue(ptrEntryLineSource->GetValue());
		cout << "Selected LineSource  is:  " << ptrLineSource->GetCurrentEntry()->GetSymbolic() << endl;


		CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

		if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
		{
			serialNumber = ptrStringSerial->GetValue();
		}
	}
	catch (Spinnaker::Exception& e)
	{
		// Retrieve device serial number for filename
		cout << "[" << serialNumber << "] "<< "Error: " << e.what() << endl;
        result = -1;
	}
	return result;
}

/*
=================
The function BufferHandlingSettings sets manual buffer handling mode to numBuffers set above.
=================
*/
int BufferHandlingSettings(CameraPtr pCam)
{
	int result = 0;
    cout << endl << "*** CONFIGURING BUFFER ***" << endl << endl;
	// Retrieve Stream Parameters device nodemap
	Spinnaker::GenApi::INodeMap& sNodeMap = pCam->GetTLStreamNodeMap();

	// Retrieve Buffer Handling Mode Information
	CEnumerationPtr ptrHandlingMode = sNodeMap.GetNode("StreamBufferHandlingMode");
	if (!IsAvailable(ptrHandlingMode) || !IsWritable(ptrHandlingMode))
	{
		cout << "Unable to set Buffer Handling mode (node retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrHandlingModeEntry = ptrHandlingMode->GetCurrentEntry();
	if (!IsAvailable(ptrHandlingModeEntry) || !IsReadable(ptrHandlingModeEntry))
	{
		cout << "Unable to set Buffer Handling mode (Entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	// Set stream buffer Count Mode to manual
	CEnumerationPtr ptrStreamBufferCountMode = sNodeMap.GetNode("StreamBufferCountMode");
	if (!IsAvailable(ptrStreamBufferCountMode) || !IsWritable(ptrStreamBufferCountMode))
	{
		cout << "Unable to set Buffer Count Mode (node retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrStreamBufferCountModeManual = ptrStreamBufferCountMode->GetEntryByName("Manual");
	if (!IsAvailable(ptrStreamBufferCountModeManual) || !IsReadable(ptrStreamBufferCountModeManual))
	{
		cout << "Unable to set Buffer Count Mode entry (Entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	ptrStreamBufferCountMode->SetIntValue(ptrStreamBufferCountModeManual->GetValue());

	
	// Retrieve and modify Stream Buffer Count
	CIntegerPtr ptrBufferCount = sNodeMap.GetNode("StreamBufferCountManual");
	if (!IsAvailable(ptrBufferCount) || !IsWritable(ptrBufferCount))
	{
		cout << "Unable to set Buffer Count (Integer node retrieval). Aborting..." << endl << endl;
		return -1;
	}

	// Display Buffer Info
    cout << "Stream Buffer Count Mode set to manual" << endl;
	cout << "Default Buffer Count: " << ptrBufferCount->GetValue() << endl;
	cout << "Maximum Buffer Count: " << ptrBufferCount->GetMax() << endl;
	if (ptrBufferCount->GetMax() < numBuffers)
	{
		ptrBufferCount->SetValue(ptrBufferCount->GetMax());
	}
	else
	{
		ptrBufferCount->SetValue(numBuffers);
	}

	cout << "Manual Buffer Count: " << ptrBufferCount->GetValue() << endl;
	ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("OldestFirst");
	ptrHandlingMode->SetIntValue(ptrHandlingModeEntry->GetValue());
	cout << "Buffer Handling Mode has been set to " << ptrHandlingModeEntry->GetDisplayName() << endl;

	return result;
}

/*
=================
The function AcquireImages runs in parallel threads and grabs images from each camera and saves them in the corresponding binary file. Each image also records the image status to the logging csvFile.
=================
*/
#if defined(_WIN32)

DWORD WINAPI AcquireImages(LPVOID lpParam)
{
	CameraPtr pCam = *((CameraPtr*)lpParam);
#else
void* AcquireImages(void* arg)
{
	CameraPtr pCam = *((CameraPtr*)arg);
#endif

	try
	{
		// Retrieve TL device nodemap
		INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

		// Retrieve device serial number for filename
		CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

		string serialNumber = "";

		if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
		{
			serialNumber = ptrStringSerial->GetValue();
		}

		cout << "[" << serialNumber << "] " << "IMAGE ACQUISITION THREAD STARTING" << endl;

		// Initialize camera
		pCam->Init();
		INodeMap& nodeMap = pCam->GetNodeMap();

		// Begin acquiring images
		pCam->BeginAcquisition();

        // clear buffer before starting recording
        try
        {
            for (unsigned int imagesInBuffer = 0; imagesInBuffer < numBuffers; imagesInBuffer++)
            {
                ImagePtr pResultImage = pCam->GetNextImage(100);
                pResultImage->Release();
            }
        }
        catch (Exception &e) 
        {
        }

		cout << "[" << serialNumber << "] " << "Started recording..." << endl;

		// Retrieve, convert, and save images for each camera
		CStringPtr ptrDeviceUserId = nodeMap.GetNode("DeviceUserID");
		if (!IsAvailable(ptrDeviceUserId) || !IsWritable(ptrDeviceUserId))
		{
			cout << "Unable to get node ptrDeviceUserId. Aborting..." << endl << endl;
			return -1;
		}
		
		while (GetAsyncKeyState(VK_ESCAPE) == 0)
		{
			try
			{
				// Retrieve next received image and ensure image completion
				ImagePtr pResultImage = pCam->GetNextImage(1000);

				if (pResultImage->IsIncomplete())
				{
					cout << "[" << serialNumber << "] " << "Image incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl << endl;
				}
				else
				{
					//Acquire the image buffer to write to a file
					char *imageData = static_cast<char*>(pResultImage->GetData());
					string deviceUser_ID = ptrDeviceUserId->GetValue();

					cameraCnt = atoi(deviceUser_ID.c_str());
					// cameraCnt = number of the File to be writteninto

					// Do the writing
					cameraFiles[cameraCnt].write(imageData, pResultImage->GetImageSize());
					csvFile<<pResultImage->GetFrameID()<< "," << pResultImage->GetTimeStamp() <<","<< serialNumber<< endl;

					// Check if the writing is successful
					if (!cameraFiles[cameraCnt].good())
					{
						cout << "Error writing to file for camera " << cameraCnt << " !" << endl;
						return -1;
					}
                }
				// Release image
				pResultImage->Release();
			}
			catch (Spinnaker::Exception& e)
			{
				cout << "[" << serialNumber << "] " << "Error: " << e.what() << endl;
                return -1;
			}
		}

		// End acquisition
		pCam->EndAcquisition();
		cameraFiles[cameraCnt].close();
        csvFile.close();

		// Deinitialize camera
		pCam->DeInit();

#if defined(_WIN32)
		return 1;
#else
		return (void*)1;
#endif
	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
#if defined(_WIN32)
		return 0;
#else
		return (void*)0;
#endif
	}
}

/*
=================
The function InitializeMultipleCameras bundles the initialization process for all cameras on the system. It is called in RecordMultipleCameraThreads and starts a loop to set Buffer, Strobe, Exposure and Trigger, as well as to create binary files for each camera.
=================
*/
int InitializeMultipleCameras(CameraList camList, CameraPtr* pCamList)
{
    int result = 0;
    unsigned int camListSize = 0;

    try
    {
        // Retrieve camera list size
		camListSize = camList.GetSize();

        string serialNumber = "";

		imageHeight.resize(camListSize);
		imageWidth.resize(camListSize);
		
        for (unsigned int i = 0; i < camListSize; i++)
		{
			// Select camera
			pCamList[i] = camList.GetByIndex(i); // GET BY SERIAL??
			pCamList[i]->Init();
			INodeMap& nodeMap = pCamList[i]->GetNodeMap();

			CStringPtr ptrStringSerial = pCamList[i]->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

			if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
			{
				serialNumber = ptrStringSerial->GetValue();
			}

			CStringPtr ptrDeviceUserId = nodeMap.GetNode("DeviceUserID");
			if (!IsAvailable(ptrDeviceUserId) || !IsWritable(ptrDeviceUserId))
			{
				cout << "Unable to get node ptrDeviceUserId. Aborting..." << endl << endl;
				return -1;
			}
			
            cout << endl << endl << "[" << serialNumber << "] " << "*** Camera Initialization ***" << endl;
			
			if (serialNumber == triggerCam)  // if MASTER "20323052"
			{
				ConfigureStrobe(pCamList[i], nodeMap); 
				chosenTrigger = SOFTWARE;  // master camera will work in free running
				ConfigureTrigger(nodeMap);
			}
			else
			{
				chosenTrigger = HARDWARE; // triggered cameras
				ConfigureTrigger(nodeMap);
			}

			BufferHandlingSettings(pCamList[i]);
			ConfigureStrobe(pCamList[i], nodeMap);
			ConfigureExposure(nodeMap);

            // create binary files for each camera
            CreateFiles(serialNumber, i);

			pCamList[i]->DeInit();
		}
    }
    catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}
    return result;
}

/*
=================
The function RecordMultipleCameraThreads acts as the body of the program. It calls the InitializeMultipleCameras functino to configure all cameras, then creates parallel threads and starts AcquireImages on each thred. With the one triggers all setting configured above, threads are waiting for synchronized trigger as a central clock. 
=================
*/
int RecordMultipleCameraThreads(CameraList camList)
{
	int result = 0;
	unsigned int camListSize = 0;

	try
	{
		// Retrieve camera list size
		camListSize = camList.GetSize();

		// Create an array of handles
		CameraPtr* pCamList = new CameraPtr[camListSize];

        // move init here to use same CameraPtr
        InitializeMultipleCameras(camList, pCamList);

        // START RECORDING
        cout << endl << "*** START RECORDING ***" << endl << endl;

#if defined(_WIN32)
		HANDLE* grabThreads = new HANDLE[camListSize];
#else
		pthread_t* grabThreads = new pthread_t[camListSize];
#endif

		for (unsigned int i = 0; i < camListSize; i++) 
		{
			// Start grab thread
#if defined(_WIN32)
            
			grabThreads[i] = CreateThread(nullptr, 0, AcquireImages, &pCamList[i], 0, nullptr); // USING ACQUIREIMAGES FUNCTION
			assert(grabThreads[i] != nullptr);
#else
			int err = pthread_create(&(grabThreads[i]), nullptr, &AcquireImages, &pCamList[i]);
			assert(err == 0);
            
#endif
		}

#if defined(_WIN32)

		// Wait for all threads to finish
		WaitForMultipleObjects(
			camListSize, // number of threads to wait for
			grabThreads, // handles for threads to wait for
			TRUE,        // wait for all of the threads
			INFINITE     // wait forever
		);

		// Check thread return code for each camera
		for (unsigned int i = 0; i < camListSize; i++) 
		{
			DWORD exitcode;

			BOOL rc = GetExitCodeThread(grabThreads[i], &exitcode);
			if (!rc)
			{
				cout << "Handle error from GetExitCodeThread() returned for camera at index " << i << endl;
				result = -1;
			}
			else if (!exitcode)
			{
				cout << "Grab thread for camera at index " << i
					<< " exited with errors."
					"Please check onscreen print outs for error details"
					<< endl;
				result = -1;
			}
		}

#else
		for (unsigned int i = 0; i < camListSize; i++)
		{
			// Wait for all threads to finish
			void* exitcode;
			int rc = pthread_join(grabThreads[i], &exitcode);
			if (rc != 0)
			{
				cout << "Handle error from pthread_join returned for camera at index " << i << endl;
				result = -1;
			}
			else if ((int)(intptr_t)exitcode == 0) // check thread return code for each camera
			{
				cout << "Grab thread for camera at index " << i
					<< " exited with errors."
					"Please check onscreen print outs for error details" << endl;
				result = -1;
			}
		}
#endif

		// Clear CameraPtr array and close all handles
		for (unsigned int i = 0; i < camListSize; i++)
		{
			pCamList[i] = 0;
#if defined(_WIN32)
			CloseHandle(grabThreads[i]);
#endif
		}

		// Delete array pointer
		delete[] pCamList;

		// Delete array pointer
		delete[] grabThreads;

	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}


/*
=================
 This is the Entry point for the program. After testing writing priviledges it gets all camera instances and calls RecordMultipleCameraThreads.
=================
*/
int main(int /*argc*/, char** /*argv*/)
{
	// Since this application saves binary files in the current folder
	// we must ensure that we have permission to write to this folder.
    stringstream testfilestream;
	string testfilestring;

    testfilestream << directory + "test.txt"; 
    testfilestream >> testfilestring;

    const char * testfile = testfilestring.c_str();

	FILE* tempFile = fopen(testfile, "w+");
	if (tempFile == nullptr)
	{
		cout << "Failed to create file in current folder.  Please check permissions." << endl;
		cout << "Press Enter to exit..." << endl;
		getchar();
		return -1;
	}

	fclose(tempFile);
	remove(testfile);

	int result = 0;

	// Print application build information
    cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__  << endl;
    cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
    cout << "*************************************************************" << endl;

	// Retrieve singleton reference to system object
	SystemPtr system = System::GetInstance();

	// Print out current library version
	const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
	cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor<< "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl;

	// Retrieve list of cameras from the system
	CameraList camList = system->GetCameras();

	unsigned int numCameras = camList.GetSize();

	cout << "Number of cameras detected: " << numCameras << endl;


	// Finish if there are no cameras
	if (numCameras == 0)
	{
		// Clear camera list before releasing system
		camList.Clear();

		// Release system
		system->ReleaseInstance();

		cout << "No cameras connected!" << endl;
		cout << "Done! Press Enter to exit..." << endl;
		getchar();

		return -1;
	}


	// Run all cameras
	result = RecordMultipleCameraThreads(camList);

	// Clear camera list before releasing system
	camList.Clear();

	// Release system
	system->ReleaseInstance();

	cout << endl << "Done! Press Enter to exit..." << endl << endl;

    // Print application build information
    cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__  << endl;
    cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
    cout << "*************************************************************" << endl;
	getchar();

	return result;
}
