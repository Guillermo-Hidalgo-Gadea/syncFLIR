import PySpin
import time
import asyncio
import nest_asyncio
import os
from concurrent.futures import ThreadPoolExecutor

nest_asyncio.apply()

class TriggerType:
    SOFTWARE = 1
    HARDWARE = 2

CHOSEN_TRIGGER = TriggerType.HARDWARE

NUM_IMAGES = 50  # number of images to grab

NUM_BUFFERS = 1000 # how to set dynamically?

NUM_SAVERS = 10  # The number of saver coroutines

SAVE_DIRS = ['E:\camA','E:\camB','E:\camC','E:\camD','E:\camE','E:\camF',]

def configure_strobe(cam):
    """
    This function configures the strobe output.
    

     :param cam: Camera to configure strobe for.
     :type cam: CameraPtr
     :return: True if successful, False otherwise.
     :rtype: bool
    """
    result = True

    print('CONFIGURING STROBE')

    try:
        #Line Selector to an Output Line
        nodemap = cam.GetNodeMap()
        node_line_selector = PySpin.CEnumerationPtr(nodemap.GetNode('LineSelector'))
        if not PySpin.IsAvailable(node_line_selector) or not PySpin.IsWritable(node_line_selector):
            print('Unable to set Line selector  (node retrieval). Aborting...')
            return False

        node_Line2 = node_line_selector.GetEntryByName('Line2')
        if not PySpin.IsAvailable(node_Line2) or not PySpin.IsReadable(node_Line2):
            print('Unable to get Line2 Entry from Line selector (enum entry retrieval). Aborting...')
            return False

        node_line_selector.SetIntValue(node_Line2.GetValue())
        print('Line selector set to Line2...')

        # Select Line Mode
        node_line_mode = PySpin.CEnumerationPtr(nodemap.GetNode('LineMode'))
        if not PySpin.IsAvailable(node_line_mode) or not PySpin.IsWritable(node_line_mode):
            print('Unable to get LineMode (node retrieval). Aborting...')
            return False

        node_line_mode_output = node_line_mode.GetEntryByName('Output')
        if not PySpin.IsAvailable(node_line_mode_output) or not PySpin.IsReadable(node_line_mode_output):
            print('Unable to get Line2 Entry from Line selector (enum entry retrieval). Aborting...')
            return False

        node_line_mode.SetIntValue(node_line_mode_output.GetValue())
        print('Line Mode set to Output...')
        
        #Line Source 
        node_line_source = PySpin.CEnumerationPtr(nodemap.GetNode('LineSource'))
        if not PySpin.IsAvailable(node_line_source) or not PySpin.IsWritable(node_line_source):
            print('Unable to get LineMode (node retrieval). Aborting...')
            return False

        node_line_source_ExposureActive = node_line_source.GetEntryByName('ExposureActive')
        if not PySpin.IsAvailable(node_line_source_ExposureActive) or not PySpin.IsReadable(node_line_source_ExposureActive):
            print('Unable to get Line2 Entry from Line selector (enum entry retrieval). Aborting...')
            return False

        node_line_source.SetIntValue(node_line_source_ExposureActive.GetValue())
        print('Line Source set to Exposure Active...')

    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        return False

    return result

