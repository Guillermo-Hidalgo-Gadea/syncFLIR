# syncFLIR [<img src="https://github.com/Guillermo-Hidalgo-Gadea/personal-academic-website/blob/master/assets/images/icon.png" alt="logo" align="right" width="100"/>](https://guillermohidalgogadea.com/)
A synchronized multiview video recording setup using FLIR cameras and Spinnaker [SDK](http://softwareservices.flir.com/Spinnaker/latest/index.html) on a Windows machine.

## **!!! The project has migrated to [GitLab](https://gitlab.ruhr-uni-bochum.de/ikn/syncflir) !!!**

This code compilation allows to operate a wired setup of FLIR cameras for synchronized video recording. Due to the high data stream with incresing number of cameras and the processing speed required for high fps recordings, it is recommended to use at least 30GB of RAM and runnig the video recording on a SSD disk. This setup was developed and tested using 6 USB3 cameras connected to 2 separate windows machines. The three cameras on the secondary machine run in idle mode until triggered by the primary camera from the first machine, thus synchronizing all six cameras over both machines.

To further optimize processing speed, cameras dump their recording to a binary file as intermediate storage (**RECtoBIN.cpp**). The recording process is logged in a .csv file with exact time stamps and frame IDs. In a subsequent process, the binary files can be read out and converted to .avi video files (**BINtoAVI.cpp**). In case regular video players fail to play your videos (e.g., due to broken codec) try using OpenCV in python (**VideoPlayer.py**). To keep track of recording consistancy (i.e., skipped frames, inter-frame-intervals, average fps, etc.) use the diagnostics tool provided (**Diagnostics.py**) to analyze the recording log.

## Materials
* Computer Vision cameras from [FLIR](https://www.flir.eu/products/blackfly-s-usb3/?model=BFS-U3-16S2C-CS)
* Appropriate lenses depending on setup, see guide [here](https://www.flir.eu/iis/machine-vision/lens-calculator/)
* GPIO cables for synchronized trigger from [FLIR](https://www.flir.eu/products/hirose-hr10-6-pin-circular-connector/?model=ACC-01-3009)
* Wiring for synchronized trigger, see guide [here](https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/)

## Instructions
1) To record multiple synchronized videos to binary file use RECtoBIN.cpp

![RECtoBIN terminal output](https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR/blob/main/archive/screenshot1.png)


2) To convert the recorded binary files use BINtoAVI.cpp  

![BINtoAVI terminal output](https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR/blob/main/archive/screenshot2.png)

3) To play the video use VideoPlayer.py 

4) To pocess your recording logfile run the Diagnostics.py program

Check the diagnostics report for missing and skipped frames between cameras
![Diagnostics output](https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR/blob/main/archive/DiagnosticReport_20210317142329.png)

## Funding
The development of syncFLIR was supported by the German Research Foundation DFG in the context of funding the Research Training Group “Situated Cognition” (GRK 2185/1). *Original: Gefördert durch die Deutsche Forschungsgemeinschaft (DFG) - Projektnummer GRK 2185/1*.
