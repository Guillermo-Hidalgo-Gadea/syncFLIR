/*
====================================================================================================
This script was developed with close assistance by the FLIR Systems Support Team, and is adapted
from examples in FLIR Systems/Spinnaker/src. Copyright from FLIR Integrated Imaging Solutions, Inc. applies.

After recording hardware triggered, synchronized images to binary files in the previous script,
this file converts the binary file back to a vector of images and creates a video file.
Note that FrameRate and imageHeight and imageWidth are hardcoded from previous recording settings
and need to be adapted before compiling the executable file. Install Spinnaker SDK before using this script.

MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com
Sourcecode: https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR
====================================================================================================
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <assert.h>
#include <time.h>
#include "SpinVideo.h"
#include <algorithm>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace Spinnaker::Video;
using namespace std;


// Initialize Config parameters will be updated by config file
double frameRateToSet = 100;
int imageHeight = 1080;
int imageWidth = 1440;
int color = 1; // 1= color, else = mono
std::string chosenVideoType = "MJPG"; 
std::string path;

/*
================
This function reads the config file to update camera parameters such as directory path, Framerate and image size from previous recording
================
*/
auto readconfig(string metadata)
{
	int result = 0;

	std::ifstream cFile(metadata);
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
			if (name == "Framerate") frameRateToSet = std::stod(value);
			else if (name == "ImageHeight") imageHeight = std::stod(value);
			else if (name == "ImageWidth") imageWidth = std::stod(value);
			else if (name == "ColorVideo") color = std::stod(value);
			else if (name == "chosenVideoType") chosenVideoType = value;
			else if (name == "VideoPath") path = value;
		}
	}
	else
	{
		std::cerr << "Couldn't open config file for reading.\n";
	}

	cout << "\nFramerate=" << frameRateToSet;
	std::cout << "\nImageHeight=" << imageHeight;
	std::cout << "\nImageWidth=" << imageWidth;
	std::cout << "\nColorVideo=" << color;
	std::cout << "\nchosenVideoType=" << chosenVideoType;
	std::cout << "\nVideoPath=" << path << endl << endl;

	return result, frameRateToSet, imageHeight, imageWidth, color, chosenVideoType, path;
}



