"""
====================================================================================================
This script was developed to compress video files from BINtoAVI.cpp as well as GOPRO recordings
before using DeepLabCut. 

The video file should be 1) compressed, 2) appended and 3) Trimed

MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com

Sourcecode: https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR
====================================================================================================
"""

import os
from tkinter import filedialog





def compress_h265(crf):
    '''
    Video Compression with H265: This function reads video filenames from a list and compresses with H264 codec. Audio is removed. 
    '''
    videofile = filedialog.askopenfilenames(title='Choose Video Files you want to compress')
    for video in videofile:
        filename = video[:-4]
        output = filename + '_h265.mp4'
        
        # crf from 0 to 51, 18 recommended lossless average
        ffmpeg_command = f"ffmpeg -i {video} -an -vcodec libx265 -crf {crf} {output}"
        print(ffmpeg_command)
        os.system(ffmpeg_command)
	# color settings: ffmpeg -i input -vf format=gray output

         
def concat_videos():
    # select videos to append and write to txt
    concatlist= filedialog.askopenfilenames(title='Choose Video Files you want to concatenate')
    ## ??? video ORDER?
    with open("mylist.txt", "w+") as textfile:
        for element in concatlist:
            textfile.write("file " +"'"+ element + "'\n")
        textfile.close()

    # name output file
    output = input(f"Output filename for {concatlist[0]}: ")
    output = output + '.mp4'
    
    ffmpeg_command = f"ffmpeg -f concat -safe 0 -i mylist.txt -c copy {output}"
    print(ffmpeg_command)
    os.system(ffmpeg_command)


def trim_and_split():
    pass
    

## Entry point
print("########################################################")
print("       Welcome to the the video compression tool")
print("MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com")
print("########################################################")

choice = ''
print("You can Compress, Append and Trim videos, or Quit...")

while True:
    
    if choice.startswith("c"):
        # start compression
        print("\nStart video compression in ffmpeg with H265 codec... \n")
        crf = input("Choose constant rate factor between 0-50: ")
        compress_h265(crf)
        # reset while loop
        choice = ''
        
    elif choice.startswith("a"):
        print("\nStart appending videos in ffmpeg... \n")
        concat_videos()
        # reset while loop
        choice = ''
        
    elif choice.startswith("t"):
        print("\nStart trimming videos in ffmpeg... \n")
        # reset while loop
        choice = ''
        
    elif choice.startswith("q"):
        break
        
    else:
        choice = input("What would you like to do? (C/A/T/Q): ")
    
