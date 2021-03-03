"""
====================================================================================================
This script was developed to visualize the video output from BINtoAVI.cpp as comercial video 
players have problems with broken avi index or codec (?).

After recording hardware triggered, synchronized images to binary files and converting binaries
to avi files in the previous script, this python script reads the avi output in openCV for visual
inspection.

MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com

Sourcecode: https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR
====================================================================================================
"""

# importing libraries 
import cv2 
from tkinter import filedialog

# Choose Video File from Dialog Box
videofile = filedialog.askopenfilenames(title='Choose the Video Files you want to play')

# Create a VideoCapture object and read from input file 
cap = cv2.VideoCapture(videofile[0]) 

# Calculate Video Properties
fps = int(cap.get(cv2.CAP_PROP_FPS))
total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
duration = total/fps

print("Video Frame Rate: %d FPS" %fps)
print("Frames contained: %d" %total)
print("Video duration: %d sec" %duration)

# Check if camera opened successfully 
if (cap.isOpened()== False): 
    print("Error opening video file") 

# Read until video is completed 
while(cap.isOpened()):
    # Capture frame-by-frame
    ret, frame = cap.read()
        
    if ret == True:
        # resize frame
        h, w, c = frame.shape
        frame = cv2.resize(frame, (int(w/2), int(h/2))) 
        
        # Display the resulting frame
        cv2.imshow('Frame', frame)
               
        # Press Q on keyboard to exit
        if cv2.waitKey(25) & 0xFF == ord('q'):
            break
        
    # Break the loop
    else:
        break

# When everything done, release 
# the video capture object 
cap.release() 

# Closes all the frames 
cv2.destroyAllWindows() 
