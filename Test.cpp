//=============================================================================
// Copyright (c) 2001-2019 FLIR Systems, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================

/**
 *  @example AcquisitionMultipleThread.cpp
 *
 *  @brief AcquisitionMultipleThread.cpp shows how to capture images from multiple
 *  cameras simultaneously using threads. It relies on information provided in the
 *  Enumeration, Acquisition, and NodeMapInfo examples.
 *
 *  This example is similar to the Acquisition example, except that threads
 *  are used to allow for simultaneous acquisitions.
 */

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include<fstream>
#include<vector>
#include <ctime>
#include <fstream>
#include <assert.h>
#include <time.h>

#ifndef _WIN32
#include <pthread.h>
#endif
#define numBuffers 200

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

enum chunkDataType
{
	IMAGE,
	NODEMAP
};

const chunkDataType chosenChunkData = IMAGE;

// Directory to save data to.
const string csDestinationDirectory = "";
const string csDestinationDirectory1 = "";
// Image Height and width
vector<size_t> imageHeight;

vector<size_t> imageWidth;

vector<string> filename;

int k_numImages[6];
vector<uint16_t> frameIDPrevious(6);  // 6 Kameras

vector<int> frameIDDelta(6);
vector<ofstream> cameraFiles(6);
vector<ofstream> csvFile(6);
ofstream csvDatei;
int cameraCnt;
// Pixel format 
vector<PixelFormatEnums> pixelFormat;

