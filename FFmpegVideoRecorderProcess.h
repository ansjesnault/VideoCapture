#pragma once

#include <iostream>
#include <string>
#include <stdio.h>

// in cpp
#include <sstream>
#include <iomanip> 

#ifdef HAS_QT
	#include <QtOpenGL>
	
	// Wrapper to Qt5 QOpenGLFunctions* 
	//  choosing the best inheritance class for you
	//  according to OPENGL_VERSION_* preprocessors
	#include "opengl_functions.h"
#else
	#ifdef WIN32
		#include <windows.h>
	#endif
	#include <GL/gl.h>
	// Emulate the class to be coherent with Qt5 opengl impl
	class GLFunctions { public: void init(){}; };
#endif


/**
* Pipe ffmpeg process asking to get our raw frame from our OpenGL renderer
* and use libx264 to output a video captue of the OpenGL scene.
* https://trac.ffmpeg.org/wiki/Encode/H.264
*
* 
*
* Advanced use for lossless quality :
*
* 1- lossless flag (-qp 0) which will exclude the use of CRF (for auto efficiency) => but may induce very big file size
* 2- Constant Rate Factor (CRF) quality => best recommandation
*
* For 1 and 2 : Each frame gets the bitrate it needs to keep the requested quality level.
* The downside is that you can't tell it to get a specific filesize or not go over a specific size or bitrate.
*
* 3- CRF with maximum bit rate, set a crf quality in addition to maxrate
* example : with crf quality = 20 , maxrate = 400 and bufsize = 1835
* This will "target" crf 20, but if the output exceeds 400kb/s, it will degrade to something less than crf 20 in that case. 
*
* 4- ABR (Average Bit Rate). Set manually the bitrate => not really recommanded to tweak this parameter
* 5- CBR (Constant Bit Rate). Set manually bitrate = minrate = maxrate (=4000 for example) 
* and bufsize (=1835 for example) is the "rate control buffer"
* so it will enforce your requested "average" (4000k in this case)
* across each 1835k worth of video. => assume you know what you are doing
*/
class FFmpegVideoRecorderProcess :  protected GLFunctions
{
public:
	/// How handle the resulted encoded video
	enum PRESET 
	{
		FASTEST_ENCODING,	///< -preset ultrafast
		FASTER_ENCODING,	///< -preset superfast
		FAST_ENCODING,		///< -preset faster
		BALANCED,			///< -preset medium
		BETTER_COMPRESSION,	///< -preset slow
		BEST_COMPRESSION	///< -preset veryslow
	};

private:
    // internal data

	// needed for pipe creation and frame capture
    FILE*   mFFmpeg;
    int*	mFramedata;
    bool    mStarted;

	// needed for default ffmpeg cmd line creation
	std::string mPath;
	std::string mBaseName;
	static int  mId;
	int			mWidth;
	int			mHeight;

	// ffmpeg customizable options
	bool		 mOverwrite;///< overwrite outputed video file
	PRESET		 mPreset;
	unsigned int mCRF;		///< quality [0:lossless - 51:worse] default 23 ->only applies to 8-bit x264 (yuv420p) and 10-bit x264 (yuv420p101e)
	bool		 mLossless; ///< -qp 0 (if set, will disable crf for auto ffmpeg efficiency)
	
	/** Forces libx264 to build video in a way, that it could be streamed over 500kbit/s line considering device buffer of 1000kbits.
	*	Very useful for web - setting this to bitrate and 2x bitrate gives good results.
	*/
	struct Bitrate
	{
		bool use;			  ///< induce use 3 bitrate ffmpeg parameters
		unsigned int bitrate; ///< in kbits
		unsigned int minrate; ///< in kbits
		unsigned int maxrate; ///< in kbits
		unsigned int bufsize; ///< in kbits
	};
	Bitrate mBitrate;
	

protected:
	/// Will generate a video filename based on mBaseName and mId
	std::string formatFileName(bool increment = true)
	{
		std::stringstream fileName;
		fileName << mBaseName << std::setfill('0') << std::setw(2) << (increment ? ++mId : mId) <<".mp4";
		return fileName.str();
	}

	/// Check if resolution changed
	/// The resolution sizes need to be divisible by 2 in order to use   '-pix_fmt yuv420p'   option with ffmpeg
	bool resolutionCheck(int width, int height)
	{
		if( (width % 2 != 0 && mWidth != width-1 ) || (width % 2 == 0 && mWidth != width) )			   // width changed
			mWidth = width % 2 != 0 ? width - 1 : width;
		else if( (height % 2 != 0 && mHeight != height-1 ) || (height % 2 == 0 && mHeight != height) ) // height changed
			mHeight = height % 2 != 0 ? height - 1 : height;
		else
			return false;
		return true;
	}

public:
    // constructor/destructor
    FFmpegVideoRecorderProcess(std::string path = "./")
		: mFFmpeg(nullptr), mFramedata(nullptr),	mStarted(false)
		, mPath(path),		mBaseName("ibr_video_"),mWidth(800),		mHeight(600)
		, mCRF(23),			mLossless(false),		mOverwrite(true),	mPreset(BALANCED)
    {
		GLFunctions::init();

		if (mPath.at(mPath.length()-1) != '/')
            mPath.append("/");

		mBitrate.bitrate = 0;
		mBitrate.minrate = 0;
		mBitrate.maxrate = 0;
		mBitrate.bufsize = 0;
		mBitrate.use	 = false;
    }
    virtual ~FFmpegVideoRecorderProcess()
    {
    }

public:
	// set and get flags options