def configure_trigger(cam):
    """
    This function configures the camera to use a trigger. First, trigger mode is
    set to off in order to select the trigger source. Once the trigger source
    has been selected, trigger mode is then enabled, which has the camera
    capture only a single image upon the execution of the chosen trigger.

     :param cam: Camera to configure trigger for.
     :param trigger: SOFTWARE or HARDWARE, see class TriggerType
     :type cam: CameraPtr
     :return: True if successful, False otherwise.
     :rtype: bool
    """
    result = True
    
    print('CONFIGURING TRIGGER')

    if CHOSEN_TRIGGER == TriggerType.SOFTWARE:
        print('Software trigger chosen ...')
    elif CHOSEN_TRIGGER == TriggerType.HARDWARE:
        print('Hardware trigger chose ...')

    try:
        # Ensure trigger mode off
        # The trigger must be disabled in order to configure whether the source
        # is software or hardware.
        nodemap = cam.GetNodeMap()
        node_trigger_mode = PySpin.CEnumerationPtr(nodemap.GetNode('TriggerMode'))
        if not PySpin.IsAvailable(node_trigger_mode) or not PySpin.IsReadable(node_trigger_mode):
            print('Unable to disable trigger mode (node retrieval). Aborting...')
            return False

        node_trigger_mode_off = node_trigger_mode.GetEntryByName('Off')
        if not PySpin.IsAvailable(node_trigger_mode_off) or not PySpin.IsReadable(node_trigger_mode_off):
            print('Unable to disable trigger mode (enum entry retrieval). Aborting...')
            return False

        node_trigger_mode.SetIntValue(node_trigger_mode_off.GetValue())

        print('Trigger mode disabled...')

        # Select trigger source
        # The trigger source must be set to hardware or software while trigger
        # mode is off.
        node_trigger_source = PySpin.CEnumerationPtr(nodemap.GetNode('TriggerSource'))
        if not PySpin.IsAvailable(node_trigger_source) or not PySpin.IsWritable(node_trigger_source):
            print('Unable to get trigger source (node retrieval). Aborting...')
            return False

        if CHOSEN_TRIGGER == TriggerType.SOFTWARE:
            node_trigger_source_software = node_trigger_source.GetEntryByName('Software')
            if not PySpin.IsAvailable(node_trigger_source_software) or not PySpin.IsReadable(
                    node_trigger_source_software):
                print('Unable to set trigger source (enum entry retrieval). Aborting...')
                return False
            node_trigger_source.SetIntValue(node_trigger_source_software.GetValue())

        elif CHOSEN_TRIGGER == TriggerType.HARDWARE:
            node_trigger_source_hardware = node_trigger_source.GetEntryByName('Line3')
            if not PySpin.IsAvailable(node_trigger_source_hardware) or not PySpin.IsReadable(
                    node_trigger_source_hardware):
                print('Unable to set trigger source (enum entry retrieval). Aborting...')
                return False
            node_trigger_source.SetIntValue(node_trigger_source_hardware.GetValue())
        
            # check if this is correct
            cam.TriggerOverlap.SetValue(PySpin.TriggerOverlap_ReadOut)

        # Turn trigger mode on
        # Once the appropriate trigger source has been set, turn trigger mode
        # on in order to retrieve images using the trigger.
        node_trigger_mode_on = node_trigger_mode.GetEntryByName('On')
        if not PySpin.IsAvailable(node_trigger_mode_on) or not PySpin.IsReadable(node_trigger_mode_on):
            print('Unable to enable trigger mode (enum entry retrieval). Aborting...')
            return False

        node_trigger_mode.SetIntValue(node_trigger_mode_on.GetValue())
        print('Trigger mode turned back on...')

    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        return False

    return result


def reset_trigger(nodemap):
    """
    This function returns the camera to a normal state by turning off trigger mode.
  
    :param nodemap: Transport layer device nodemap.
    :type nodemap: INodeMap
    :returns: True if successful, False otherwise.
    :rtype: bool
    """
    try:
        result = True
        node_trigger_mode = PySpin.CEnumerationPtr(nodemap.GetNode('TriggerMode'))
        if not PySpin.IsAvailable(node_trigger_mode) or not PySpin.IsReadable(node_trigger_mode):
            print('Unable to disable trigger mode (node retrieval). Aborting...')
            return False

        node_trigger_mode_off = node_trigger_mode.GetEntryByName('Off')
        if not PySpin.IsAvailable(node_trigger_mode_off) or not PySpin.IsReadable(node_trigger_mode_off):
            print('Unable to disable trigger mode (enum entry retrieval). Aborting...')
            return False

        node_trigger_mode.SetIntValue(node_trigger_mode_off.GetValue())

        print('Trigger mode disabled...')

    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        result = False

    return result