// This function configures the camera to add chunk data to each image. It does
// this by enabling each type of chunk data before enabling chunk data mode.
// When chunk data is turned on, the data is made available in both the nodemap
// and each image.
int ConfigureChunkData(INodeMap& nodeMap)
{
	int result = 0;

	cout << endl << endl << "*** CONFIGURING CHUNK DATA ***" << endl << endl;

	try
	{
		//
		// Activate chunk mode
		//
		// *** NOTES ***
		// Once enabled, chunk data will be available at the end of the payload
		// of every image captured until it is disabled. Chunk data can also be
		// retrieved from the nodemap.
		//
		CBooleanPtr ptrChunkModeActive = nodeMap.GetNode("ChunkModeActive");

		if (!IsAvailable(ptrChunkModeActive) || !IsWritable(ptrChunkModeActive))
		{
			cout << "Unable to activate chunk mode. Aborting..." << endl << endl;
			return -1;
		}

		ptrChunkModeActive->SetValue(true);

		cout << "Chunk mode activated..." << endl;

		//
		// Enable all types of chunk data
		//
		// *** NOTES ***
		// Enabling chunk data requires working with nodes: "ChunkSelector"
		// is an enumeration selector node and "ChunkEnable" is a boolean. It
		// requires retrieving the selector node (which is of enumeration node
		// type), selecting the entry of the chunk data to be enabled, retrieving
		// the corresponding boolean, and setting it to true.
		//
		// In this example, all chunk data is enabled, so these steps are
		// performed in a loop. Once this is complete, chunk mode still needs to
		// be activated.
		//
		NodeList_t entries;

		// Retrieve the selector node
		CEnumerationPtr ptrChunkSelector = nodeMap.GetNode("ChunkSelector");

		if (!IsAvailable(ptrChunkSelector) || !IsReadable(ptrChunkSelector))
		{
			cout << "Unable to retrieve chunk selector. Aborting..." << endl << endl;
			return -1;
		}

		// Retrieve entries
		ptrChunkSelector->GetEntries(entries);

		cout << "Enabling entries..." << endl;

		for (size_t i = 0; i < entries.size(); i++)
		{
			// Select entry to be enabled
			CEnumEntryPtr ptrChunkSelectorEntry = entries.at(i);

			// Go to next node if problem occurs
			if (!IsAvailable(ptrChunkSelectorEntry) || !IsReadable(ptrChunkSelectorEntry))
			{
				continue;
			}

			ptrChunkSelector->SetIntValue(ptrChunkSelectorEntry->GetValue());

			cout << "\t" << ptrChunkSelectorEntry->GetSymbolic() << ": ";

			// Retrieve corresponding boolean
			CBooleanPtr ptrChunkEnable = nodeMap.GetNode("ChunkEnable");

			// Enable the boolean, thus enabling the corresponding chunk data
			if (!IsAvailable(ptrChunkEnable))
			{
				cout << "not available" << endl;
				result = -1;
			}
			else if (ptrChunkEnable->GetValue())
			{
				cout << "enabled" << endl;
			}
			else if (IsWritable(ptrChunkEnable))
			{
				ptrChunkEnable->SetValue(true);
				cout << "enabled" << endl;
			}
			else
			{
				cout << "not writable" << endl;
				result = -1;
			}
		}
	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}

// This function displays a select amount of chunk data from the image. Unlike
// accessing chunk data via the nodemap, there is no way to loop through all
// available data.
int DisplayChunkData(ImagePtr pImage)
{
	int result = 0;

	cout << "Printing chunk data from image..." << endl;

	try
	{
		//
		// Retrieve chunk data from image
		//
		// *** NOTES ***
		// When retrieving chunk data from an image, the data is stored in a
		// a ChunkData object and accessed with getter functions.
		//
		ChunkData chunkData = pImage->GetChunkData();

		//
		// Retrieve exposure time; exposure time recorded in microseconds
		//
		// *** NOTES ***
		// Floating point numbers are returned as a float64_t. This can safely
		// and easily be statically cast to a double.
		//

		// Retrieve frame ID
		//
		// *** NOTES ***
		// Integers are returned as an int64_t. As this is the typical integer
		// data type used in the Spinnaker SDK, there is no need to cast it.
		//
		int64_t frameID = chunkData.GetFrameID();
		cout << "\tFrame ID: " << frameID << endl;

		// Retrieve timestamp
		uint64_t timestamp = chunkData.GetTimestamp();
		cout << "\tTimestamp: " << timestamp << endl;

		//// Retrieve width; width recorded in pixels
		//int64_t width = chunkData.GetWidth();
		//cout << "\tWidth: " << width << endl;
	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}

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
	//string getCurrentDateTime(bool useLocalTime) {
	stringstream currentDateTime;
	// current date/time based on current system
	time_t ttNow = time(0);
	tm * ptmNow;

	/*if (useLocalTime)
		ptmNow = localtime(&ttNow);
	else
		ptmNow = gmtime(&ttNow);*/

	ptmNow = localtime(&ttNow);
	currentDateTime << 1900 + ptmNow->tm_year;

	//month
	if (ptmNow->tm_mon < 9)
		//Fill in the leading 0 if less than 10
		currentDateTime << "0" << 1 + ptmNow->tm_mon;
	else
		currentDateTime << (1 + ptmNow->tm_mon);

	//day
	if (ptmNow->tm_mday < 10)
		currentDateTime << "0" << ptmNow->tm_mday << " ";
	else
		currentDateTime << ptmNow->tm_mday << " ";

	//hour
	if (ptmNow->tm_hour < 10)
		currentDateTime << "0" << ptmNow->tm_hour;
	else
		currentDateTime << ptmNow->tm_hour;

	//min
	if (ptmNow->tm_min < 10)
		currentDateTime << "0" << ptmNow->tm_min;
	else
		currentDateTime << ptmNow->tm_min;

	//sec
	if (ptmNow->tm_sec < 10)
		currentDateTime << "0" << ptmNow->tm_sec;
	else
		currentDateTime << ptmNow->tm_sec;
	string test = currentDateTime.str();

	//str.erase(remove_if(str.begin(), str.end(), isspace), str.end());
	test = removeSpaces(test);
	cout << test << endl;
	return test;
}


// Create files to save each of the camera images
int CreateFiles(vector<ofstream> &cameraFiles, vector<string> &filenames, int i, string serialN)
{

	stringstream sstream;
	stringstream sstream1;
	string tmpFilename;
	string tmpFilename1;
	ofstream outputFile;

	//sstream << csDestinationDirectory << "C:\\temp\\" + getCurrentDateTime() +"_"+ serialN << "_file_" << i << ".tmp"; // timestamp_serial_file[i].tmp
	sstream << csDestinationDirectory << "E:\\FlirCamera\\" + getCurrentDateTime() + "_" + serialN << "_file_" << i << ".tmp"; // timestamp_serial_file[i].tmp
	sstream >> tmpFilename;
	filenames.push_back(tmpFilename);

	cout << "Creating " << tmpFilename << "..." << endl << endl;

	// Create temporary files
	cameraFiles[i].open(tmpFilename.c_str(), ios_base::out | ios_base::binary);

	if (!cameraFiles[i])
	{
		assert(false);
		return -1;
	}

	if (i == 0)
	{
		//sstream1 << csDestinationDirectory1 << "C:\\temp\\" + getCurrentDateTime() + "_" + serialN << "_file_" << i << ".csv"; // timestamp_serial_file[i].tmp
		sstream1 << csDestinationDirectory << "E:\\FlirCamera\\" + getCurrentDateTime() + "_" + serialN << "_file_" << i << ".csv"; // timestamp_serial_file[i].tmp
		sstream1 >> tmpFilename1;
		filenames.push_back(tmpFilename1);

		cout << "Creating csv:" << tmpFilename1 << "..." << endl << endl;


		// Create temporary files
		csvDatei.open(tmpFilename1);
		csvDatei << "Frame ID" << "," << "Timestamp" << "," <<" Serial number"  << endl;
		if (!csvDatei)
		{
			assert(false);
			return -1;
		}


		////sstream1 << csDestinationDirectory1 << "C:\\temp\\" + getCurrentDateTime() + "_" + serialN << "_file_" << i << ".csv"; // timestamp_serial_file[i].tmp
		//sstream1 << csDestinationDirectory << "E:\\FlirCamera\\" + getCurrentDateTime() + "_" + serialN << "_file_" << i << ".csv"; // timestamp_serial_file[i].tmp
		//sstream1 >> tmpFilename1;
		//filenames.push_back(tmpFilename1);

		//cout << "Creating csv:" << tmpFilename1 << "..." << endl << endl;


		//// Create temporary files
		//csvFile[i].open(tmpFilename1);
		//csvFile[i] << "Frame ID" << "," << "Timestamp" << endl;
		//if (!csvFile[i])
		//{
		//	assert(false);
		//	return -1;
		//}
	}
	return 0;
}

int RetrieveImagesFromFiles(vector<string>&filenames, int numCameras, string fileFormat = "bmp")
{
	int result = 0;

	try {
		// Loop through the saved files for each camera and retrieve the images
		for (unsigned int cameraCnt = 0; cameraCnt < numCameras; cameraCnt++)
		{
			string tempFilename = filenames.at(cameraCnt);

			int imageSize = imageHeight.at(cameraCnt) * imageWidth.at(cameraCnt);

			cout << "Opening " << tempFilename.c_str() << "..." << endl;

			ifstream rawFile(filenames.at(cameraCnt).c_str(), ios_base::in | ios_base::binary);

			if (!rawFile)
			{
				cout << "Error opening file: " << filenames.at(cameraCnt).c_str() << " Aborting..." << endl;

				return -1;
			}

			cout << "Splitting images" << endl;

			// Read image into buffer
			for (int imageCnt = 0; imageCnt < k_numImages[cameraCnt]; imageCnt++)
			{
				string imageFilename;

				stringstream sstream;

				char* imageBuffer = new char[imageSize];

				rawFile.read(imageBuffer, imageSize);


				// Import image into Image structure
				ImagePtr pImage = Image::Create(imageWidth.at(cameraCnt), imageHeight.at(cameraCnt), 0, 0, pixelFormat[cameraCnt], imageBuffer);

				// Create file location and file name
				sstream << csDestinationDirectory << "camera" << cameraCnt << "_"
					<< imageCnt << "." << fileFormat;

				sstream >> imageFilename;

				//  Save image to disk
				pImage->Save(imageFilename.c_str());

				// Delete the acquired buffer
				delete[] imageBuffer;

				// Check if reading is successful
				if (!rawFile.good())
				{
					cout << "Error reading from file for camera " << cameraCnt << " !" << endl;

					// Close the file
					rawFile.close();

					return -1;
				}

				cout << "Camera[" << cameraCnt << "]: Retrieved image " << imageCnt << endl;

			}

			// Close the file
			rawFile.close();

			cout << endl << endl;

		}

	}

	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		return -1;
	}
	return result;
}

