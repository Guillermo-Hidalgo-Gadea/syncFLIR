/*
====================================================================================================
This script was developed with close assistance by the FLIR Systems Support Team, and is adapted
from examples in FLIR Systems/Spinnaker/src. Copyright from FLIR Integrated Imaging Solutions, Inc. applies.
This program initializes and configures FLIR cameras in a hardware trigger setup in wich the primary camera
triggers all other secondary cameras. Image acquisition runs in parallel threads, locked during grabbing
and writing, and saves data to binary files to save time in the between frames intervals.
The .tmp output file has to be converted to AVI with a different code TMPtoAVI.
Note that exposureTime and triggerCam serial number are hardcoded and need to be adapted before compiling
the executable file. The binary files get very large, make sure to run the executable from the directory
in which the recordings should be saved. Install Spinnaker SDK before using this script.
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
#include <chrono>
#include <algorithm>
#include <string>
#ifndef _WIN32
#include <pthread.h>

#endif

using namespace std::chrono;
using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// INITIALIZE CONFIG PARAMETERS FOR RECORDING >> exported to config file
std::string path;
std::string triggerCam = "20323052"; // serial number of primary camera
double exposureTime = 5000.0; // exposure time in microseconds (i.e., 1/ max FPS)
double FPS = 100.0; // frames per second in Hz
int color = 0; // 1=color, else Mono 
double compression = 1.0; // compression factor
// TODO: use decimation or binning instead of size compression (http://softwareservices.flir.com/BFS-U3-89S6/latest/Model/public/ImageFormatControl.html)
int numBuffers = 200; // depending on RAM

// Image Height and width
vector<size_t> imageHeight;
vector<size_t> imageWidth;

// placeholder for names of file and camera IDs
vector<ofstream> cameraFiles;
ofstream csvFile;
int cameraCnt;
string serialNumber;

// mutex lock for parallel threads
HANDLE ghMutex;

// Camera trigger type for primary and secondary cameras
enum triggerType
{
	SOFTWARE, // Primary camera
	HARDWARE // Secondary camera
};

triggerType chosenTrigger = SOFTWARE; // standard initialization


/*
================
This function reads the config file to set camera parameters
================
*/
int readconfig()
{
	int result = 0;

	std::ifstream cFile("myConfig.txt");
	if (cFile.is_open())
	{
		
		std::string line;
		while (getline(cFile, line))
		{
			line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
			if (line[0] == '#' || line.empty()) continue;

			auto delimiterPos = line.find("=");
			auto name = line.substr(0, delimiterPos);
			auto value = line.substr(delimiterPos + 1);

			//Custom coding
			if (name == "triggerCam") triggerCam = value;
			else if (name == "FPS") FPS = std::stod(value);
			else if (name == "color") color = std::stoi(value);
			else if (name == "compression") compression = std::stod(value);
			else if (name == "exposureTime") exposureTime = std::stod(value);
			else if (name == "numBuffers") numBuffers = std::stod(value);
			else if (name == "path") path = value;
		}
	}
	else
	{
		std::cerr << "Couldn't open config file for reading.\n";
	}

	cout << "Parameter Settings from config file:";
	cout << "\ntriggerCam=" << triggerCam;
	std::cout << "\nFPS=" << FPS;
	std::cout << "\ncolor=" << color;
	std::cout << "\ncompression=" << compression;
	std::cout << "\nexposureTime=" << exposureTime;
	std::cout << "\nnumBuffers=" << numBuffers;
	std::cout << "\nPath=" << path<< endl << endl;

	return result, triggerCam, exposureTime, path, FPS, color, compression, numBuffers;
}

/*
=================
These functions getCurrentDateTime and removeSpaces get the system time and transform it to readable format. The output string is used as timestamp for new filenames. The function getTimeStamp prints system writing time in nanoseconds to the csv logfile.
=================
*/
string removeSpaces(string word)
{
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
	// current date/time based on system
	time_t ttNow = time(0);
	tm* ptmNow;
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
	//spacer
	currentDateTime << "_";
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

	test = removeSpaces(test);
	cout << test << endl;
	return test;
}