def print_device_info(nodemap, cam_num):
    """
    This function prints the device information of the camera from the transport
    layer; please see NodeMapInfo example for more in-depth comments on printing
    device information from the nodemap.

    :param nodemap: Transport layer device nodemap.
    :param cam_num: Camera number.
    :type nodemap: INodeMap
    :type cam_num: int
    :returns: True if successful, False otherwise.
    :rtype: bool
    """
 
    print('\n*** DEVICE INFORMATION CAMERA %d ***\n' %cam_num)

    try:
        result = True
        node_device_information = PySpin.CCategoryPtr(nodemap.GetNode('DeviceInformation'))

        if PySpin.IsAvailable(node_device_information) and PySpin.IsReadable(node_device_information):
            features = node_device_information.GetFeatures()
            for feature in features:
                node_feature = PySpin.CValuePtr(feature)
                print('%s: %s' % (node_feature.GetName(),
                                  node_feature.ToString() if PySpin.IsReadable(node_feature) else 'Node not readable'))

        else:
            print('Device control information not available.')
        print()

    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        return False

    return result


def setAcquisitionModeContinuous(cam, i):
    
    print('Acquisition Mode Settings')
    # Set acquisition mode to continuous
    node_acquisition_mode = PySpin.CEnumerationPtr(cam.GetNodeMap().GetNode('AcquisitionMode'))
    if not PySpin.IsAvailable(node_acquisition_mode) or not PySpin.IsWritable(node_acquisition_mode):
        print('Unable to set acquisition mode to continuous (node retrieval; camera %d). Aborting... \n' % i)
        return False

    node_acquisition_mode_continuous = node_acquisition_mode.GetEntryByName('Continuous')
    if not PySpin.IsAvailable(node_acquisition_mode_continuous) or not PySpin.IsReadable(
            node_acquisition_mode_continuous):
        print('Unable to set acquisition mode to continuous (entry \'continuous\' retrieval %d). \
              Aborting... \n' % i)
        return False

    acquisition_mode_continuous = node_acquisition_mode_continuous.GetValue()
            
    node_acquisition_mode.SetIntValue(acquisition_mode_continuous)
            
    print('Camera %d acquisition mode set to continuous...' % i)
            
    
def setFrameRate(cam):
    
    print('Framerate Settings')
    # AcquisitionFrameRateAuto to Off  
    node_acquisitionFrameRateAuto = PySpin.CEnumerationPtr(cam.GetNodeMap().GetNode('AcquisitionFrameRateAuto'))
    if not PySpin.IsAvailable(node_acquisitionFrameRateAuto) or not PySpin.IsWritable(node_acquisitionFrameRateAuto):
        print('Unable to set node_acquisitionFrameRateAuto to Off  (enum retrieval). Aborting...\n')
        return False

    # Retrieve entry node from enumeration node
    node_acquisitionFrameRateAutoOff = node_acquisitionFrameRateAuto.GetEntryByName('Off')
    if not PySpin.IsAvailable(node_acquisitionFrameRateAutoOff) or not PySpin.IsReadable(node_acquisitionFrameRateAutoOff):
        print('Unable to get   node_acquisitionFrameRateAutoOff  (entry retrieval). Aborting...\n')
        return False

    # Retrieve integer value from entry node
    acquisitionFrameRateAutoOff = node_acquisitionFrameRateAutoOff.GetValue()

    # Set integer value from entry node as new value of enumeration node
    node_acquisitionFrameRateAuto.SetIntValue(acquisitionFrameRateAutoOff)
    print('Acquisition Frame Rate Auto set to Off...')

    # then the AcquisitionFrameRate will be writeable 
    node_AcquisitionFrameRate= PySpin.CFloatPtr(cam.GetNodeMap().GetNode('AcquisitionFrameRate'))
    if not PySpin.IsAvailable(node_AcquisitionFrameRate) or not PySpin.IsWritable(node_AcquisitionFrameRate):
        print('Unable to set  AcquisitionFrameRate  (enum retrieval). Aborting...')
        return False 
    node_AcquisitionFrameRate.SetValue(200);

    print('Acquisition Frame Rate set to : ', node_AcquisitionFrameRate.GetValue())
        