	/// change the default path where video file(s) will be generated
	void setOutputPath(std::string path)
	{
		// TODO: handle the dynamic folder hierarchy creation if necessary
		mPath = path;
	}

	/// get the default path where video file(s) will be generated
	std::string getOutputPath()
	{
		return mPath;
	}

	/// change the default base name used to create videos (it will be suffix by xx number of video, overwriting already existing ones)
	void setOutputBaseFileName(std::string base)
	{
		mBaseName = base;
	}

	/// get the default file name where video file(s) will be generated
	std::string getOutputFileName()
	{
		return formatFileName(false);
	}

	/// get the outputed file path name video
	std::string getOutputVideoFilePath()
	{
		return mPath + formatFileName(false);
	}

    /// append PATH environnement variable (to find ffmpeg executable)
    void appendEnvVarPath(std::string envVarPath)
    {

    }

	/// Change the encode quality. 
	/// crf => Set the Constant Rate Factor (http://slhck.info/articles/crf) : Quality-based VBR. Range values are [0:lossless ; 51:worst] => default to 23
	/// The range is exponential, so increasing the CRF value +6 is roughly half the bitrate while -6 is roughly twice the bitrate
	void setQuality(unsigned int crf)
	{
		mCRF = crf < 1 ? 1 : (crf > 50 ? 50 : crf);
	}

	/// Get the encode quality. Get the Constant Rate Factor (http://slhck.info/articles/crf)
	/// Range values are [0:lossless ; 51:worst] => default to 23
	/// The range is exponential, so increasing the CRF value +6 is roughly half the bitrate while -6 is roughly twice the bitrate
	unsigned int getQuality()
	{
		return mCRF;
	}

	/// The way to handle the resulted encoded video (FASTEST=>faster encoding : BEST_COMPRESSION very slow encoding)
	void setPreset(PRESET preset)
	{
		mPreset = preset;
	}

	/// The way to handle the resulted encoded video (FASTEST=>faster encoding : BEST_COMPRESSION very slow encoding)
	PRESET getPreset()
	{
		return mPreset;
	}

	/// Do not lost data information => induce biger result file size
	/// If set to true, the quality crf will not be used (for letting ffmpeg auto set it according to selected pixel format)
	void lossless(bool qp)
	{
		mLossless = qp;
	}

	/// Do not lost data information => induce biger result file size
	/// If set to true, the quality crf will not be used (for letting ffmpeg auto set it according to selected pixel format)
	bool lossless()
	{
		return mLossless;
	}

	/// Overwrite existing output file is exist if true [default]. Or exit directly ffmpeg process.
	void overwriteOutput(bool overwrite)
	{
		mOverwrite = overwrite;
	}

	/// Do ffmpeg will overwrite existing output file is exist or exit directly ffmpeg process.
	bool overwriteOutput()
	{
		return mOverwrite;
	}

	/// Set bitrate options. Very useful for web - setting this to bitrate and 2x bitrate gives good results.
	/// If one of params is set to 0, the specific param will be not used
	void setBitrate(unsigned int bufsize = 2000, unsigned int maxrate = 1000, unsigned int minrate = 1000, unsigned int bitrate = 0, bool use = true)
	{
		mBitrate.bitrate = bitrate;
		mBitrate.minrate = minrate;
		mBitrate.maxrate = maxrate;
		mBitrate.bufsize = bufsize;
		mBitrate.use	 = use;
	}

	/// Set bitrate options. Very useful for web - setting this to bitrate and 2x bitrate gives good results.
	/// If one of params is set to 0, the specific param will be not used
	void setBitrate(unsigned int& bufsize, unsigned int& maxrate, unsigned int& minrate, unsigned int& bitrate, bool use)
	{
		bitrate = mBitrate.bitrate;
		minrate = mBitrate.minrate;
		maxrate = mBitrate.maxrate;
		bufsize = mBitrate.bufsize;
		use		= mBitrate.use;
	}

