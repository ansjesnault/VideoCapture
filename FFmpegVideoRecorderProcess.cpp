#include "FFmpegVideoRecorderProcess.h"

#include <iostream>
#include <sstream>
#include <fstream>  // extract sub string from long string PATH
#include <iomanip>	// setfill, setw ...
#include <algorithm>// std::for_each
#include <cstdio>	// FOPEN, FWRITE , FCLOSE ...
#include <cstdlib>	// PUTENV, GETENV ...
#include <numeric>	// accumulate string using operator +


#ifdef WIN32
#define OS_GET_ENV(VAR_C_STR_CHAR_STAR, OUT_CHAR_STAR) \
	size_t requiredSize; \
	getenv_s( &requiredSize, NULL, 0, VAR_C_STR_CHAR_STAR ); \
	if(requiredSize != 0)	{ \
		OUT_CHAR_STAR = new char[requiredSize];	\
		getenv_s( &requiredSize, OUT_CHAR_STAR, requiredSize, VAR_C_STR_CHAR_STAR ); \
	}
#define OS_PUT_ENV(VAR_C_STR_CHAR_STAR, VALUE_C_STR_CHAR_STAR) _putenv_s(VAR_C_STR_CHAR_STAR, VALUE_C_STR_CHAR_STAR);
#define OS_SEPARATOR		';'
#define OS_EXTENSION		".exe"
#define OS_POPEN(X,Y)		_popen(X,"wb")
#define OS_PCLOSE(X)		_pclose(X)
#define OS_FWRITE(X,Y,Z,W)	fwrite(X, Y, Z, W);
#define OS_MKDIR(X)			CreateDirectoryA(path.c_str(),NULL)
#else
#define OS_GET_ENV(VAR_C_STR_CHAR_STAR, OUT_CHAR_STAR)			OUT_CHAR_STAR = getenv(VAR_C_STR_CHAR_STAR);
#define OS_PUT_ENV(VAR_C_STR_CHAR_STAR, VALUE_C_STR_CHAR_STAR)  putenv( std::string(VALUE_C_STR_CHAR_STAR).insert(0, std::string(VAR_C_STR_CHAR_STAR)+"=").c_str() )
#define OS_SEPARATOR		':'
#define OS_EXTENSION		""
#define OS_POPEN(x,y)		popen(#X,"wb")
#define OS_PCLOSE(X)		pclose(X)
#define OS_FWRITE(X,Y,Z,W)	fwrite(X, Z, Y, W);
#define OS_MKDIR(X)			mkdir(X,S_IRUSR|S_IWUSR|S_IXUSR)
#endif

//===========================================================================================================

class FFmpegVideoRecorderProcess::Private
{
public:
	// needed for pipe creation and frame capture
	bool	mFound;		///< is the ffmpeg process found
	bool    mStarted;	///< is the ffmpeg process already started
	int*	mFramedata;	///< the frame buffer used to catch the frames from oprnGL renderer
	FILE*   mFFmpeg;	///< the file stream used to put frames buffers into ffmpeg process

	// needed for default ffmpeg cmd line creation
	std::string mPath;		///< the output video path (directory)
	std::string mBaseName;	///< the pattern used to create the output video file
	static int  mId;		///< the increasing number to complete the creation of the output video file
	int			mWidth;		///< the Width resolution for capture
	int			mHeight;	///< the height resolution for capture

	// ffmpeg customizable options
	bool		 mOverwrite;///< ffmpeg option to overwrite outputed video file if already exist (should never append since we check and increase mID number to avoid this)
	PRESET		 mPreset;	///< the ffmpeg preset use to auto handle encoding
	unsigned int mCRF;		///< ffmpeg option to set the quality [0:lossless - 51:worse] default 23 ->only applies to 8-bit x264 (yuv420p) and 10-bit x264 (yuv420p101e)
	bool		 mLossless; ///< ffmpeg option to encode without losing anything : -qp 0 (if set, will disable crf for auto ffmpeg efficiency)