enum triggerType
{
	SOFTWARE,
	HARDWARE
};

triggerType chosenTrigger = SOFTWARE;
// This function configures the camera to use a trigger. First, trigger mode is 
// set to off in order to select the trigger source. Once the trigger source
// has been selected, trigger mode is then enabled, which has the camera 
// capture only a single image upon the execution of the chosen trigger.
int ConfigureTrigger(INodeMap & nodeMap)
{
	int result = 0;

	cout << endl << endl << "*** CONFIGURING TRIGGER ***" << endl << endl;

	if (chosenTrigger == SOFTWARE)
	{
		cout << "Software trigger chosen..." << endl;
	}
	else if (chosenTrigger == HARDWARE)
	{
		cout << "Hardware trigger chosen..." << endl;
	}

	try
	{
		//
		// Ensure trigger mode off
		//
		// *** NOTES ***
		// The trigger must be disabled in order to configure whether the source
		// is software or hardware.
		//
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

		cout << "Trigger mode disabled..." << endl;

		//
		// Select trigger source
		//
		// *** NOTES ***
		// The trigger source must be set to hardware or software while trigger 
		// mode is off.
		//
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

			cout << "Trigger source set to software..." << endl;
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

			cout << "Trigger source set to hardware..." << endl;


			//Turn TriggerMode to ON for Hardware Trigger
			CEnumEntryPtr ptrTriggerModeOn = ptrTriggerMode->GetEntryByName("On");
			if (!IsAvailable(ptrTriggerModeOn) || !IsReadable(ptrTriggerModeOn))
			{
				cout << "Unable to enable trigger mode (enum entry retrieval). Aborting..." << endl;
				return -1;
			}

			ptrTriggerMode->SetIntValue(ptrTriggerModeOn->GetValue());

			// TODO: Blackfly and Flea3 GEV cameras need 1 second delay after trigger mode is turned on 

			cout << "Trigger mode turned back on..." << endl << endl;
		}

	}
	catch (Spinnaker::Exception &e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}