auto getTimeStamp()
{
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();

	typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<8>>::type> Days;

	Days days = std::chrono::duration_cast<Days>(duration);
	duration -= days;

	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

	auto timestamp = nanoseconds.count();
	return timestamp;
}

/*
=================
The function CreateFiles works within a for loop and creates .tmp binary files for each camera, as well as a single .csv logging sheet csvFile. The files are saved in working irectory.
=================
*/
int CreateFiles(string serialNumber, int cameraCnt)
{
	int result = 0;
	cout << endl << "*** CREATING FILES ***" << endl << endl;

	stringstream sstream_tmpFilename;
	stringstream sstream_csvFile;
	string tmpFilename;
	string csvFilename;
	const string csDestinationDirectory = path;

	sstream_tmpFilename << csDestinationDirectory << getCurrentDateTime() + "_" + serialNumber << "_file" << cameraCnt << ".tmp";
	sstream_tmpFilename >> tmpFilename;

	cout << "File " << tmpFilename << " initialized" << endl;

	// Create temporary file from serialnum assigned to cameraCnt
	ofstream filename;
	cameraFiles.push_back(std::move(filename));
	cameraFiles[cameraCnt].open(tmpFilename.c_str(), ios_base::out | ios_base::binary);

	// Create .csv logfile only once for all cameras during first loop
	if (cameraCnt == 0)
	{
		sstream_csvFile << csDestinationDirectory << getCurrentDateTime() << ".csv";
		sstream_csvFile >> csvFilename;

		cout << "CSV file: " << csvFilename << " initialized" << endl << endl;

		csvFile.open(csvFilename);
		csvFile << "FrameID" << "," << "Timestamp" << "," << "SerialNumber" << "," << "FileNumber" << "," << "SystemTimeInNanoseconds" << endl;
	}
	return result;
}

