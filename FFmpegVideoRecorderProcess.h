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


//TODO: description + example usage
class FFmpegVideoRecorderProcess :  protected GLFunctions
{
private:
    // internal data
    FILE*   mFFmpeg;
    int*	mFramedata;
    bool    mStarted;

	std::string mPath;
	std::string mBaseName;
	std::string mName;
	int			mWidth;
	int			mHeight;
	static int  mId;

protected:
	std::string formatNewFileName()
	{
		std::stringstream fileName;
		fileName << mBaseName << std::setfill('0') << std::setw(2) << mId++ <<".mp4";
		return fileName.str();
	}

public:
    // constructor/destructor (use Qt or not)
    FFmpegVideoRecorderProcess(std::string path = "./")
        : mBaseName("ibr_video_")
		, mStarted(false),		mPath(path)
		, mWidth(800),			mHeight(600)
		, mFFmpeg(nullptr),		mFramedata(nullptr)
    {
		GLFunctions::init();

		if (mPath.at(mPath.length()-1) != '/')
            mPath.append("/");
		mName = formatNewFileName();
    }
    virtual ~FFmpegVideoRecorderProcess()
    {
    }

public:
	// set and get flags options

	// change the default path where video file(s) will be generated
	void setOutputPath(std::string path)
	{
		// TODO: handle the dynamic folder hierarchy creation if necessary
		mPath = path;
	}

	// get the default path where video file(s) will be generated
	std::string getOutputPath()
	{
		return mPath;
	}

	// change the default base name used to create videos (it will be suffix by xx number of video, overwriting already existing ones)
	void setOutputBaseFileName(std::string base)
	{
		mBaseName = base;
	}

	// get the default file name where video file(s) will be generated
	std::string getOutputFileName()
	{
		return mName;
	}

	// get the outputed file path name video
	std::string getOutputVideoFilePath()
	{
		return mPath + mName;
	}

    // append PATH environment variable (to find ffmpeg executable)
    void appendEnvVarPath(std::string envVarPath)
    {
		//TODO
    }

public:
    // interfaces
    virtual void init()
    {
		if(mStarted) return;

        //std::string cmd = "ffmpeg -r 25 -f h264 -pix_fmt rgba -s " + getWidthSize() + 'x' + getHeightSize() + std::string(" -i - -preset ultrafast -y " + getOutputVideoFile() );
        //const char* cmd = "ffmpeg -s 800x533 -framerate 25 -f rawvideo -vcodec rawvideo -pix_fmt rgba -i - "// -s 800x533
        //                  "-threads 0 -preset fast -y -vf vflip -pix_fmt yuv420p output.mp4";

		// ffmpeg command line telling to expect raw rgba, reading frames from stdin
		std::stringstream cmd;
		cmd <<	"ffmpeg "
			// input options
			<<	"-s " << mWidth << "x" << mHeight << " "
			<<	"-framerate 25 -f rawvideo -vcodec rawvideo -pix_fmt rgba -i - "
			// output options
			<<  "-threads 0 -preset fast -y -vf vflip -pix_fmt yuv420p "
			<<  mPath + formatNewFileName();
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
		// check if we need to auto stop to create another video with another due to the changed resolution

		bool resolutionChanged = true;

		// The resolution sizes need to be divisible by 2 in order to use   '-pix_fmt yuv420p'   option with ffmpeg
		if( (width % 2 != 0 && mWidth != width-1 ) || (width % 2 == 0 && mWidth != width) )			   // width changed
			mWidth = width % 2 != 0 ? width - 1 : width;
		else if( (height % 2 != 0 && mHeight != height-1 ) || (height % 2 == 0 && mHeight != height) ) // height changed
			mHeight = height % 2 != 0 ? height - 1 : height;
		else
			resolutionChanged = false;

		if(resolutionChanged)
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

//---------------------------------------------------------------------------------

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