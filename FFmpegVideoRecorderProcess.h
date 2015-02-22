#pragma once

#include <vector>
#include <string>

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
* It will check ffmpeg command is available (or otherwise this class will warn and do nothing) by checking the path.
* You can also specify an additional path env to the app to help to find ffmpeg. 
* It will create a video path file name according to the given path, the baseName and the automatic increment number.
* It wil check if the output path file name do not already exist before starting to write, and auto increment number name to avoid overwriting it.
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
	enum class PRESET 
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
	class Private;
	Private* d;

protected:
	// internal utilities

	/// Return a list of available values corresponding to the given environnement variable (Windows/Linux handled)
	std::vector<std::string> getEnvVar(std::string var);

	/// Add a list of values corresponding to the given environnement variable (Windows/Linux handled)
	/// Remove duplicated values (from input values) of a same envVar.
	/// this only affects the environment variable of the current process (The command processor's environment is not changed)
	void putEnvVar(std::string var, std::vector<std::string> values);

	/// Before starting, we should check the system can call ffmpeg programm or not
	bool checkFFmpegFound(unsigned int& outNbFound, bool verbose = false, bool tryOpen = false);
	
	/// Will generate a video filename based on mBaseName and mId
	std::string formatFileName(bool increment = true);

	/// Check if resolution changed
	/// The resolution sizes need to be divisible by 2 in order to use   '-pix_fmt yuv420p'   option with ffmpeg
	bool resolutionCheck(int width, int height);

public:
    // constructor/destructor
    FFmpegVideoRecorderProcess(std::string path = "./");
    virtual ~FFmpegVideoRecorderProcess();

	/// append PATH environnement variable (to find ffmpeg executable)
    void appendEnvVarPath(std::string envVarPath);
    
	/// append PATH environnement variable (to find ffmpeg executable)
    void appendEnvVarPath(std::vector<std::string> envVarPathList);

public:
	// set and get flags options

	/// change the default path where video file(s) will be generated (will auto create path if necessary)
	void setOutputPath(std::string path);

	/// get the default path where video file(s) will be generated
	std::string getOutputPath();

	/// change the default base name used to create videos (it will be suffix by xx number of video, overwriting already existing ones)
	void setOutputBaseFileName(std::string base);

	/// get the default file name where video file(s) will be generated
	std::string getOutputFileName();

	/// get the outputed file path name video
	std::string getOutputVideoFilePath();
	
	/// Overwrite existing output file is exist if true [default]. Or exit directly ffmpeg process.
	void overwriteOutput(bool overwrite);
	
	/// Do ffmpeg will overwrite existing output file is exist or exit directly ffmpeg process.
	bool overwriteOutput();
	
	/// Change the encode quality. 
	/// crf => Set the Constant Rate Factor (http://slhck.info/articles/crf) : Quality-based VBR. Range values are [0:lossless ; 51:worst] => default to 23
	/// The range is exponential, so increasing the CRF value +6 is roughly half the bitrate while -6 is roughly twice the bitrate
	void setQuality(unsigned int crf);
	
	/// Get the encode quality. Get the Constant Rate Factor (http://slhck.info/articles/crf)
	/// Range values are [0:lossless ; 51:worst] => default to 23
	/// The range is exponential, so increasing the CRF value +6 is roughly half the bitrate while -6 is roughly twice the bitrate
	unsigned int getQuality();

	/// The way to handle the resulted encoded video (FASTEST=>faster encoding : BEST_COMPRESSION very slow encoding)
	void setPreset(PRESET preset);

	/// The way to handle the resulted encoded video (FASTEST=>faster encoding : BEST_COMPRESSION very slow encoding)
	PRESET getPreset();
	
	/// Do not lost data information => induce biger result file size
	/// If set to true, the quality crf will not be used (for letting ffmpeg auto set it according to selected pixel format)
	void lossless(bool qp);

	/// Do not lost data information => induce biger result file size
	/// If set to true, the quality crf will not be used (for letting ffmpeg auto set it according to selected pixel format)
	bool lossless();
	
	/// Set bitrate options. Very useful for web - setting this to bitrate and 2x bitrate gives good results.
	/// If one of params is set to 0, the specific param will be not used
	/// Use it only if you know what you want to do => advanced user (otherwise do not call it and prefer the lossless or quality functions)
	void setBitrate(unsigned int bufsize = 2000, unsigned int maxrate = 1000, unsigned int minrate = 1000, unsigned int bitrate = 0, bool use = true);

	/// Set bitrate options. Very useful for web - setting this to bitrate and 2x bitrate gives good results.
	/// If one of params is set to 0, the specific param will be not used
	void setBitrate(unsigned int& bufsize, unsigned int& maxrate, unsigned int& minrate, unsigned int& bitrate, bool use);

	/// resume all plausible common params for quick setting in 1 function call
	void init(int width, int height, std::string outputPath, std::string baseFileName, bool overWriteFile, PRESET preset, unsigned int crfQuality);
	
public:
    // interfaces

	/// check ffmpeg, compute command line and start the process waiting for capturing
    virtual bool init();

	/// catch renderer opengl frame buffer and transmit to ffmpeg process
    virtual void capture(int width, int height, int x = 0, int y = 0);

	/// stop the video capture and delete buffer and close ffmpeg process
    virtual void finish();
};



/** The sinfleton version of FFmpegVideoRecorderProcess, to ease the use of it in a simple context
*
* Example:
* key::videoRecord ? 
*	FFmpegVideoRecorderProcessSingleton::Get()->init( width(), height(), path+"videos", appNameWE+"_", overwriteBool, PRESET::FASTEST_ENCODING, crf_quality_20 )
* :
*	FFmpegVideoRecorderProcessSingleton::Get()->finish();
*
* then in paintGL :
* if(videoRecord) FFmpegVideoRecorderProcessSingleton::Get()->capture( width(), height() );
*
*/
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