int ConfigureExposure(INodeMap& nodeMap)
{
	int result = 0;

	cout << endl << endl << "*** CONFIGURING EXPOSURE ***" << endl << endl;

	try
	{
		//
		// Turn off automatic exposure mode
		//
		// *** NOTES ***
		// Automatic exposure prevents the manual configuration of exposure
		// time and needs to be turned off.
		//
		// *** LATER ***
		// Exposure time can be set automatically or manually as needed. This
		// example turns automatic exposure off to set it manually and back
		// on in order to return the camera to its default state.
		//
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

		cout << "Automatic exposure disabled..." << endl;

		//
		// Set exposure time manually; exposure time recorded in microseconds
		//
		// *** NOTES ***
		// The node is checked for availability and writability prior to the
		// setting of the node. Further, it is ensured that the desired exposure
		// time does not exceed the maximum. Exposure time is counted in
		// microseconds. This information can be found out either by
		// retrieving the unit with the GetUnit() method or by checking SpinView.
		//
		CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
		if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
		{
			cout << "Unable to set exposure time. Aborting..." << endl << endl;
			return -1;
		}

		// Ensure desired exposure time does not exceed the maximum
		const double exposureTimeMax = ptrExposureTime->GetMax();
		double exposureTimeToSet = 5000.0;

		if (exposureTimeToSet > exposureTimeMax)
		{
			exposureTimeToSet = exposureTimeMax;
		}

		ptrExposureTime->SetValue(exposureTimeToSet);

		cout << std::fixed << "Exposure time set to " << exposureTimeToSet << " us..." << endl << endl;

		//checking the frame rate :
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
		cout << "Resulting acquisition Frame Rate is  : " << testResultingAcqFrameRate << endl;





	}
	catch (Spinnaker::Exception& e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}


int ConfigureStrobe(CameraPtr pCam, INodeMap& nodeMap)
{
	int result = 0;
	string serialNumber = "";

	cout << " *** Configuring Strobe *** " << endl << endl;

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

		cout << "[" << serialNumber << "] "
			<< "Error: " << e.what() << endl;
	}

	return result;

}


int BufferHandlingSettings(CameraPtr pCam)
{
	int result = 0;

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

	cout << "Stream Buffer Count Mode set to manual..." << endl;

	// Retrieve and modify Stream Buffer Count
	CIntegerPtr ptrBufferCount = sNodeMap.GetNode("StreamBufferCountManual");
	if (!IsAvailable(ptrBufferCount) || !IsWritable(ptrBufferCount))
	{
		cout << "Unable to set Buffer Count (Integer node retrieval). Aborting..." << endl << endl;
		return -1;
	}

	// Display Buffer Info
	cout << endl << "Default Buffer Handling Mode: " << ptrHandlingModeEntry->GetDisplayName() << endl;
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

	cout << "Buffer count now set to: " << ptrBufferCount->GetValue() << endl;
	ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("OldestFirst");
	ptrHandlingMode->SetIntValue(ptrHandlingModeEntry->GetValue());
	cout << endl
		<< endl
		<< "Buffer Handling Mode has been set to " << ptrHandlingModeEntry->GetDisplayName() << endl;

	cout << endl;

	return result;
}

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap& nodeMap, std::string camSerial)
{
	int result = 0;

	cout << "[" << camSerial << "] Printing device information ..." << endl << endl;

	FeatureList_t features;
	CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
	if (IsAvailable(category) && IsReadable(category))
	{
		category->GetFeatures(features);

		FeatureList_t::const_iterator it;
		for (it = features.begin(); it != features.end(); ++it)
		{
			CNodePtr pfeatureNode = *it;
			CValuePtr pValue = (CValuePtr)pfeatureNode;
			cout << "[" << camSerial << "] " << pfeatureNode->GetName() << " : "
				<< (IsReadable(pValue) ? pValue->ToString() : "Node not readable") << endl;
		}
	}
	else
	{
		cout << "[" << camSerial << "] "
			<< "Device control information not available." << endl;
	}

	cout << endl;

	return result;
}

