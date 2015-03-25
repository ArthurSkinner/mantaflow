#
# Helper functions independent of mantaflow 
# 

import os
import shutil


def outputFilename( file, gridname ):
	return file +"_"+ gridname + "_out.uni" 

# original, simpler...
def referenceFilename_old( file, gridname ):
	return file +"_"+ gridname + "_ref.uni" 

# new version, extract directory & basename...
def referenceFilename( file, gridname ):
	(name,ext) = os.path.splitext( os.path.basename(file) )
	ddir = dataDirectory(file)
	return ddir+"/"+ name +"_"+ gridname + ".uni" 

def dataDirectory( file ):
	# extract path from script call
	basename = os.path.basename(file)
	basedir  = os.path.dirname (file)
	if len(basedir)==0:
		basedir = "."
	return basedir +"/"+ "../testdata"


def getGenRefFileSetting( ):
	# check env var for generate data setting
	ret = int(os.getenv('MANTA_GEN_TEST_DATA', 0))
	# print("Gen-data-setting: " + str(ret))
	if(ret>0):
		return 1
	return 0

def getStrictSetting( ):
	# check env var whether strict mode enabled
	ret = int(os.getenv('MANTA_TEST_STRICT', 0))
	#print("Strict-test-setting: " + str(ret))
	if(ret>0):
		return 1
	return 0

# visual mode on? returns multiplier
def getVisualSetting( ):
	ret = int(os.getenv('MANTA_VISUAL', 0))
	if(ret>0):
		return ret
	return 0



