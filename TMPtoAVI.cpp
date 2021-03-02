/*
====================================================================================================
This script was developed with close assistance by the FLIR Systems Support Team, and is adapted 
from the examples SavetoAvi.cpp and AcquisitionMultipleThread.cpp.

After recording hardware triggered, synchronized images to binary files in the previous script,
this file converts the binary file back to a vector of images and creates a video file.
Note that FrameRate and imageHeight and imageWidth are hardcoded from previous recording settings.

Install Spinnaker SDK before using this script.

MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com 2021

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
#include <assert.h>
#include <time.h>
#include "SpinVideo.h"

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace Spinnaker::Video;
using namespace std;


// select the type of video compression
enum videoType
{
	UNCOMPRESSED,
	MJPG,
	H264
};
const videoType chosenVideoType = UNCOMPRESSED;
std::string compression(1,chosenVideoType);

/*
=================
The function SaveVectorToVideo takes an image vector and converts it into avi. Parameters like frameRateToSet, k_videoFileSize, MJPGquality and H264bitrate are changed within the function.
=================
*/
int SaveVectorToVideo(string tempFilename, vector<ImagePtr>& images)
{
	int result = 0;

	// Since this application saves images in the current folder
	// we must ensure that we have permission to write to this folder.
	// If we do not have permission, fail right away.
	FILE* tempFile = fopen("E:\\FlirCamera\\test.txt", "w+");
	if (tempFile == nullptr)
	{
		cout << "Failed to create file in current folder.  Please check permissions." << endl;
		cout << "Press Enter to exit..." << endl;
		getchar();
		return -1;
	}

	fclose(tempFile);
	//remove("E:\\FlirCamera\\test.txt");
	

	cout << endl << endl << "*** CREATING VIDEO ***" << endl << endl;
	
	try
	{
		// *** FILENAME ***
		string videoFilename = tempFilename.substr(0, tempFilename.length() - 4) + "_" + compression;
	
		// *** FRAMERATE *** 
		float frameRateToSet = 199; // hardcoded, check if real fps
		cout << "Frame rate to be set to " << frameRateToSet << "..." << endl;

		// Start and open video file
		SpinVideo video;

		// *** NOTES ***
		// Depending on the file type, a number of settings need to be set in
		// an object called an option. An uncompressed option only needs to
		// have the video frame rate set whereas videos with MJPG or H264
		// compressions should have more values set.
		//
		// Once the desired option object is configured, open the video file
		// with the option in order to create the video file.
		//
		// Set maximum video file size to 5GB / 5120MB.
		// A new video file is generated when 5GB
		// limit is reached. Setting maximum file
		// size to 0 indicates no limit.
		const unsigned int k_videoFileSize = 0; //5120 MB

		video.SetMaximumFileSize(k_videoFileSize);

		if (chosenVideoType == UNCOMPRESSED)
		{
			Video::AVIOption option;

			option.frameRate = frameRateToSet;

			video.Open(videoFilename.c_str(), option);
		}
		else if (chosenVideoType == MJPG)
		{
			Video::MJPGOption option;

			option.frameRate = frameRateToSet;
			option.quality = 75;

			video.Open(videoFilename.c_str(), option);
		}
		else if (chosenVideoType == H264)
		{
			Video::H264Option option;

			option.frameRate = frameRateToSet;
			option.bitrate = 1000000;
			option.height = static_cast<unsigned int>(images[0]->GetHeight());
			option.width = static_cast<unsigned int>(images[0]->GetWidth());

			video.Open(videoFilename.c_str(), option);
		}


		// Construct and save video
		cout << "Appending " << images.size() << " images to video file: " << videoFilename << ".avi" << endl
			<< endl;

		for (int imageCnt = 0; imageCnt < images.size(); imageCnt++)
		{
			cout << "loop" << endl;
			video.Append(images[imageCnt]);

			cout << "\tAppended image " << imageCnt << "..." << endl;
		}

		// Close video file
		video.Close();

		cout << endl << "Video " << videoFilename << ".avi saved!" << endl << endl;
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
The function RetrieveImagesFromFiles loops over all files in filenames vector and retrieves chunks if imageSize from binary file. These chunks are conevrted into image structure and appended to images vector. The images vector is passed to the function SaveVectorToVideo and saved as AVI. Parameters imageHeight and imageWidth are hardcoded from previous recording settings.
=================
*/
int RetrieveImagesFromFiles(vector<string>& filenames, int numFiles, string fileFormat = "bmp") //fileFormat NEEDED??
{
	int result = 0;
	//HARDCODED FROM PREVIOUS RECORDING
	int imageHeight = 1080;
	int imageWidth = 1440;
	int imageSize = imageHeight * imageWidth;
	// PixelFormat_BayerRG8;  see http://softwareservices.flir.com/Spinnaker/latest/group___camera_defs__h.html#ggabd5af55aaa20bcb0644c46241c2cbad1aab73a79118b4db06f577b5e2563d7246

	try {
		// Loop through the binary filenames and retrieve images of imageSize
		for (unsigned int fileCnt = 0; fileCnt < numFiles; fileCnt++)
		{
			string tempFilename = filenames.at(fileCnt);

			cout << "Opening " << tempFilename.c_str() << "..." << endl;

			ifstream rawFile(filenames.at(fileCnt).c_str(), ios_base::in | ios_base::binary);

			if (!rawFile)
			{
				cout << "Error opening file: " << filenames.at(fileCnt).c_str() << " Aborting..." << endl;

				return -1;
			}

			cout << "Start splitting images..." << endl;

			// Read image into buffer
			// HOW TO KNOW THE EXACT k_numImages BEFOREHAND?
			int k_numImages = 100; //rawFile.size() / imageSize;

			// Save acquired images into images vector
			vector<ImagePtr> images;

			for (int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
			{
				char* imageBuffer = new char[imageSize];

				rawFile.read(imageBuffer, imageSize);

				// Import binary image into Image structure
				// HARDCODED ATTRIBUTES FROM PREVIOUS RECORDING
				ImagePtr pImage = Image::Create(imageWidth, imageHeight, 0, 0, PixelFormat_BayerRG8, imageBuffer);

				// Deep copy image into image vector
				images.push_back(pImage);

				// Release image
				//pImage->Release();

				// Delete the acquired buffer
				delete[] imageBuffer;

				// Check if reading is successful
				if (!rawFile.good())
				{
					cout << "Error reading from file " << fileCnt << " !" << endl;

					// Close the file
					rawFile.close();

					return -1;
				}

				cout << "File[" << fileCnt << "]: Retrieved image " << imageCnt << endl;

			}

			// Close the file
			cout << "Closing " << tempFilename.c_str() << "..." << endl;
			rawFile.close();

			// start converting image vector into avi for fileCnt loop
			result = SaveVectorToVideo(tempFilename, images);

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


/*
=================
 Entry point
=================
*/
int main(int /*argc*/, char** /*argv*/)
{
	int result = 0;

	// Print application build information
    cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__  << endl;
    cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com 2021" << endl;
    cout << "*************************************************************" << endl;

	// TODO: Ask for directory with Dialog

	// TODO: List .tmp files in directory as filenames vector
	vector<string> filenames{ "E:\\FlirCamera\\20210210145518_20323040_file_0.tmp", "E:\\FlirCamera\\20210210145522_20323043_file_1.tmp", "E:\\FlirCamera\\20210210145525_20323052_file_2.tmp" };

	int numFiles = filenames.size(); // size list
	string num = to_string(numFiles);
	cout << endl << "Running example for" + num + "files..." << endl;

	// Retrieve images from .tmp file
	result = RetrieveImagesFromFiles(filenames, numFiles, "bmp");

	// Image vector saved to video within Retrieve Image loop

	cout << "Conversion complete..." << endl << endl;

	cout << endl << "Done! Press Enter to exit..." << endl;

	// Print application build information
    cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__  << endl;
    cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com 2021" << endl;
    cout << "*************************************************************" << endl;
	getchar();

	return result;
}

