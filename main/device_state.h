#ifndef _DEVICE_STATE_H_
#define _DEVICE_STATE_H_

enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateNotifying,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateAudioTesting,
    kDeviceStateRecording,
    kDeviceStateFatalError
};

// Application modes
enum AppMode {
    kModeChat = 0,    // Q&A mode (default) - press-to-talk via XiaoZhi protocol
    kModeRecording,   // Meeting recording mode - continuous PCM UDP streaming
};

#endif // _DEVICE_STATE_H_ 