	/// resum all plausible common params for quick setting in 1 function call
	void init(int width, int height, std::string outputPath, std::string baseFileName, bool overWriteFile, PRESET preset, unsigned int crfQuality)
	{
		resolutionCheck(width, height);
		setOutputPath(outputPath);
		setOutputBaseFileName(baseFileName);
		overwriteOutput(overWriteFile);
		setPreset(preset);
		setQuality(crfQuality);
		init();
	}

public:
    // interfaces
    virtual void init()
    {
		if(mStarted) return;

		// apply the selected param preset
		std::string preset("fast"); // fast is the only preset not available in enum but valid
		switch ((int)mPreset)
		{
		case (int)PRESET::BALANCED:				preset = "medium";		break;
		case (int)PRESET::BEST_COMPRESSION:		preset = "veryslow";	break;
		case (int)PRESET::BETTER_COMPRESSION:	preset = "slow";		break;
		case (int)PRESET::FASTEST_ENCODING:		preset = "ultrafast";	break;
		case (int)PRESET::FASTER_ENCODING:		preset = "faster";		break;
		case (int)PRESET::FAST_ENCODING:		preset = "superfast";	break;
		default: break;
		}

		// apply the selected bitrate param (individually set)
		std::stringstream bitrateParam;
		bitrateParam	
			<< (mBitrate.use && mBitrate.bitrate ? ("-b:v "		+ mBitrate.bitrate+std::string("k ")) : std::string())
			<< (mBitrate.use && mBitrate.maxrate ? ("-maxrate "	+ mBitrate.maxrate+std::string("k ")) : std::string())
			<< (mBitrate.use && mBitrate.minrate ? ("-maxrate "	+ mBitrate.minrate+std::string("k ")) : std::string())
			<< (mBitrate.use && mBitrate.bufsize ? ("-bufsize "	+ mBitrate.bufsize+std::string("k ")) : std::string());

		// apply the selected lossless param
		std::stringstream lossless;
		if(mBitrate.use && mBitrate.bitrate) // if bitrate flag set to true and value < 0, no auto optimization quality is needed as bitrate fix it
			lossless << mLossless ? "-qp 0 " : ("-crf "+mCRF+std::string(" ")); //if real bool lossless, do not use crf param otherwise use it

		// https://trac.ffmpeg.org/wiki/Encode/H.264
		// ffmpeg command line telling to expect raw rgba, reading frames from stdin
		std::stringstream cmd;
		cmd <<	"ffmpeg "
			// input options
				<<	"-s " << mWidth << "x" << mHeight << " "
				<<	"-framerate 25 -f rawvideo -vcodec rawvideo -pix_fmt rgba -i - "
			// output options
			    <<  "-c:v libx264 "				// force the use of libx264 (due to best perf/quality ratio and some specific additional options we may need: crf)
				<<  "-threads 0 -vf vflip "		// threads 0 mean [auto detect]  and videoFlip verticaly
				<<  (mOverwrite ? "-y " : "-n ")// overwrite output file if exist or immediatly exit ffmpeg
				<<  "-preset " << preset << " "
				<<  lossless.str()
				<<  bitrateParam.str()
				<<  "-pix_fmt yuv420p " //rgb24
				<<  mPath + formatFileName();
		std::cout<<"[FFmpegVideoRecorderProcess] init : command called: "<< cmd.str() <<std::endl;

        // open pipe to ffmpeg's stdin in binary write mode
#ifdef WIN32
		mFFmpeg = _popen(cmd.str().c_str(), "wb");
#else
        mFFmpeg = popen(cmd.str().c_str(), "w");
#endif

        mFramedata	= new int[mWidth*mHeight];
        mStarted	= true;
		std::cout<<"[FFmpegVideoRecorderProcess] START capturing video in : "<<getOutputVideoFilePath()<<std::endl;
    }

    virtual void capture(int width, int height, int x = 0, int y = 0)
    {
		// check if we need to auto stop to create another video due to the changed resolution
		if(mStarted && resolutionCheck(width, height))
			finish();

		if(!mStarted)
			init();

        glReadPixels(x, y, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, mFramedata);
#ifdef WIN32
       fwrite(mFramedata, sizeof(int)*mWidth*mHeight, 1, mFFmpeg);
#else
       fwrite(mFramedata, 1, sizeof(int)*mWidth*mHeight, mFFmpeg); //TODO : TEST it!
#endif
    }

    virtual void finish()
    {
		std::cout<<"[FFmpegVideoRecorderProcess] finish"<<std::endl;

		if(mFFmpeg != nullptr)
        {
#ifdef WIN32
            _pclose(mFFmpeg);
#else
            pclose(mFFmpeg);
#endif
			mFFmpeg = nullptr;
        }

        if(mFramedata != nullptr || mFramedata != NULL)
        {
            delete [] mFramedata;
            mFramedata = nullptr;
        }

        if(mStarted)
            mStarted = false;

		std::cout<<"[FFmpegVideoRecorderProcess] FINISH, check video at : "<<getOutputVideoFilePath()<<std::endl;
    }
};


class FFmpegVideoRecorderProcessSingleton : public FFmpegVideoRecorderProcess
{
protected:
    static FFmpegVideoRecorderProcessSingleton* msInstance;

private:
    FFmpegVideoRecorderProcessSingleton() : FFmpegVideoRecorderProcess() {}
    virtual ~FFmpegVideoRecorderProcessSingleton() {}

public:
    static FFmpegVideoRecorderProcessSingleton* Get()
    {
		return msInstance == nullptr ? msInstance = new FFmpegVideoRecorderProcessSingleton() : msInstance;
    }

    static void Destroy()
    {
        if(msInstance)
        {
			msInstance->finish();
            delete msInstance;
            msInstance = nullptr;
        }
    }
};