/*
=================
The function ImageSettings configures the image compression i.e., the actual image width and height.
=================
*/
int ImageSettings(INodeMap& nodeMap)
{
	int result = 0;
	cout << endl << "*** CONFIGURING IMAGE SETTINGS ***" << endl << endl;

	try
	{
		// Set image width
		CIntegerPtr ptrWidth = nodeMap.GetNode("Width");
		if (IsAvailable(ptrWidth) && IsWritable(ptrWidth))
		{
			int widthToSet = ptrWidth->GetMax();
			widthToSet = widthToSet / compression;
			ptrWidth->SetValue(widthToSet);

			cout << "Width set to " << ptrWidth->GetValue() << "..." << endl;
		}
		else
		{
			cout << "Width not available..." << endl;
		}

		// Set maximum height
		CIntegerPtr ptrHeight = nodeMap.GetNode("Height");
		if (IsAvailable(ptrHeight) && IsWritable(ptrHeight))
		{
			int heightToSet = ptrHeight->GetMax();
			heightToSet = heightToSet / compression;
			ptrHeight->SetValue(heightToSet);

			cout << "Height set to " << ptrHeight->GetValue() << "..." << endl << endl;
		}
		else
		{
			cout << "Height not available..." << endl << endl;
		}


		// Set Pixel Format

		// Retrieve the enumeration node from the nodemap
		CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
		if (IsAvailable(ptrPixelFormat) && IsWritable(ptrPixelFormat))
		{
			if (color == 1)
			{
				// Retrieve the desired entry node from the enumeration node
				CEnumEntryPtr ptrPixelFormatBayerRG8 = ptrPixelFormat->GetEntryByName("BayerRG8");
				if (IsAvailable(ptrPixelFormatBayerRG8) && IsReadable(ptrPixelFormatBayerRG8))
				{
					// Set integer as new value for enumeration node
					ptrPixelFormat->SetIntValue(ptrPixelFormatBayerRG8->GetValue());
					cout << "Pixel format set to " << ptrPixelFormat->GetCurrentEntry()->GetSymbolic() << "..." << endl;

				}
				else
				{
					cout << "Pixel format BayerRG8 not available..." << endl;
				}
			}
			else
			{
				// Retrieve the desired entry node from the enumeration node
				CEnumEntryPtr ptrPixelFormatMono8 = ptrPixelFormat->GetEntryByName("Mono8");
				if (IsAvailable(ptrPixelFormatMono8) && IsReadable(ptrPixelFormatMono8))
				{
					// Set integer as new value for enumeration node
					ptrPixelFormat->SetIntValue(ptrPixelFormatMono8->GetValue());

					cout << "Pixel format set to " << ptrPixelFormat->GetCurrentEntry()->GetSymbolic() << "..." << endl;
				}
				else
				{
					cout << "Pixel format Mono8 not available..." << endl;
				}
			}
		}
		else
		{
			cout << "Pixel format not available..." << endl;
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
The function ConfigureTrigger turns off trigger mode and then configures the trigger source for each camera. Once the trigger source has been selected, trigger mode is then enabled, which has the camera capture only a single image upon the execution of the chosen trigger. Note that chosenTrigger SOFTWARE/HARDWARE is chosen from outside the function. See resources here: https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/
=================
*/
int ConfigureTrigger(INodeMap& nodeMap)
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

			// Trigger mode not yet activated, initialized with BeginAcquisition
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
		double exposureTimeToSet = exposureTime; // Exposure time will limit FPS by exposure = 1/FPS

		if (exposureTimeToSet > exposureTimeMax)
		{
			exposureTimeToSet = exposureTimeMax;
		}
		ptrExposureTime->SetValue(exposureTimeToSet);
		cout << std::fixed << "Exposure time set to " << exposureTimeToSet << " microseconds" << endl;

		//checking the frame rate
		cout << endl << "*** CONFIGURING FRAMERATE ***" << endl << endl;
		CFloatPtr ptrAcquisitionFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
		if (!IsAvailable(ptrAcquisitionFrameRate) || !IsReadable(ptrAcquisitionFrameRate))
		{
			cout << "Unable to get node AcquisitionFrameRate. Aborting..." << endl << endl;
			return -1;
		}
		ptrAcquisitionFrameRate->SetValue(ptrAcquisitionFrameRate->GetMax());
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
		double FPSToSet = FPS;

		// if FPS > resulting then set FPS to max available
		if (FPSToSet > testResultingAcqFrameRate)
		{
			FPSToSet = testResultingAcqFrameRate;
			cout << "Chosen FPS to high, setting to max" << endl;
		}

		// set chosen FPS
		ptrAcquisitionFrameRate->SetValue(FPSToSet);

		double NewFrameRate = ptrAcquisitionFrameRate->GetValue();
		cout << "New acquisition Frame Rate is  : " << NewFrameRate << endl;

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
DWORD WINAPI AcquireImages(LPVOID lpParam)
{
	// START function in UN-locked thread

	// Initialize camera
	CameraPtr pCam = *((CameraPtr*)lpParam);
	pCam->Init();

	// Clean Buffer acquiring idle images
	pCam->BeginAcquisition();
	try
	{
		for (unsigned int imagesInBuffer = 0; imagesInBuffer < numBuffers; imagesInBuffer++)
		{
			// first numBuffer images are descarted
			ImagePtr pResultImage = pCam->GetNextImage();
			char* imageData = static_cast<char*>(pResultImage->GetData());
			pResultImage->Release();
		}
	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		return 1;
	}

	pCam->EndAcquisition();

	// Start actual image acquisition
	pCam->BeginAcquisition();

	// Initialize empty parameters outside of locked case
	ImagePtr pResultImage;
	char* imageData;
	string deviceUser_ID;
	int firstFrame = 1;
	int stopwait = 0;

	// Retrieve and save images in while loop until manual ESC press
	while (GetAsyncKeyState(VK_ESCAPE) == 0)
	{
		// Start mutex_lock
		DWORD dwCount = 0, dwWaitResult;
		dwWaitResult = WaitForSingleObject(
			ghMutex,    // handle to mutex
			INFINITE);  // no time-out interval
		switch (dwWaitResult)
		{

			// The thread got ownership of the mutex
		case WAIT_OBJECT_0:

			try
			{
				// Identify specific camera in locked thread
				INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
				CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
				if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
				{
					serialNumber = ptrStringSerial->GetValue();
				}

				INodeMap& nodeMap = pCam->GetNodeMap();
				CStringPtr ptrDeviceUserId = pCam->GetNodeMap().GetNode("DeviceUserID");
				if (IsAvailable(ptrDeviceUserId) && IsReadable(ptrDeviceUserId))
				{
					deviceUser_ID = ptrDeviceUserId->GetValue();
					cameraCnt = atoi(deviceUser_ID.c_str());
				}

				// anounce start recordnig only for firstFrame
				if (firstFrame == 1)
				{
					cout << "Camera [" << serialNumber << "] " << "Started recording with ID [" << cameraCnt << " ]..." << endl;
				}
				firstFrame = 0; // turn off firstFrame status

				// Break recording loop is stopwait status has been activated
				if (stopwait == 1)
				{
					break;
				}

				// Retrieve image and ensure image completion
				try
				{
					pResultImage = pCam->GetNextImage(1000); // waiting time for NextImage in miliseconds
				}
				catch (Spinnaker::Exception& e)
				{
					cout << "Error: " << e.what() << endl;
					stopwait == 1; // stopwait activated after waiting 1000ms without trigger
					cout << "stopwait activated" << endl;
				}

				// Acquire the image buffer to write to a file
				imageData = static_cast<char*>(pResultImage->GetData());

				// Do the writing to assigned cameraFile
				cameraFiles[cameraCnt].write(imageData, pResultImage->GetImageSize());

				csvFile << pResultImage->GetFrameID() << "," << pResultImage->GetTimeStamp() << "," << serialNumber << "," << cameraCnt << endl;

				//TODO add system time to see if frame id 0 (secondary cams) corresponds to frame id 0 (primary cam)
				//CAUTION: slows down recording loop...
				//csvFile << pResultImage->GetFrameID() << "," << pResultImage->GetTimeStamp() << "," << serialNumber << "," << cameraCnt << "," << getTimeStamp() << endl; 

				// Check if the writing is successful
				if (!cameraFiles[cameraCnt].good())
				{
					cout << "Error writing to file for camera " << cameraCnt << " !" << endl;
					return -1;
				}

				// Release image
				pResultImage->Release();

			}
			catch (Spinnaker::Exception& e)
			{
				cout << "Error: " << e.what() << endl;
				return -1;
			}

			// normal break
			ReleaseMutex(ghMutex);
			break;

			// The thread got ownership of an abandoned mutex
		case WAIT_ABANDONED:
			cout << "wait abandoned" << endl;
		}
	}

	// End acquisition
	pCam->EndAcquisition();
	cameraFiles[cameraCnt].close();
	csvFile.close();

	// Deinitialize camera
	pCam->DeInit();

	return 1;
}

/*
=================
The function InitializeMultipleCameras bundles the initialization process for all cameras on the system. It is called in RecordMultipleCameraThreads and starts a loop to set Buffer, Strobe, Exposure and Trigger, as well as to create binary files for each camera.
=================
*/
int InitializeMultipleCameras(CameraList camList, CameraPtr* pCamList, unsigned int camListSize)
{
	int result = 0;

	try
	{
		for (unsigned int i = 0; i < camListSize; i++)
		{
			// Select camera in loop
			pCamList[i] = camList.GetByIndex(i); // TODO: try to get order USB Interface/primary vs secondary // get serial
			pCamList[i]->Init();

			INodeMap& nodeMap = pCamList[i]->GetNodeMap();
			CStringPtr ptrStringSerial = pCamList[i]->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

			if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
			{
				serialNumber = ptrStringSerial->GetValue();
			}

			// Set DeviceUserID to loop counter to assign camera order to cameraFiles in oarallel threads
			CStringPtr ptrDeviceUserId = nodeMap.GetNode("DeviceUserID");
			if (!IsAvailable(ptrDeviceUserId) || !IsWritable(ptrDeviceUserId))
			{
				cout << "Unable to get node ptrDeviceUserId. Aborting..." << endl << endl;
				return -1;
			}

			stringstream sstreamDeviceUserID;
			sstreamDeviceUserID << i;
			string DeviceUserID = sstreamDeviceUserID.str();
			ptrDeviceUserId->SetValue(DeviceUserID.c_str());
			cameraCnt = atoi(DeviceUserID.c_str());

			cout << endl << endl << "[" << serialNumber << "] " << "*** Camera Initialization ***" << " ID: [" << DeviceUserID << "]" << endl;

			if (serialNumber == triggerCam)  // primary camera defined in PARAMETERS
			{
				ConfigureStrobe(pCamList[i], nodeMap);
				chosenTrigger = SOFTWARE; // variable is changed globally
				ConfigureTrigger(nodeMap); // primary camera will work in free running
			}
			else // secondary cameras
			{
				chosenTrigger = HARDWARE; // variable is changed globally
				ConfigureTrigger(nodeMap); // secondary cameras are triggered by primary camera
			}

			// Set Buffer
			BufferHandlingSettings(pCamList[i]);

			// Set Strobe
			ConfigureStrobe(pCamList[i], nodeMap);

			// Set Exposure and Framerate
			ConfigureExposure(nodeMap);

			// Set Image Settings
			ImageSettings(nodeMap);

			// Create binary files for each camera and overall .csv logfile
			CreateFiles(serialNumber, cameraCnt);

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
The function RecordMultipleCameraThreads acts as the body of the program. It calls the InitializeMultipleCameras function to configure all cameras, then creates parallel threads and starts AcquireImages on each thred. With the setting configured above, threads are waiting for synchronized trigger as a central clock.
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

		// Initialize cameras in camList 
		InitializeMultipleCameras(camList, pCamList, camListSize);

		// START RECORDING
		cout << endl << "*** START RECORDING ***" << endl << endl;

		HANDLE* grabThreads = new HANDLE[camListSize];
		for (unsigned int i = 0; i < camListSize; i++)
		{
			// Start grab thread
			grabThreads[i] = CreateThread(nullptr, 0, AcquireImages, &pCamList[i], 0, nullptr); // call AcquireImages in parallel threads
			assert(grabThreads[i] != nullptr);
		}

		// Wait for all threads to finish
		WaitForMultipleObjects(
			camListSize, // number of threads to wait for
			grabThreads, // handles for threads to wait for
			TRUE,        // wait for all of the threads
			INFINITE     // wait forever
		);

		CloseHandle(ghMutex);

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
				cout << "Grab thread for camera at index " << i << " exited with errors." << endl;
				result = -1;
			}
		}

		// Clear CameraPtr array and close all handles
		for (unsigned int i = 0; i < camListSize; i++)
		{
			pCamList[i] = 0;
			CloseHandle(grabThreads[i]);
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
	// Print application build information
	cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl;
	cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
	cout << "*************************************************************" << endl;

	int result = 0;

	// Test permission to write to directory
	stringstream testfilestream;
	string testfilestring;
	testfilestream << "test.txt";
	testfilestream >> testfilestring;
	const char* testfile = testfilestring.c_str();

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

	// Read config file and set parameters
	readconfig();

	// Retrieve singleton reference to system object
	SystemPtr system = System::GetInstance();

	// Print out current library version
	const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
	cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl;

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

	// Create mutex to lock parallel threads
	ghMutex = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex

	if (ghMutex == NULL)
	{
		printf("CreateMutex error: %d\n", GetLastError());
		return 1;
	}

	// Run all cameras
	result = RecordMultipleCameraThreads(camList);

	// Clear camera list before releasing system
	camList.Clear();

	// Release system
	system->ReleaseInstance();

	cout << endl << "Recording ended" << endl << endl;
	cout << endl << "Done! Press Enter to exit..." << endl << endl;
	// TODO: print Working directory?

	// Print application build information
	cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl;
	cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
	cout << "*************************************************************" << endl;
	getchar();

	return result;
}