	/** Forces libx264 to build video in a way, that it could be streamed over 500kbit/s line considering device buffer of 1000kbits.
	*	Very useful for web - setting this to bitrate and 2x bitrate gives good results. */
	struct Bitrate
	{
		bool use;			  ///< induce use 3 bitrate ffmpeg parameters
		unsigned int bitrate; ///< in kbits
		unsigned int minrate; ///< in kbits
		unsigned int maxrate; ///< in kbits
		unsigned int bufsize; ///< in kbits
	};
	Bitrate mBitrate;

	Private(std::string path)
		: mPath( path.at(path.length()-1) != '/' ? path.append("/") : path ) 
		, mFFmpeg(nullptr),			mFramedata(nullptr),	mStarted(false),	mFound(true)// because we willl check it after for the first init
		, mBaseName("ibr_video_"),	mWidth(800),			mHeight(600)
		, mCRF(23),					mLossless(false),		mOverwrite(true),	mPreset(PRESET::BALANCED)
		//, mBitrate({false,0,0,0,0}) // c++11 initialization list (only for MSVC12 and latter version)
	{
		// init the bitrate structure
		mBitrate.use	 = false;
		mBitrate.bitrate = 0;
		mBitrate.minrate = 0;
		mBitrate.maxrate = 0;
		mBitrate.bufsize = 0;
	}
};
int FFmpegVideoRecorderProcess::Private::mId = 0;


//===========================================================================================================


FFmpegVideoRecorderProcess::FFmpegVideoRecorderProcess(std::string path) 
	: d(new Private(path))
{
	GLFunctions::init();
}

FFmpegVideoRecorderProcess::~FFmpegVideoRecorderProcess()
{
	finish();
}

//------------------------------------------------------------------------------------------------------------
//---------------------------- utilities functions ----------------------------------------------------
//------------------------------------------------------------------------------------------------------------