def setBufferHandling(cam):
    
    print('Buffer Settings')
    # Retrieve Stream Parameters device nodemap
    s_node_map = cam.GetTLStreamNodeMap()
    # Set the buffer handling mode to oldest first
    handling_mode = PySpin.CEnumerationPtr(s_node_map.GetNode('StreamBufferHandlingMode'))
    if not PySpin.IsAvailable(handling_mode) or not PySpin.IsWritable(handling_mode):
        print('Unable to set Buffer Handling mode (node retrieval).')
    else:
        handling_mode_entry = handling_mode.GetEntryByName('OldestFirst')
        handling_mode.SetIntValue(handling_mode_entry.GetValue())
        print('Buffer Handling Mode has been set to %s' % handling_mode_entry.GetDisplayName())

        #Check if the stream buffer count can be set to NUM_BUFFERS
        max_buffer_count = PySpin.CIntegerPtr(s_node_map.GetNode('StreamBufferCountMax'))
        if not PySpin.IsAvailable(max_buffer_count) or not PySpin.IsReadable(max_buffer_count):
            print('Unable to check max buffer count')
        else:
            if max_buffer_count.GetValue() < NUM_BUFFERS:
                print('Trying to use %d buffers, max is %d ' % (NUM_BUFFERS ,max_buffer_count.GetValue()))
            else:
                # Set stream buffer count mode to manual
                stream_buffer_count_mode = PySpin.CEnumerationPtr(s_node_map.GetNode('StreamBufferCountMode'))
                if not PySpin.IsAvailable(stream_buffer_count_mode) or not PySpin.IsWritable(stream_buffer_count_mode):
                    print('Unable to set buffer count mode')
                else:
                    stream_buffer_count_manual = stream_buffer_count_mode.GetEntryByName('Manual')
                    if not PySpin.IsAvailable(stream_buffer_count_manual) or not PySpin.IsReadable(stream_buffer_count_manual):
                        print('Unable to set buffer count mode')
                    else:
                        stream_buffer_count_mode.SetIntValue(stream_buffer_count_manual.GetValue())

                # set the buffer count to sepcified number of buffers
                buffer_count = PySpin.CIntegerPtr(s_node_map.GetNode('StreamBufferCountManual'))
                if not PySpin.IsAvailable(buffer_count) or not PySpin.IsWritable(buffer_count):
                    print('Unable to set buffer count')
                else:
                    print('buffer count set to %d' % NUM_BUFFERS)
                    buffer_count.SetValue(NUM_BUFFERS)
    
    
def setExposureTime(cam):
    try: 
        result = True
        print('Exposure Settings')
        #Step 1: Set Exposure Auto to Off (then the node Exposure Time will bekome Writable )
        node_ExposureAuto= PySpin.CEnumerationPtr(cam.GetNodeMap().GetNode('ExposureAuto'))
        
        if PySpin.IsAvailable(node_ExposureAuto) and PySpin.IsWritable(node_ExposureAuto):
            print('Current Value for Exposure Auto: ', node_ExposureAuto.GetCurrentEntry().GetDisplayName())
                
        node_ExposureAutoOff= node_ExposureAuto.GetEntryByName('Off')
        if not PySpin.IsAvailable(node_ExposureAutoOff) or not PySpin.IsReadable(node_ExposureAutoOff):
            print('Unable to set Exposure Mode  to Off (entry retrieval). Aborting...')
            return False
        # Retrieve integer value from entry node
        exposure_autoOff = node_ExposureAutoOff.GetValue()
        
        node_ExposureAuto.SetIntValue(exposure_autoOff)
        print('Current Value for Exposure Auto: ', node_ExposureAuto.GetCurrentEntry().GetDisplayName())
        
        #Step 2: Set a value for the Exposure Time
        node_ExposureTime=PySpin.CFloatPtr(cam.GetNodeMap().GetNode('ExposureTime'))
        if PySpin.IsAvailable(node_ExposureTime) and PySpin.IsWritable(node_ExposureTime):
            print('Current Value for Exposure Time: ', node_ExposureTime.GetValue())
        
        #print('Maximum Value: ', node_ExposureTime.GetMax())
        #print('minimum Value: ', node_ExposureTime.GetMin())
        #example to set a value between the min and max 
        #minim=node_ExposureTime.GetMin()
        node_ExposureTime.SetValue(4000) # 5000 us should generate ~200fps
        print('The Value for Exposure Time is set to: ', node_ExposureTime.GetValue())
        
    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        return False

    return result


