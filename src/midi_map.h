#ifndef Midi_Map_h
#define Midi_Map_h

/* Track */
enum ctrl_name {
        ARM_CC = 8,
        MUTE_CC = 12,
        SOLO_CC = 16,
        VOLUME_CC = 32,
        PAN_CC = 36,
        SEND_CC = 40,
        SEND_UP_CC = 104,
        SEND_DOWN_CC = 105,
        DEVICE_MODE_CC = 105,
        DEVICE_LEFT_CC = 106,
        DEVICE_RIGHT_CC = 107,
        TRACK_SELECT_CC = 64
};

uint8_t pin_to_midi[] = {
        ARM_CC,
        MUTE_CC,
        SOLO_CC,
        0,
        0,
        SOLO_CC + 1,
        MUTE_CC + 1,
        ARM_CC + 1
};

#endif /* Midi_Map_h */