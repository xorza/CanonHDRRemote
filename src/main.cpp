#include <Arduino.h>
#include <cycle_array.h>
#include <vector.h>

constexpr int8_t shutterPin = 2;
constexpr int8_t focusPin = 3;
constexpr int8_t shutterButtonPin = 4;
constexpr int8_t startButtonPin = 5;

constexpr int8_t startedLedPin = 11;
constexpr int8_t idleLedPin = 12;
constexpr int8_t shutterLedPin = 13;

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
    digitalWrite(shutterPin, isOpen == false);
    digitalWrite(shutterLedPin, isOpen == false);

    static bool shutterWasOpen = false;
    if (isOpen != shutterWasOpen) {
        static uint32_t lastShutterChangedTime = 0;
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

void addTimestamp(const uint32_t timespanS, const bool shutterValue) {
    uint32_t lastTimestampTime = 0;
    if (timestamps.count() > 0) {
        lastTimestampTime = timestamps.last().endTime;
    }

    timestamps.push_back(
        Timestamp{lastTimestampTime + timespanS * 1000, shutterValue});
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

void applyState() {
    switch (state) {
        case State::Idle:
            shutter(false);
            digitalWrite(idleLedPin, LOW);
            digitalWrite(startedLedPin, HIGH);
            return;

        case State::ShutterForced:
            shutter(true);
            digitalWrite(idleLedPin, HIGH);
            digitalWrite(startedLedPin, HIGH);
            return;

        case State::TimerWorking:
            shutter(shoudOpenShutter());
            digitalWrite(idleLedPin, HIGH);
            digitalWrite(startedLedPin, LOW);
            return;

        default:
            shutter(false);
            digitalWrite(idleLedPin, HIGH);
            digitalWrite(startedLedPin, HIGH);
            return;
    }
}

void changeState(const State newState, const bool force = false) {
    if (state == newState && !force) {
        Serial.println("ERROR: state == newState");
        return;
    }

    state = newState;
    cycleStartTime = currentTime;

    applyState();

    delay(150);

    Serial.print("Changed state to: ");
    Serial.println(static_cast<uint8_t>(state));
}

void setup() {
    currentTime = millis();

    pinMode(shutterPin, OUTPUT);
    pinMode(focusPin, OUTPUT);

    pinMode(startedLedPin, OUTPUT);
    pinMode(shutterLedPin, OUTPUT);
    pinMode(idleLedPin, OUTPUT);

    pinMode(shutterButtonPin, INPUT);
    pinMode(startButtonPin, INPUT);

    digitalWrite(focusPin, HIGH);

    changeState(State::Idle, true);

    constexpr uint32_t delayBetweenShots = 1;
    addTimestamp(delayBetweenShots, false);
    addTimestamp(1, true);
    addTimestamp(delayBetweenShots, false);
    addTimestamp(2, true);
    addTimestamp(delayBetweenShots, false);
    addTimestamp(3, true);

    Serial.begin(9600);
    Serial.println("op!");
}

void loop() {
    currentTime = millis();

    updateButtons();
    applyState();

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
