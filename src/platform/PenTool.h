#pragma once

// Tells whether the eraser end of the Marker is the one on the screen.
// AppLoad forwards eraser strokes as ordinary pen events (its qtfb code leaves
// devId as 0 with a "differentiate between pen / eraser" TODO), so this asks
// the pen's own input device instead: the kernel reports the eraser end as the
// BTN_TOOL_RUBBER key. The device is opened read-only and not grabbed, so the
// rest of the system keeps receiving its events. On machines without such a
// device (desktop builds) available() is false and nothing is ever an eraser.
class PenTool {
public:
    PenTool();
    ~PenTool();
    PenTool(const PenTool &) = delete;
    PenTool &operator=(const PenTool &) = delete;

    bool available() const;
    // Whether the eraser end is in use right now.
    bool eraserActive() const;

    // Tests bit `key` in a Linux key-state bitmap (as from EVIOCGBIT/EVIOCGKEY).
    static bool keyBitSet(const unsigned char *bits, int bitsLength, int key);

private:
    int m_fd = -1;
};
