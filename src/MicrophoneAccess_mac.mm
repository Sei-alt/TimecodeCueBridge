#include "MicrophoneAccess.h"
#import <AVFoundation/AVFoundation.h>
namespace tcb {
void requestMicrophoneAccess(std::function<void(bool)> completion) {
    const auto status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (status == AVAuthorizationStatusNotDetermined) {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
            completion(granted == YES);
        }];
    } else {
        completion(status == AVAuthorizationStatusAuthorized);
    }
}
}
