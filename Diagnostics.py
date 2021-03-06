"""
====================================================================================================
This script was developed to create a diagnostics report from the csv logfile generated during
video recording with the synchFLIR RECtoBIN and BINtoAVI scripts.

After recording hardware triggered, synchronized images to binary files and converting binaries
to avi files in the previous script, this python script reads the csv logfile and generates a
recording diagnostics report to analyze framerate, dropped frames and recording length.

MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com

Sourcecode: https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR
====================================================================================================
"""

# Importing libraries
import pandas as pd
import numpy as np
import datetime
import time
import matplotlib.pyplot as plt
from tkinter import filedialog
from scipy import stats
from reportlab.pdfgen import canvas


# Choose CVS File from Dialog Box
logfilepath = filedialog.askopenfilenames(title='Choose the csv log file to analyze')
logfile = pd.read_csv(logfilepath[0])
#print(logfile)

# Initialize PDF
Date = str(datetime.date.today())
fileName = logfilepath[0][0:-19]+ 'DiagnosticReport_'+ logfilepath[0][-19:-4] +'.pdf'
documentTitle = 'Diagnostic Report syncFLIR'
title = 'Diagnostic Report'
subTitle = 'Recording: ' + logfilepath[0][-19:-4]
textLinesIntro = [
'This diagnostics report provides visual feedback for the video recording mentioned above,',
'as extracted from the recording logfile generated with the RECtoBIN executable. Should the',
'overall recording performance be unsatisfactory, try increasing the RAM and saving to SSD.',
'Report generated with Diagnostics.py - MIT Copyright (c) 2021 GuillermoHidalgoGadea.com']

# Create PDF
pdf = canvas.Canvas(fileName)
pdf.setTitle(documentTitle)
pdf.setFont("Times-Roman",12)
pdf.drawRightString(500,790, Date)
pdf.setFont("Times-Bold",16)
pdf.drawCentredString(290,770, title) #(x = [0, 600], x = [0, 800])
pdf.drawCentredString(290, 750, subTitle)
pdf.drawInlineImage('logo.png', 40, 750, width = 70, height = 70)
pdf.line(50, 720, 550, 720)


intro = pdf.beginText(70, 700)
intro.setFont("Times-Roman",12)
for line in textLinesIntro:
    intro.textLine(line)
pdf.drawText(intro)
pdf.line(50, 640, 550, 640)

# Split logfile by Serial number
grouped = logfile.groupby(logfile.SerialNumber)

# positioning for output text and figures in pdf file
htext = 610
wtext = 50
whist = 30
himage = 270

for serial in grouped.grouper.levels[0]:
    # Diagnose recording
    group = grouped.get_group(serial)
    group.sort_values(by=['FrameID'], inplace=True)
    lastFrame = group['FrameID'].max()
    timespan = (group['Timestamp'].max()-group['Timestamp'].min())/1e9
    group['IntFramesInt'] = group['Timestamp'].diff()/1e9
    group['FrameSkip'] = group['FrameID'].diff()-1
    avgfps = lastFrame/timespan
    meanfps = 1/group.IntFramesInt.mean()
    critFPS = group.IntFramesInt[group.IntFramesInt > .04].count()
    skipFrames = group.FrameSkip.sum()
    missingFrames = logfile['FrameID'].max()-lastFrame

    # save output
    serialnum = 'Camera:         #' + str(serial)
    numframes = 'Total frames:    ' + str(lastFrame)
    duration =  'Recording time:  ' + time.strftime("%M:%S",time.gmtime(timespan))
    avgfps =    'Frames/Time:     ' + str("{:.2f}".format(avgfps))
    meanfps =   'Mean FPS:        ' + str("{:.2f}".format(meanfps))
    critical =  'Critical frames: ' + str(critFPS)
    skipped =   'Skipped frames:  ' + str(int(skipFrames))
    missing =   'Missing frames:  ' + str(int(missingFrames))

    textLinesReport = [
        serialnum,
        numframes,
        duration,
        avgfps,
        meanfps,
        critical,
        skipped,
        missing]

    # Plot FPS time series
    plt.rcParams['font.size'] = '12'
    timeseries = 'timeseries-' + str(serial) + '.png'
    fig, ax = plt.subplots(figsize=(10, 2))
    ax.plot(group.FrameID, group.IntFramesInt, marker='.', alpha=0.3, color = 'black', linestyle='solid')
    ax.axhline(y=.04, color='r', linestyle='-', lw=2)
    plt.text(0,0.045,'FPS = 25',color='r',rotation=0)
    ax.axhline(y=.02, color='y', linestyle='-', lw=2)
    plt.text(0,0.025,'FPS = 50',color='y',rotation=0)
    ax.axhline(y=.005, color='g', linestyle='-', lw=2)
    plt.text(0,.01,'FPS = 200',color='g',rotation=0)
    plt.ylabel('Inter Frame Interval')
    plt.xlabel('Frame ID')
    plt.title(serial)
    plt.savefig(timeseries)
    #plt.show()

    # Plot FPS Histogram
    plt.rcParams['font.size'] = '34'
    histogram = 'histogram-' + str(serial) + '.png'
    res = stats.relfreq(group.IntFramesInt.dropna(), numbins=30)
    x = res.lowerlimit + np.linspace(0, res.binsize*res.frequency.size, res.frequency.size)
    fig, ax = plt.subplots(figsize = (18,12))
    ax.bar(x, res.frequency, width=res.binsize)
    ax.axvline(x=.04, color='r', linestyle='-', lw=1)
    plt.text(.05,.4,'FPS = 25', color='r', rotation=0)
    ax.axvline(x=.02, color='y', linestyle='-', lw=1)
    plt.text(.05,.45,'FPS = 50',color='y', rotation=0)
    ax.axvline(x=.005, color='g', linestyle='-', lw=1)
    plt.text(.05,.5,'FPS = 200',color='g', rotation=0)
    plt.xlabel('Inter Frame Interval')
    plt.xlim(0,0.075)
    plt.ylabel('Relative Frequency')
    plt.title(serial)
    plt.savefig(histogram)
    #plt.show()

    # write diagnostics to pdf
    text = pdf.beginText(wtext, htext)
    text.setFont("Times-Roman",11)
    for line in textLinesReport:
        text.textLine(line)
    pdf.drawText(text)

    # write timeseries figures to pdf
    pdf.drawInlineImage (timeseries, 0, himage, width=600, height = 120)

    # wirte histograms to pdf
    pdf.drawInlineImage (histogram, whist, 390, width=180, height = 120)

    # move next text to the right
    wtext = wtext + 180
    # move histogram to the right
    whist = whist + 180
    # move image down
    himage = himage - 125

pdf.save()
