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


// PARAMETERS FROM RECORDING SETTINGS
// TODO: parameters as text input instead of hardcoded before compilation...
float frameRateToSet = 194; 
int imageHeight = 1080; 
int imageWidth = 1440;
int color = 1; // 1= color, else = mono
// TODO: change color format
// TODO: compression videoType?

// select the type of video compression
enum videoType
{
	UNCOMPRESSED,
	MJPG,
	H264
};
const videoType chosenVideoType = MJPG;


/*
=================
The function SaveVectorToVideo takes an image vector and converts it into avi. Parameters like frameRateToSet, k_videoFileSize, MJPGquality and H264bitrate are changed within the function.
=================
*/
int SaveVectorToVideo(string tempFilename, vector<ImagePtr>& images, int part)
{
	int result = 0;

	// Since this application saves images in the current folder
	// we must ensure that we have permission to write to this folder.
	// If we do not have permission, fail right away.
	FILE* tempFile = fopen("test.txt", "w+");
	if (tempFile == nullptr)
	{
		cout << "Failed to create file in current folder.  Please check permissions." << endl;
		cout << "Press Enter to exit..." << endl;
		getchar();
		return -1;
	}

	fclose(tempFile);
	remove("test.txt");


	cout << endl << "*** CONVERTING VIDEO ***" << endl << endl;
	try
	{
		// FILENAME (TODO: append compression type 
		string videoFilename = tempFilename.substr(0, tempFilename.length() - 4) + "_" + to_string(part);

		cout << "Frame Rate set to " << frameRateToSet << "FPS" << endl;

		// Start and open video file
		SpinVideo video;


		// Set maximum video file size to 2GB. A new video file is generated when limit is reached. Setting maximum file size to 0 indicates no limit.
		const unsigned int k_videoFileSize = 0;

		video.SetMaximumFileSize(k_videoFileSize);

		// Setting chosenVideoType. Once the desired option object is configured, open the video file with the option in order to create the video file.

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
			option.quality = 95;

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
		cout << "Appending " << images.size() << " images to video file... ";

		for (int imageCnt = 0; imageCnt < images.size(); imageCnt++)
		{
			video.Append(images[imageCnt]);
		}

		// Close video file
		video.Close();
		cout << " done!" << endl;
		cout << "Video " << videoFilename << ".avi saved!" << endl << endl;
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
	// PixelFormat_BayerRG8;  see http://softwareservices.flir.com/Spinnaker/latest/group___camera_defs__h.html#ggabd5af55aaa20bcb0644c46241c2cbad1aab73a79118b4db06f577b5e2563d7246

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

				// Import binary image into Image structure
				ImagePtr pImage = Image::Create(imageWidth, imageHeight, 0, 0, PixelFormat_BayerRG8, imageBuffer);

				// Deep copy image into image vector
				images.push_back(pImage->Convert(PixelFormat_BayerRG8, HQ_LINEAR));

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


	// TODO: Ask for files with Dialog
	// Manual input of filenames to be converted
	vector<string> filenames = {};
	string S, T;
	cout << endl << "Enter the binary files to convert separated by a + sign: " << endl;
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

	// Image vector saved to video within Retrieve Image loop

	cout << "Conversion complete." << endl;

	cout << endl << "Done! Press Enter to exit..." << endl;

	// Print application build information
	cout << "*************************************************************" << endl;
	cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl;
	cout << "MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
	cout << "*************************************************************" << endl;
	getchar();

	return result;
}