def run_multiple_cameras(cam_list):
    """
    This function acts as the body of the example; please see NodeMapInfo example
    for more in-depth comments on setting up cameras.

    :param cam_list: List of cameras
    :type cam_list: CameraList
    :return: True if successful, False otherwise.
    :rtype: bool
    """
    try:
        result = True
        
        for i, cam in enumerate(cam_list):
            
            # Retrieve TL device nodemap
            nodemap_tldevice = cam.GetTLDeviceNodeMap()
           
            
            # Print device information
            result &= print_device_info(nodemap_tldevice, i)
            
            # Set Exposure Time
            setExposureTime(cam)
            
            # Set Acquisition Mode 
            setAcquisitionModeContinuous(cam, i)
            
            # Set Frame rate
            setFrameRate(cam)
            
            #setGain(cam, nodemap)
        
            #setPixelFormat(cam, nodemap)
            
            # Set Buffer Handling
            setBufferHandling(cam)
            
            cam.BeginAcquisition()
            fps = cam.AcquisitionFrameRate()
            
            print('Camera %d started acquisition...' % i)
            print('FrameRate: %d\n' %fps)
 
        # Ask for Filename metadata
        project = input("Specify PROJECT: ")
        session = input("Specify SESSION: ")
        animal = input("Specify ANIMAL: ") 
        filename = project + "_" + session + "_" + animal + "_"
        
        ## Ask for start recording
        input("Press ENTER to start recording")
        
    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        result = False

    return result


def avi_recording(cam_list, filename):
    ### here specs on recording
    timestamp = time.strftime("%Y%m%d_%H%M%S")

    filenameA = filename + timestamp + '_camA'
    filenameB = filename + timestamp + '_camB' 
    filenameC = filename + timestamp + '_camC' 
    filenameD = filename + timestamp + '_camD'
    filenameE = filename + timestamp + '_camE'
    filenameF = filename + timestamp + '_camF'
    
    for i, cam in enumerate(cam_list):
        ## Rename cameras here
        if i == 0:
            camA = cam
        elif i ==1:
            camB = cam
        elif i ==2:
            camC = cam
        elif i == 3:
            camD = cam
        elif i == 4:
            camE = cam
        elif i == 5:
            camF = cam
        else:
            pass
        
    avi_camA = PySpin.SpinVideo()
    avi_camB = PySpin.SpinVideo()
    avi_camC = PySpin.SpinVideo()
    avi_camD = PySpin.SpinVideo()
    avi_camE = PySpin.SpinVideo()
    avi_camF = PySpin.SpinVideo()
        
    option = PySpin.MJPGOption()
    fps = camA.AcquisitionFrameRate()
    option.frameRate = fps
    option.quality = 50
    
    avi_camA.Open(filenameA, option)
    avi_camB.Open(filenameB, option)
    avi_camC.Open(filenameC, option)
    avi_camD.Open(filenameD, option)
    avi_camE.Open(filenameE, option)
    avi_camF.Open(filenameF, option)

    try:
        frame = 0
        while True:
            strttime = time.time()

            frameA = camA.GetNextImage()
            frameB = camB.GetNextImage()
            frameC = camC.GetNextImage()
            frameD = camD.GetNextImage()
            frameE = camE.GetNextImage()
            frameF = camF.GetNextImage()
            
            avi_camA.Append(frameA)
            avi_camB.Append(frameB)
            avi_camC.Append(frameC)
            avi_camD.Append(frameD)
            avi_camE.Append(frameE)
            avi_camF.Append(frameF)
                
            frameA.Release()
            frameB.Release()
            frameC.Release()
            frameD.Release()
            frameE.Release()
            frameF.Release()
            
            endtime = time.time()
            t = round(endtime-strttime)
            rfps = 1/t
            print('time in loop Frame %d: %f' %(frame,t))
            print('resulting FrameRate: %d FPS' %rfps)
            frame += 1

    except KeyboardInterrupt:
        print("Press Ctrl-C to terminate while statement")
        
    avi_camA.Close()
    avi_camB.Close()
    avi_camC.Close()
    avi_camD.Close()
    avi_camE.Close()
    avi_camF.Close()
    return