#ifdef _DEBUG
// Disables heartbeat on GEV cameras so debugging does not incur timeout errors
int DisableHeartbeat(CameraPtr pCam, INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
	cout << "Checking device type to see if we need to disable the camera's heartbeat..." << endl << endl;
	//
	// Write to boolean node controlling the camera's heartbeat
	//
	// *** NOTES ***
	// This applies only to GEV cameras and only applies when in DEBUG mode.
	// GEV cameras have a heartbeat built in, but when debugging applications the
	// camera may time out due to its heartbeat. Disabling the heartbeat prevents
	// this timeout from occurring, enabling us to continue with any necessary debugging.
	// This procedure does not affect other types of cameras and will prematurely exit
	// if it determines the device in question is not a GEV camera.
	//
	// *** LATER ***
	// Since we only disable the heartbeat on GEV cameras during debug mode, it is better
	// to power cycle the camera after debugging. A power cycle will reset the camera
	// to its default settings.
	//

	CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
	if (!IsAvailable(ptrDeviceType) && !IsReadable(ptrDeviceType))
	{
		cout << "Error with reading the device's type. Aborting..." << endl << endl;
		return -1;
	}
	else
	{
		if (ptrDeviceType->GetIntValue() == DeviceType_GigEVision)
		{
			cout << "Working with a GigE camera. Attempting to disable heartbeat before continuing..." << endl << endl;
			CBooleanPtr ptrDeviceHeartbeat = nodeMap.GetNode("GevGVCPHeartbeatDisable");
			if (!IsAvailable(ptrDeviceHeartbeat) || !IsWritable(ptrDeviceHeartbeat))
			{
				cout << "Unable to disable heartbeat on camera. Continuing with execution as this may be non-fatal..."
					<< endl
					<< endl;
			}
			else
			{
				ptrDeviceHeartbeat->SetValue(true);
				cout << "WARNING: Heartbeat on GigE camera disabled for the rest of Debug Mode." << endl;
				cout << "         Power cycle camera when done debugging to re-enable the heartbeat..." << endl << endl;
			}
		}
		else
		{
			cout << "Camera does not use GigE interface. Resuming normal execution..." << endl << endl;
		}
	}
	return 0;
}
#endif