std::vector<std::string> FFmpegVideoRecorderProcess::getEnvVar(std::string var)
{
	// get a long dtring from OS env var
	std::string currentPathEnvVarValue;
	char* envVarValue = nullptr;
	OS_GET_ENV(var.c_str(), envVarValue);
	if( envVarValue != nullptr  )
		currentPathEnvVarValue = std::string(envVarValue);

	// reformat long string into list of values (using OS separator)
	std::vector<std::string> result;
	std::string val;
	std::stringstream pathList(currentPathEnvVarValue);
	while( std::getline(pathList, val, OS_SEPARATOR) )
		result.push_back(val);

	return result;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::putEnvVar(std::string var, std::vector<std::string> values)
{
	if(values.size() <= 0) return;

	// remove duplicates and merge string values with right separator
	// use c++11 lamdba function capturing any scoped referenced variables by reference  
	std::vector<std::string> shortListedValues;
	std::vector<std::string> alreadyValList = getEnvVar(var);
	std::for_each( alreadyValList.begin(), alreadyValList.end(), [&](std::string valRef) 
	{
		if(std::find( std::begin(shortListedValues), std::end(shortListedValues), valRef) == std::end(shortListedValues) ) 
		{
			shortListedValues.push_back( valRef );
			shortListedValues.push_back( std::string(1,OS_SEPARATOR) );
		}
	});
	std::for_each( values.begin(), values.end(), [&](std::string val)
	{	
		if(std::find( std::begin(shortListedValues), std::end(shortListedValues), val) == std::end(shortListedValues) ) 
		{
			shortListedValues.push_back( val );
			shortListedValues.push_back( std::string(1,OS_SEPARATOR) );
		}
	});
	shortListedValues.pop_back(); // remove last separator (not needed)
	std::string unifiedValue = std::accumulate(std::begin(shortListedValues), std::end(shortListedValues), std::string(""));
	OS_PUT_ENV(var.c_str(), unifiedValue.c_str());
}

//------------------------------------------------------------------------------------------------------------

bool FFmpegVideoRecorderProcess::checkFFmpegFound(unsigned int& outNbFound, bool verbose, bool tryOpen)
{
	outNbFound = 0;

	// check to call it directly
	if(tryOpen)
	{
		if(verbose) std::cout<<"[FFmpegVideoRecorderProcess] Checking ffmpeg cmd..."<<std::endl;
		FILE *ffmpeg = nullptr;
		ffmpeg = OS_POPEN("ffmpeg");
		if(!ffmpeg || ffmpeg == nullptr || ffmpeg == NULL)
		{
			if(verbose) std::cout<<"[FFmpegVideoRecorderProcess] Command FFmpeg seems to failed."<<std::endl;
		}
		else
		{
			if(verbose) std::cout<<"[FFmpegVideoRecorderProcess] Command FFmpeg seems to be found."<<std::endl;
			OS_PCLOSE(ffmpeg);
			ffmpeg = nullptr;
		}
	}

	// try to parse PATH env var to get more details
	if(verbose) std::cout<<"[FFmpegVideoRecorderProcess] Checking path env..."<<std::endl;
	std::vector<std::string> paths = getEnvVar("PATH");
	for(std::string path : paths)
	{
		std::string		ffmpegFilePath( path + "/ffmpeg" + OS_EXTENSION );
		std::ifstream f(ffmpegFilePath.c_str(), std::ios::binary);			
		if( f.is_open() )
		{
			if(verbose) std::cout<<"[FFmpegVideoRecorderProcess] FOUND :"<< ffmpegFilePath << "\n";
			outNbFound++;
		}
		else if(verbose) 
			std::cout<<"[FFmpegVideoRecorderProcess] NOT FOUND in : "<< path << "\n";
		f.close();
	}
	std::cout<<std::endl;
	return outNbFound ? true : false;
}

//------------------------------------------------------------------------------------------------------------

std::string FFmpegVideoRecorderProcess::formatFileName(bool increment)
{
	std::stringstream fileName;
	fileName << d->mBaseName << std::setfill('0') << std::setw(2) << (increment ? ++d->mId : d->mId) <<".mp4";
	return fileName.str();
}

//------------------------------------------------------------------------------------------------------------

bool FFmpegVideoRecorderProcess::resolutionCheck(int width, int height)
{
	if( (width % 2 != 0 && d->mWidth != width-1 ) || (width % 2 == 0 && d->mWidth != width) )			 // width changed
		d->mWidth = width % 2 != 0 ? width - 1 : width;
	else if( (height % 2 != 0 && d->mHeight != height-1 ) || (height % 2 == 0 && d->mHeight != height) ) // height changed
		d->mHeight = height % 2 != 0 ? height - 1 : height;
	else
		return false;
	return true;
}

//------------------------------------------------------------------------------------------------------------
//---------------------------- set/get ffmpeg options ----------------------------------------------------
//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::appendEnvVarPath(std::string envVarPath)
{
	if(envVarPath.empty()) return;
	std::vector<std::string> envVarPathList;
	envVarPathList.push_back(envVarPath);
	putEnvVar("PATH", envVarPathList);
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::appendEnvVarPath(std::vector<std::string> envVarPathList)
{
	putEnvVar("PATH", envVarPathList);
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::setOutputPath(std::string path)
{
	if(path.empty())
	{
		std::cerr<<"[FFmpegVideoRecorderProcess] empty path given at : " << __FILE__ << ":" << __LINE__ << std::endl;
		path = "./";
		std::cerr<<"[FFmpegVideoRecorderProcess] set default path to "<< path << std::endl;			
	}
	else
	{
		OS_MKDIR(path.c_str());
	}
	d->mPath = path.at(path.length()-1) != '/' ? path.append("/") : path;
}

//------------------------------------------------------------------------------------------------------------

std::string FFmpegVideoRecorderProcess::getOutputPath()
{
	return d->mPath;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::setOutputBaseFileName(std::string base)
{
	d->mBaseName = base;
}

//------------------------------------------------------------------------------------------------------------

std::string FFmpegVideoRecorderProcess::getOutputFileName()
{
	return formatFileName(false);
}

//------------------------------------------------------------------------------------------------------------

std::string FFmpegVideoRecorderProcess::getOutputVideoFilePath()
{
	return d->mPath + formatFileName(false);
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::overwriteOutput(bool overwrite)
{
	d->mOverwrite = overwrite;
}

//------------------------------------------------------------------------------------------------------------

bool FFmpegVideoRecorderProcess::overwriteOutput()
{
	return d->mOverwrite;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::setQuality(unsigned int crf)
{
	d->mCRF = crf < 0 ? 0 : (crf > 51 ? 51 : crf);
}

//------------------------------------------------------------------------------------------------------------

unsigned int FFmpegVideoRecorderProcess::getQuality()
{
	return d->mCRF;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::setPreset(FFmpegVideoRecorderProcess::PRESET preset)
{
	d->mPreset = preset;
}

//------------------------------------------------------------------------------------------------------------

FFmpegVideoRecorderProcess::PRESET FFmpegVideoRecorderProcess::getPreset()
{
	return d->mPreset;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::lossless(bool qp)
{
	d->mLossless = qp;
}

//------------------------------------------------------------------------------------------------------------

bool FFmpegVideoRecorderProcess::lossless()
{
	return d->mLossless;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::setBitrate(unsigned int bufsize, unsigned int maxrate, unsigned int minrate, unsigned int bitrate, bool use)
{
	d->mBitrate.bitrate = bitrate;
	d->mBitrate.minrate = minrate;
	d->mBitrate.maxrate = maxrate;
	d->mBitrate.bufsize = bufsize;
	d->mBitrate.use	 = use;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::setBitrate(unsigned int& bufsize, unsigned int& maxrate, unsigned int& minrate, unsigned int& bitrate, bool use)
{
	bitrate = d->mBitrate.bitrate;
	minrate = d->mBitrate.minrate;
	maxrate = d->mBitrate.maxrate;
	bufsize = d->mBitrate.bufsize;
	use		= d->mBitrate.use;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::init(int width, int height, std::string outputPath, std::string baseFileName, bool overWriteFile, FFmpegVideoRecorderProcess::PRESET preset, unsigned int crfQuality)
{
	resolutionCheck(width, height);
	setOutputPath(outputPath);
	setOutputBaseFileName(baseFileName);
	overwriteOutput(overWriteFile);
	setPreset(preset);
	setQuality(crfQuality);
	init();
}


//------------------------------------------------------------------------------------------------------------
//---------------------------- MAIN VIRTUAL FUNCTIONS ----------------------------------------------------
//------------------------------------------------------------------------------------------------------------


bool FFmpegVideoRecorderProcess::init()
{
	if(d->mStarted || !d->mFound) return false;

	// Check system can find the ffmpeg cmd
	unsigned int nbFFmpegFound	= 0;
	d->mFound = checkFFmpegFound(nbFFmpegFound);
	if( 0 == nbFFmpegFound || nbFFmpegFound > 1 )
	{
		if(!checkFFmpegFound(nbFFmpegFound, true)) // display list of research path with status (verbosity info)
		{
			std::cerr<<"[FFmpegVideoRecorderProcess] FFMPEG NOT FOUND, AVOID THE VIDEO CAPTURE..."<<std::endl;
			return false;
		}
		else
		{
			std::cerr<<"[FFmpegVideoRecorderProcess] Too many version of FFmpeg found but continue..."<<std::endl;
		}
	}

	// apply the selected param preset
	std::string preset("fast"); // fast is the only preset not available in enum but valid
	switch ((int)d->mPreset)
	{
	case (int)PRESET::BALANCED:				preset = "medium ";		break;
	case (int)PRESET::BEST_COMPRESSION:		preset = "veryslow ";	break;
	case (int)PRESET::BETTER_COMPRESSION:	preset = "slow ";		break;
	case (int)PRESET::FASTEST_ENCODING:		preset = "ultrafast ";	break;
	case (int)PRESET::FASTER_ENCODING:		preset = "faster ";		break;
	case (int)PRESET::FAST_ENCODING:		preset = "superfast ";	break;
	default: break;
	}

	// apply the selected bitrate param (individually set)
	std::stringstream bitrateParam;
	bitrateParam	
		<< (d->mBitrate.use && d->mBitrate.bitrate ? ("-b:v "		+ d->mBitrate.bitrate+std::string("k ")) : std::string())
		<< (d->mBitrate.use && d->mBitrate.maxrate ? ("-maxrate "	+ d->mBitrate.maxrate+std::string("k ")) : std::string())
		<< (d->mBitrate.use && d->mBitrate.minrate ? ("-maxrate "	+ d->mBitrate.minrate+std::string("k ")) : std::string())
		<< (d->mBitrate.use && d->mBitrate.bufsize ? ("-bufsize "	+ d->mBitrate.bufsize+std::string("k ")) : std::string());

	// apply the selected lossless param
	std::stringstream lossless;
	if(d->mBitrate.use && d->mBitrate.bitrate) // if bitrate flag set to true and value < 0, no auto optimization quality is needed as bitrate fix it
		lossless << d->mLossless ? "-qp 0 " : ("-crf "+d->mCRF+std::string(" ")); //if real bool lossless, do not use crf param otherwise use it

	// create an non already existing output file path name video
	bool exist = false;
	std::string outFilePathName;
	do {
		outFilePathName = d->mPath + formatFileName();
		std::ifstream f(outFilePathName.c_str(), std::ios::binary);
		exist = f.is_open();
		f.close();
	}while(exist);


	// https://trac.ffmpeg.org/wiki/Encode/H.264
	// ffmpeg command line telling to expect raw rgba, reading frames from stdin
	std::stringstream cmd;
	cmd <<	"ffmpeg "
		// input options
			<<	"-s " << d->mWidth << "x" << d->mHeight << " "
			<<	"-framerate 25 -f rawvideo -vcodec rawvideo -pix_fmt rgba -i - "
		// output options
			<<  "-c:v libx264 "				// force the use of libx264 (due to best perf/quality ratio and some specific additional options we may need: crf)
			<<  "-threads 0 -vf vflip "		// threads 0 mean [auto detect]  and videoFlip verticaly
			<<  (d->mOverwrite ? "-y " : "-n ")// overwrite output file if exist or immediatly exit ffmpeg
			<<  "-preset " << preset
			<<  lossless.str()
			<<  bitrateParam.str()
			<<  "-pix_fmt yuv420p " //rgb24
			<<  outFilePathName;
	std::cout<<"[FFmpegVideoRecorderProcess] init : command called: "<< cmd.str() <<std::endl;

    // open pipe to ffmpeg's stdin in binary write mode
	d->mFFmpeg = OS_POPEN(cmd.str().c_str());
    d->mFramedata	= new int[d->mWidth*d->mHeight];
	std::cout<<"[FFmpegVideoRecorderProcess] START capturing video in : "<<getOutputVideoFilePath()<<std::endl;
	return d->mStarted	= true;
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::capture(int width, int height, int x, int y)
{
	// check if we need to auto stop to create another video due to the changed resolution
	if(d->mStarted && resolutionCheck(width, height))
		finish();

	if(!d->mStarted)
		init();

	if(d->mFFmpeg != nullptr)
	{
		glReadPixels(x, y, d->mWidth, d->mHeight, GL_RGBA, GL_UNSIGNED_BYTE, d->mFramedata);
		OS_FWRITE(d->mFramedata, sizeof(int)*d->mWidth*d->mHeight, 1, d->mFFmpeg);
	}
}

//------------------------------------------------------------------------------------------------------------

void FFmpegVideoRecorderProcess::finish()
{
	if(d->mFFmpeg != nullptr)
    {
		OS_PCLOSE(d->mFFmpeg);
		d->mFFmpeg = nullptr;
		std::cout<<"[FFmpegVideoRecorderProcess] FINISH, check video at : "<<getOutputVideoFilePath()<<std::endl;
    }

    if(d->mFramedata != nullptr || d->mFramedata != NULL)
    {
        delete [] d->mFramedata;
        d->mFramedata = nullptr;
    }

    if(d->mStarted)
        d->mStarted = false;
}

//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------

FFmpegVideoRecorderProcessSingleton* FFmpegVideoRecorderProcessSingleton::msInstance = nullptr;