/*
=================
The function SaveVectorToVideo takes an image vector and converts it into avi. Parameters like frameRateToSet, k_videoFileSize, MJPGquality and H264bitrate are changed within the function.
=================
*/
int SaveVectorToVideo(string tempFilename, vector<ImagePtr>& images, int part)
{
	int result = 0;

	// Test write permission
	string testpath = path + "/test.txt";
	const char* testfile = testpath.c_str() ;
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


	cout << endl << "*** CONVERTING VIDEO ***" << endl << endl;
	try
	{
		// FILENAME 
		string videoFilename = path + tempFilename.substr(3, tempFilename.length() - 7) + "_" + to_string(part);

		cout << "Frame Rate set to " << frameRateToSet << " FPS" << endl;

		// Start and open video file
		SpinVideo video;

		// Set maximum video file size to 4GB. A new video file is generated when limit is reached. Setting maximum file size to 0 indicates no limit.
		const unsigned int k_videoFileSize = 4096;

		video.SetMaximumFileSize(k_videoFileSize);

		// Setting chosenVideoType. Once the desired option object is configured, open the video file with the option in order to create the video file.	
		if (chosenVideoType =="MJPG")
		{
			Video::MJPGOption option;

			option.frameRate = frameRateToSet;
			option.quality = 95;

			video.Open(videoFilename.c_str(), option);
			cout << "VideoType set to MJPG" << endl;
		}
		else if (chosenVideoType == "H264")
		{
			Video::H264Option option;

			option.frameRate = frameRateToSet;
			option.bitrate = 1000000;
			option.height = static_cast<unsigned int>(images[0]->GetHeight());
			option.width = static_cast<unsigned int>(images[0]->GetWidth());

			video.Open(videoFilename.c_str(), option);
			cout << "VideoType set to H264" << endl;
		}
		else // UNCOMPRESSED
		{
			Video::AVIOption option;

			option.frameRate = frameRateToSet;

			video.Open(videoFilename.c_str(), option);
			cout << "VideoType set to UNCOMPRESSED" << endl;
		}


		// Construct and save video
		cout << "Appending " << images.size() << " images to video file... ";

		for (int imageCnt = 0; imageCnt < images.size(); imageCnt++)
		{
			video.Append(images[imageCnt]);
		}

		// Close video file
		video.Close();
		cout << " done!" << endl;
		cout << "Video " << videoFilename << ".avi saved!"  << endl;
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
int RetrieveImagesFromFiles(vector<string>& filenames, int numFiles)
{
	int result = 0;
	int imageSize = imageHeight * imageWidth;

	try {
		// Loop through the binary filenames and retrieve images of imageSize
		for (int fileCnt = 0; fileCnt < numFiles; fileCnt++)
		{
			string tempFilename = filenames.at(fileCnt);
			cout << endl << "*** READING BINARY FILE ***" << endl << endl;
			cout << "Opening " << tempFilename.c_str() << "..." << endl;

			ifstream rawFile(filenames.at(fileCnt).c_str(), ios_base::in | ios_base::binary);

			if (!rawFile)
			{
				cout << "Error opening file: " << filenames.at(fileCnt).c_str() << " Aborting..." << endl;

				return -1;
			}

			cout << "Splitting images...";

			// Save acquired images into images vector
			vector<ImagePtr> images;

			// Binary video recordings get very large and may not fit in RAM read file and write video in steps
			int frameCnt = 0;
			int part = 1;

			while (rawFile.good())
			{
				// Split binary reading in case .tmp is too large for memory
				if (frameCnt > 5000)
				{
					// Saving images from split above
					cout << "Retrieved images from Binary file: " << images.size() << endl;

					// start converting image vector into avi for fileCnt loop
					result = SaveVectorToVideo(tempFilename, images, part);

					// reset for next batch
					images.clear();
					frameCnt = 0;
					part++;
				}

				// Reading images from Binary
				char* imageBuffer = new char[imageSize];

				rawFile.read(imageBuffer, imageSize);

				if (color == 1)
				{
					// Import binary image into BayerRG8 Image structure
					ImagePtr pImage = Image::Create(imageWidth, imageHeight, 0, 0, PixelFormat_BayerRG8, imageBuffer);

					// Deep copy image into BayerRG8 image vector
					images.push_back(pImage->Convert(PixelFormat_BayerRG8, HQ_LINEAR));
				}
				else
				{
					// Import binary image into Mono8 Image structure
					ImagePtr pImage = Image::Create(imageWidth, imageHeight, 0, 0, PixelFormat_Mono8, imageBuffer);

					// Deep copy image into Mono8 image vector
					images.push_back(pImage->Convert(PixelFormat_Mono8, HQ_LINEAR));
				}

				// Delete the acquired buffer
				delete[] imageBuffer;

				// update frame counter
				frameCnt++;
			}

			// Saving rest images from last split 
			cout << "Retrieved images from Binary file: " << images.size() << endl;

			// start converting image vector into avi for fileCnt loop
			result = SaveVectorToVideo(tempFilename, images, part);

			// Close the file
			cout << "Closing binary file" << endl;
			rawFile.close();
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
	cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl;
	cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
	cout << "*************************************************************" << endl;

	// Ask for metadata first to update config parameters
	string metadata;
	cout << endl << "Enter the METADATA file of the specific recording to convert: " << endl;
	getline(cin, metadata);
	cout << endl << "Setting parameters from " + metadata + " ... " << endl;

	// Set configuration parameters
	readconfig(metadata);

	// Manual input of Binary filenames to be converted
	vector<string> filenames = {};
	string S, T;
	cout << endl << "Enter the BINARY files to convert separated by a + sign: " << endl;
	getline(cin, S); // read entire line
	stringstream X(S);

	while (getline(X, T, '+')) { //separate input by + separator
		filenames.push_back(T); // save input as string vector
	}

	int numFiles = filenames.size();
	string num = to_string(numFiles);
	cout << endl << "Converting BINtoAVI for " + num + " files..." << endl;
	for (int files = 0; files < numFiles; files++)
	{
		cout << filenames[files] << endl;
	}

	cout << endl << "Press Enter to convert files" << endl << endl;
	getchar(); // pass filenames vector to RetrieveImagesFromFiles


	// Retrieve images from .tmp file
	result = RetrieveImagesFromFiles(filenames, numFiles);


	cout << endl << "Conversion complete! Press Enter to exit..." << endl;

	// Print application build information
	cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl;
	cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
	cout << "*************************************************************" << endl;
	getchar();

	return result;
}