// This function acquires and saves 10 images from a camera.
#if defined(_WIN32)
//DWORD WINAPI AcquireImages(LPVOID lpParam, int i)
//{
//	CameraPtr pCam = *((CameraPtr*)lpParam);
//#else
DWORD WINAPI AcquireImages(LPVOID lpParam)
{
	CameraPtr pCam = *((CameraPtr*)lpParam);
#else
void* AcquireImages(void* arg)
{
	CameraPtr pCam = *((CameraPtr*)arg);
#endif

	int missedImageCnts = 0;



	try
	{
		// Retrieve TL device nodemap
		INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();


		// Retrieve device serial number for filename
		CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

		std::string serialNumber = "";

		if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
		{
			serialNumber = ptrStringSerial->GetValue();
		}

		cout << endl
			<< "[" << serialNumber << "] "
			<< "*** IMAGE ACQUISITION THREAD STARTING"
			<< " ***" << endl
			<< endl;
		//cameraCnt++;
		// Print device information
	//	PrintDeviceInfo(nodeMapTLDevice, serialNumber);


		// Initialize camera
		pCam->Init();
		INodeMap& nodeMap = pCam->GetNodeMap();
#pragma region old_code1
		/*INodeMap& nodeMap = pCam->GetNodeMap();
		if (serialNumber == "20323052")  // if MASTER   "17458277")//
		{
			ConfigureStrobe(pCam, nodeMap);
			chosenTrigger = SOFTWARE;  // master camera will work in free running
			ConfigureTrigger(nodeMap);
			cout << " Strobe & Trigger configured for MAster Camera" << endl;
		}
		else
		{
			chosenTrigger = SOFTWARE; // slave cameras
			ConfigureTrigger(nodeMap);
			cout << " Trigger configured for slave Camera" << endl;
		}

#ifdef _DEBUG
		cout << endl << endl << "*** DEBUG ***" << endl << endl;

		// If using a GEV camera and debugging, should disable heartbeat first to prevent further issues
		if (DisableHeartbeat(pCam, pCam->GetNodeMap(), pCam->GetTLDeviceNodeMap()) != 0)
		{
#if defined(_WIN32)
			return 0;
#else
			return (void*)0;
#endif
		}

		cout << endl << endl << "*** END OF DEBUG ***" << endl << endl;
#endif
		BufferHandlingSettings(pCam);
		ConfigureStrobe(pCam, nodeMap);
		ConfigureExposure(nodeMap);

		// Keep the track of the pixel Format
		//pixelFormat.at(cameraCnt) = PixelFormatEnums::PixelFormat_Mono8;
		//pixelFormat.at(0) = PixelFormatEnums::PixelFormat_Mono8;
		//pixelFormat[1] = PixelFormatEnums::PixelFormat_Mono8;
		// Set acquisition mode to continuous
		CEnumerationPtr ptrAcquisitionMode = pCam->GetNodeMap().GetNode("AcquisitionMode");
		if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
		{
			cout << "Unable to set acquisition mode to continuous (node retrieval; camera " << serialNumber
				<< "). Aborting..." << endl
				<< endl;
#if defined(_WIN32)
			return 0;
#else
			return (void*)0;
#endif
		}

		CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
		if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
		{
			cout << "Unable to set acquisition mode to continuous (entry 'continuous' retrieval " << serialNumber
				<< "). Aborting..." << endl
				<< endl;
#if defined(_WIN32)
			return 0;
#else
			return (void*)0;
#endif
		}

		int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

		ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

		cout << "[" << serialNumber << "] "
			<< "Acquisition mode set to continuous..." << endl;

			*/
#pragma endregion old_code1 

			// Begin acquiring images
		pCam->BeginAcquisition();

		cout << "[" << serialNumber << "] "
			<< "Started acquiring images..." << endl;

		//
		// Retrieve, convert, and save images for each camera
		//
		//const unsigned int k_numImages = 10;

		CStringPtr ptrDeviceUserId = nodeMap.GetNode("DeviceUserID");
		if (!IsAvailable(ptrDeviceUserId) || !IsWritable(ptrDeviceUserId))
		{
			cout << "Unable to get node ptrDeviceUserId. Aborting..." << endl << endl;
			return -1;
		}

		//string deviceUser_ID = ptrDeviceUserId->GetValue();
		//cout << "Device User ID is: " + deviceUser_ID << endl;
		////cout << "Device User ID: " +  pCam->DeviceUserID.GetValue() << endl;
		//
		//cameraCnt = atoi(deviceUser_ID.c_str());
		//cout << "CamerCnt: " << cameraCnt << endl;
		//cout << endl;
		int imageCnt = 0;
		while (GetAsyncKeyState(VK_ESCAPE) == 0)
			//for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
		{
			try
			{
				// Retrieve next received image and ensure image completion
				ImagePtr pResultImage = pCam->GetNextImage(1000);

				if (pResultImage->IsIncomplete())
				{
					cout << "[" << serialNumber << "] "
						<< "Image incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl
						<< endl;
				}
				else
				{

					//Acquire the image buffer to write to a file
					char *imageData = static_cast<char*>(pResultImage->GetData());
					string deviceUser_ID = ptrDeviceUserId->GetValue();
					//cout << "Device User ID is: " + deviceUser_ID << endl;
					//cout << "Device User ID: " +  pCam->DeviceUserID.GetValue() << endl;

					cameraCnt = atoi(deviceUser_ID.c_str());
					// cameraCnt = number of the File to be writteninto
					//cout << "CamerCnt: " << cameraCnt << endl;

					// Do the writing
					cameraFiles[cameraCnt].write(imageData, pResultImage->GetImageSize());
					csvDatei<<pResultImage->GetFrameID()<< "," << pResultImage->GetTimeStamp() <<","<< serialNumber<< endl;

					//csvFile[cameraCnt] << pResultImage->GetFrameID() << "," << pResultImage->GetTimeStamp() << endl;
					//cout << endl << cameraCnt << endl;
					// Check if the writing is successful
					if (!cameraFiles[cameraCnt].good())
					{
						cout << "Error writing to file for camera " << cameraCnt << " !" << endl;
						return -1;
					}
					/*ChunkData chunkData = pResultImage->GetChunkData();
					cout << serialNumber + " :  Frame ID: " << pResultImage->GetFrameID() << endl;
					cout << serialNumber + " :  Timestamp: " << pResultImage->GetTimeStamp() << endl;

					cout << serialNumber + " : Chunk Frame ID: " << chunkData.GetFrameID() << endl;
					cout << serialNumber + " : Chunk Timestamp: " << chunkData.GetTimestamp() << endl;*/

					//if (imageCnt == 0)
					//{
					//	// This is the first image, set up the variables
					//	//frameIDPrevious[cameraCnt] = pResultImage->GetFrameID();

					//	//frameIDDelta[cameraCnt] = 1;

					//	//save image size
					//	imageHeight.at(cameraCnt) = pResultImage->GetHeight();

					//	imageWidth.at(cameraCnt) = pResultImage->GetWidth();
					//}
					//else
					//{
						// Get the difference in sequence numbers between the current image and
						// the last image we received
					//	frameIDDelta[cameraCnt] = pResultImage->GetFrameID() - frameIDPrevious[cameraCnt];
					//	cout << serialNumber + " :  Frame ID: " << pResultImage->GetFrameID() << endl;
					//}

					/*if (frameIDDelta[cameraCnt] != 1)
					{
						missedImageCnts += frameIDDelta[cameraCnt] - 1;
						cout << "Image " << frameIDDelta[cameraCnt] - 1 << " missed at camera " << cameraCnt << " "+ serialNumber << endl<<endl;
					}

					frameIDPrevious[cameraCnt] = pResultImage->GetFrameID();
*/

// saving on the disk but not into a generic file

//// Convert image to mono 8
//ImagePtr convertedImage = pResultImage->Convert(PixelFormat_Mono8, HQ_LINEAR);

//// Create a unique filename
//ostringstream filename;

//filename << "E:\\AcquisitionMultipleThread-";
//if (serialNumber != "")
//{
//	filename << serialNumber.c_str();
//}

//filename << "-" << imageCnt << ".jpg";

//// Save image
//convertedImage->Save(filename.str().c_str());

//// Print image information
//cout << "[" << serialNumber << "] "
//	<< "Grabbed image " << imageCnt << ", width = " << pResultImage->GetWidth()
//	<< ", height = " << pResultImage->GetHeight() << ". Image saved at " << filename.str() << endl;
				}
				/*imageCnt++;
				k_numImages[cameraCnt] = imageCnt;*/
				// Release image
				pResultImage->Release();

				//cout << endl;
			}
			catch (Spinnaker::Exception& e)
			{
				cout << "[" << serialNumber << "] "
					<< "Error: " << e.what() << endl;
			}
		}
		//cout << "We missed total of " << missedImageCnts << " images for camera "+serialNumber + " !" << endl << endl;
		// End acquisition
		pCam->EndAcquisition();
		cameraFiles[cameraCnt].close();
		csvFile[cameraCnt].close();
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

// This function acts as the body of the example
int RunMultipleCameras(CameraList camList)
{
	int result = 0;
	unsigned int camListSize = 0;

	try
	{
		// Retrieve camera list size
		camListSize = camList.GetSize();

		// Create an array of CameraPtrs. This array maintenances smart pointer's reference
		// count when CameraPtr is passed into grab thread as void pointer

		// Create an array of handles
		CameraPtr* pCamList = new CameraPtr[camListSize];
#if defined(_WIN32)
		HANDLE* grabThreads = new HANDLE[camListSize];
#else
		pthread_t* grabThreads = new pthread_t[camListSize];
#endif


		int missedImageCnts = 0;
		string serialNumber = "";
		imageHeight.resize(camListSize);

		imageWidth.resize(camListSize);
		for (unsigned int i = 0; i < camListSize; i++)
		{
			// Select camera
			int j = 1;
			pCamList[i] = camList.GetByIndex(i);
			pCamList[i]->Init();
			INodeMap& nodeMap = pCamList[i]->GetNodeMap();
			//TODO 
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
			string deviceUser_ID = ptrDeviceUserId->GetValue();
			cout << "Device User ID is: " + deviceUser_ID << endl;
			// Create files to write
		/*	gcstring number="9";
			char test;
			test = char(i);
			number =  gcstring(&test);
			string tests = "test"; */
			//ptrDeviceUserId->SetValue(number);
			stringstream ss;
			ss << i;
			//if (serialNumber == "20323052")
			//{
			//	//stringstream ss;
			//	ss << 0;
			//}
			//else
			//{
			//	//	stringstream ss;
			//	ss << j;

			//}

			//ptrDeviceUserId->SetValue(ss.str().c_str());
			ptrDeviceUserId->SetValue(ss.str().c_str());
			deviceUser_ID = ptrDeviceUserId->GetValue();
			cout << "Device User ID applied is: " + deviceUser_ID << endl;
			cout << "SerialNumber: " << serialNumber << endl;
			cout << " ########## " << endl;
			j++;

			//###############################
			//INodeMap& nodeMap = pCam->GetNodeMap();
			if (serialNumber == "20323052")  // if MASTER   "17458277")//
			{
				ConfigureStrobe(pCamList[i], nodeMap);
				chosenTrigger = SOFTWARE;  // master camera will work in free running
				ConfigureTrigger(nodeMap);
				cout << " Strobe & Trigger configured for MAster Camera" << endl;
			}
			else
			{
				//chosenTrigger = SOFTWARE; // slave cameras
				chosenTrigger = HARDWARE; // slave cameras
				ConfigureTrigger(nodeMap);
				cout << " Trigger configured for slave Camera" << endl;
			}
			//	ConfigureChunkData(nodeMap); //enable chunk data on cameras
#ifdef _DEBUG
			cout << endl << endl << "*** DEBUG ***" << endl << endl;

			// If using a GEV camera and debugging, should disable heartbeat first to prevent further issues
			if (DisableHeartbeat(pCamList[i], nodeMap, pCamList[i]->GetTLDeviceNodeMap()) != 0)
			{
#if defined(_WIN32)
				return 0;
#else
				return (void*)0;
#endif
			}

			cout << endl << endl << "*** END OF DEBUG ***" << endl << endl;
#endif
			BufferHandlingSettings(pCamList[i]);
			ConfigureStrobe(pCamList[i], nodeMap);
			ConfigureExposure(nodeMap);


			//#####################

			if (CreateFiles(cameraFiles, filename, i, serialNumber))
			{
				cout << "There was error creating the files. Aborting..." << endl;
				return -1;
			}

			pCamList[i]->DeInit();
		}

		for (unsigned int i = 0; i < camListSize; i++)
		{
			// Start grab thread
#if defined(_WIN32)
			grabThreads[i] = CreateThread(nullptr, 0, AcquireImages, &pCamList[i], 0, nullptr);
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
					"Please check onscreen print outs for error details"
					<< endl;
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

// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
int main(int /*argc*/, char** /*argv*/)
{
	// Since this application saves images in the current folder
	// we must ensure that we have permission to write to this folder.
	// If we do not have permission, fail right away.
	FILE* tempFile = fopen("test.txt", "w+"); //fopen("E:\\test.txt", "w+");
	if (tempFile == nullptr)
	{
		cout << "Failed to create file in current folder.  Please check permissions." << endl;
		cout << "Press Enter to exit..." << endl;
		getchar();
		return -1;
	}

	fclose(tempFile);
	remove("test.txt");

	int result = 0;

	// Print application build information
	cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

	// Retrieve singleton reference to system object
	SystemPtr system = System::GetInstance();

	// Print out current library version
	const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
	cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
		<< "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
		<< endl;

	// Retrieve list of cameras from the system
	CameraList camList = system->GetCameras();

	unsigned int numCameras = camList.GetSize();

	cout << "Number of cameras detected: " << numCameras << endl << endl;

	// Finish if there are no cameras
	if (numCameras == 0)
	{
		// Clear camera list before releasing system
		camList.Clear();

		// Release system
		system->ReleaseInstance();

		cout << "Not enough cameras!" << endl;
		cout << "Done! Press Enter to exit..." << endl;
		getchar();

		return -1;
	}

	// Run example on all cameras
	cout << endl << "Running example for all cameras..." << endl;

	result = RunMultipleCameras(camList);

	//RetrieveImagesFromFiles(filename, numCameras, "bmp");
	cout << "Example complete..." << endl << endl;

	// Clear camera list before releasing system
	camList.Clear();

	// Release system
	system->ReleaseInstance();

	cout << endl << "Done! Press Enter to exit..." << endl;
	getchar();

	return result;
}

