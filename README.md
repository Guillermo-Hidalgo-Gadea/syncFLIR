# syncFLIR
A synchronized multiview video recording setup using FLIR cameras and Spinnaker [SDK](http://softwareservices.flir.com/Spinnaker/latest/index.html) on a Windows machine.

## Materials
* Computer Vision cameras from [FLIR](https://www.flir.eu/products/blackfly-s-usb3/?model=BFS-U3-16S2C-CS)
* Appropriate lenses depending on the usecase, see guide [here](https://www.flir.eu/iis/machine-vision/lens-calculator/)
* GPIO cables for synchronized trigger from [FLIR](https://www.flir.eu/products/hirose-hr10-6-pin-circular-connector/?model=ACC-01-3009)
* Wiring for synchronized trigger, see guide [here](https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/)

## Instructions
1) To record multiple synchronized videos to binary file use RECtoBIN.cpp

![RECtoBIN terminal output](https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR/blob/main/archive/screenshot1.png)


2) To convert the recorded binary files use BINtoAVI.cpp  

![BINtoAVI terminal output](https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR/blob/main/archive/screenshot2.png)

3) To play the video use VideoPlayer.py 

4) Pocess your recording logfile saved as csv with the Diagnostics.py program
![Diagnostics output](https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR/blob/main/archive/screenshot2.png)
