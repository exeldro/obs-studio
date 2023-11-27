#include "system-info.hpp"

#import <Foundation/Foundation.h>

OBSDataArrayAutoRelease system_gpu_data()
{
    return nullptr;
}

OBSDataAutoRelease system_info()
{
    return nullptr;
}

std::string system_video_save_path()
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSURL *url = [fm URLForDirectory:NSMoviesDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:true
                               error:nil];

    if (!url)
        return getenv("HOME");

    return url.path.fileSystemRepresentation;
}
