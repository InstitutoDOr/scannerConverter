import os;
import sys;
from shutil import copyfile;
from time import sleep;

TR=2.0;
nSlices = 40.0;

def copyFiles(fileList, dirInput, dirOutput):
    if not os.path.exists(dirOutput):   
       os.makedirs(dirOutput);
    
    for filename in fileList:
       copyfile('%s/%s' % (dirInput, filename), '%s/%s' % (dirOutput, filename));
       sleep(TR/nSlices);

if len(sys.argv) > 2:
   dirInput = sys.argv[1];
   dirOutput = sys.argv[2];
   fileList = os.listdir(dirInput);
   copyFiles(fileList, dirInput, dirOutput); 
