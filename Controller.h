
struct Controller {
    // buttons are A, B, select, start, up, down, left, and right
    uint8_t liveButtonState = 0x00; // reading from main
    uint8_t activeButtonState = 0x00; // shift register
    int counter = 0; // button step, used to output ones after 7
    bool latch = false; // indicates when starting or finishing polling

    void UpdateLiveState(uint8_t state) {
        liveButtonState = state;
    }

    void UpdateLatch(uint8_t value) {
        bool newLatch = (value & 0x01);
        if (newLatch) { // keep reporting A. no shifting
            counter = 0;
        }
        else if (latch) { // start sequential reads, falling edge (1 => 0)
            activeButtonState = liveButtonState;
            counter = 0;
        }
        latch = newLatch;
    }

    uint8_t Fetch() {

        if (latch) { // poll A, no shift
            return liveButtonState & 0x01;
        }
        else {
            if (counter > 7) {
                return 0x01;
            }
            else  {
                uint8_t buttonState = activeButtonState & 0x01;
                activeButtonState >>= 1;
                counter++;
                return buttonState;
            }
        }
    }
};
