#include <Arduino.h>
#include <cycle_array.h>
#include <vector.h>

constexpr int8_t shutterPin = 2;
constexpr int8_t focusPin = 3;
constexpr int8_t shutterButtonPin = 4;
constexpr int8_t startButtonPin = 5;
constexpr int8_t startedLedPin = 11;
constexpr int8_t shutterLedPin = 13;

constexpr int32_t delayBetweenShotsMs = 1000;

enum class State : uint8_t {
    NotInitialized,
    Idle,
    ShutterForced,
    TimerWorking
};

State state = State::NotInitialized;

struct Timestamp final {
    uint32_t endTime = 0;
    bool shutterOpen = false;

    Timestamp() = default;
    Timestamp(const uint32_t endTimeArg, const bool shutterOpenArg)
        : endTime(endTimeArg), shutterOpen(shutterOpenArg) {}
};

constexpr int8_t TimestampsMaxCount = 100;
uint32_t cycleStartTime = 0;
csso::array<Timestamp, TimestampsMaxCount> timestamps;

uint32_t currentTime = 0;

bool startButtonDown = false;
bool shutterButtonDown = false;
bool startButtonPressed = false;

void shutter(const bool isOpen) {
    static bool shutterWasOpen = false;
    static uint32_t lastShutterChangedTime = 0;

    if (isOpen != shutterWasOpen) {
        digitalWrite(shutterPin, isOpen == false);
        digitalWrite(shutterLedPin, isOpen == false);

        Serial.print((currentTime - lastShutterChangedTime) / 1000);
        lastShutterChangedTime = currentTime;

        Serial.print(" ");
        if (isOpen) {
            Serial.println("open shutter");
        } else {
            Serial.println("close shutter");
        }
    }

    shutterWasOpen = isOpen;
}

void addTimestamp(const Timestamp &timestamp) {
    uint32_t lastTimestampTime = 0;
    if (timestamps.count() > 0) {
        lastTimestampTime = timestamps.last().endTime;
    }

    timestamps.push_back(Timestamp{lastTimestampTime + timestamp.endTime,
                                   timestamp.shutterOpen});
}

bool shoudOpenShutter() {
    const uint32_t cycleTime = currentTime - cycleStartTime;

    for (int32_t i = 0; i < timestamps.count(); i++) {
        if (cycleTime < timestamps[i].endTime) {
            return timestamps[i].shutterOpen;
        }
    }

    cycleStartTime += timestamps.last().endTime;

    return shoudOpenShutter();
}

constexpr uint16_t filterSize = 20;

void filterState(const csso::cycle_array<bool, filterSize> &arr, bool &value) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < arr.count(); ++i) {
        if (arr[i]) {
            ++count;
        }
    }

    if (count < 5) {
        value = false;
    }
    if (count > filterSize - 5) {
        value = true;
    }
}

void updateButtons() {
    static bool startButtonWasDown = false;
    // static bool shutterButtonWasDown = false;

    static csso::cycle_array<bool, filterSize> startButtonDownArray(false);
    static csso::cycle_array<bool, filterSize> shutterButtonDownArray(false);

    const bool isStartButtonDown = digitalRead(startButtonPin);
    const bool isShutterButtonDown = digitalRead(shutterButtonPin);

    startButtonDownArray.push(isStartButtonDown);
    shutterButtonDownArray.push(isShutterButtonDown);

    filterState(startButtonDownArray, startButtonDown);
    filterState(shutterButtonDownArray, shutterButtonDown);

    startButtonPressed = startButtonDown && !startButtonWasDown;
    startButtonWasDown = startButtonDown;
}

void applyState(const State state) {
    switch (state) {
        case State::Idle:
            shutter(false);
            digitalWrite(startedLedPin, HIGH);
            return;

        case State::ShutterForced:
            shutter(true);
            digitalWrite(startedLedPin, HIGH);
            return;

        case State::TimerWorking:
            shutter(shoudOpenShutter());
            digitalWrite(startedLedPin, LOW);
            return;

        default:
            Serial.print("Unknown state: ");
            Serial.println(static_cast<uint8_t>(state));
            return;
    }
}

void changeState(const State newState) {
    if (state == newState) {
        Serial.println("ERROR: state == newState");
        return;
    }

    state = newState;
    cycleStartTime = currentTime;

    applyState(state);

    delay(100);

    Serial.print("Changed state to: ");
    Serial.println(static_cast<uint8_t>(state));
}

void setup() {
    currentTime = millis();

    pinMode(shutterPin, OUTPUT);
    pinMode(focusPin, OUTPUT);

    pinMode(startedLedPin, OUTPUT);
    pinMode(shutterLedPin, OUTPUT);

    pinMode(shutterButtonPin, INPUT);
    pinMode(startButtonPin, INPUT);

    digitalWrite(shutterLedPin, HIGH);
    digitalWrite(startedLedPin, HIGH);
    digitalWrite(focusPin, HIGH);
    shutter(false);

    addTimestamp({delayBetweenShotsMs, false});
    addTimestamp({1 * 1000, true});
    addTimestamp({delayBetweenShotsMs, false});
    addTimestamp({2 * 1000, true});
    addTimestamp({delayBetweenShotsMs, false});
    addTimestamp({3 * 1000, true});

    Serial.begin(9600);
    Serial.println("op!");

    changeState(State::Idle);
}

void loop() {
    updateButtons();

    currentTime = millis();

    applyState(state);

    switch (state) {
        case State::Idle:
            if (shutterButtonDown) {
                changeState(State::ShutterForced);
            } else if (startButtonPressed) {
                changeState(State::TimerWorking);
            }
            return;

        case State::ShutterForced:
            if (!shutterButtonDown) {
                changeState(State::Idle);
            }
            return;

        case State::TimerWorking:
            if (shutterButtonDown) {
                changeState(State::ShutterForced);
            } else if (startButtonPressed) {
                changeState(State::Idle);
            }

            return;

        default:
            Serial.print("Unknown state: ");
            Serial.println(static_cast<uint8_t>(state));
            return;
    }
}