async def acquire_images(queue: asyncio.Queue, cam: PySpin.Camera):
    """
    A coroutine that captures `NUM_IMAGES` images from `cam` and puts them along
    with the camera serial number as a tuple into the `queue`.
    """
    # Set up camera
    cam_id = cam.GetUniqueID()

    # Acquisition loop
    try:
        while True:
            img = cam.GetNextImage()
            frame_ID = img.GetFrameID()
            queue.put_nowait((img,cam_id))
            print('Queue size:', queue.qsize())
            print('[{}] Acquired image {}'.format(cam_id, frame_ID))
            await asyncio.sleep(0)  # This is necessary for context switches
    except KeyboardInterrupt:
        print("Pressed Ctrl-C to terminate while loop")
        
    # Clean up
    await queue.join()  # Wait for all images to be saved before EndAcquisition
    cam.EndAcquisition()
    cam.DeInit()
    del cam
    
    
async def save_images(queue: asyncio.Queue, save_dirs: dict, ext='.png'):
    """
    A coroutine that gets images from the `queue` and saves
    them using the global Thread Pool Executor.
    The save paths per camera are determined by `save_dirs` and the file
    extension is determined by the `ext` paramenter.
    `save_dirs` is a dict where the keys are the camera serial numbers
    and the values are the directory to save to.
    Once the image is saved, it is implicitly released and the task
    is marked as done in the queue.
    """
    try:
        while True:
            # Receive image
            image, cam_id = await queue.get()
            # Create filename
            frame_id = image.GetFrameID()
            filename = cam_id + '_' + str(frame_id) + ext
            filename = os.path.join(save_dirs[cam_id], filename)
            # Save the image using a pool of threads
            await loop.run_in_executor(tpe, save_image, image, filename)
            queue.task_done()
            print('[{}] Saved image {}'.format(cam_id, filename))
            
    except KeyboardInterrupt:
        print("Pressed Ctrl-C to terminate while loop")


def save_image(image: PySpin.Image, filename: str):
    """
    Saves the given `image` under the given `filename`.
    """
    # Notice how CPU time is minimized and I/O time is maximized
    image.Save(filename)

async def main():
    # Set up cam_list and queue
    system = PySpin.System.GetInstance()
    cam_list = system.GetCameras()
    queue = asyncio.Queue()

    # Initialize each camera
    for i, cam in enumerate(cam_list):
        cam.Init()
        
    # Match serial numbers to save locations
    assert len(cam_list) <= len(SAVE_DIRS), 'More cameras than save directories'
    camera_sns = [cam.GetUniqueID() for cam in cam_list]
    save_dir_per_cam = dict(zip(camera_sns, SAVE_DIRS))
    
    # PRIMARY CAMERA ID
    primary = 'USB\\VID_1E10&PID_4000&MI_00\\9&24BAA53&0&0000'
    
    # set trigger settings
    for i, cam in enumerate(cam_list):
        if camera_sns[i] == primary:
            # Set up primary camera trigger
            configure_strobe(cam)
        else:
            configure_trigger(cam)
            
    run_multiple_cameras(cam_list) # settings, no recording
    
    try:
        # Start the acquisition and save coroutines
        acquisition = [asyncio.gather(acquire_images(queue, cam)) for cam in cam_list]
        savers = asyncio.gather(save_images(queue, save_dir_per_cam)) #for _ in range(NUM_SAVERS)]

    except KeyboardInterrupt:
        print("Pressed Ctrl-C to terminate while loop")
        
    # Wait for all images to be captured and saved
    await asyncio.gather(*acquisition)
    print('Acquisition complete.')

    # Cancel the now idle savers
    for c in savers:
        c.cancel()

    # Clean up
    cam_list.Clear()
    system.ReleaseInstance()

# The event loop and Thread Pool Executor are global for convenience.
loop = asyncio.get_event_loop()
tpe = ThreadPoolExecutor(None)
loop.run_until_complete(main())
