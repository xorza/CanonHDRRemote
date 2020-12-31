#include <Arduino.h>

#include <vector.h>
#include <cycle_array.h>

constexpr int8_t shutterPin = 2;
constexpr int8_t focusPin = 3;
constexpr int8_t shutterButtonPin = 4;
constexpr int8_t startButtonPin = 5;
constexpr int8_t startedLedPin = 11;
constexpr int8_t shutterLedPin = 13;

constexpr int32_t delayBetweenShotsMs = 1000;

bool isStarted = false;

struct Timestamp final
{
  uint32_t endTime = 0;
  bool shutterOpen = false;

  Timestamp() = default;
  Timestamp(const uint32_t endTimeArg, const bool shutterOpenArg)
      : endTime(endTimeArg),
        shutterOpen(shutterOpenArg)
  {
  }
};

constexpr int8_t TimestampsMaxCount = 100;
uint32_t cycleStartTime = 0;
csso::array<Timestamp, TimestampsMaxCount> timestamps;

bool shutterWasOpen = false;

uint32_t currentTime = 0;

uint32_t lastShutterChangedTime = 0;

void shutter(const bool isOpen)
{
  if (isOpen != shutterWasOpen)
  {
    digitalWrite(shutterPin, isOpen == false);
    digitalWrite(shutterLedPin, isOpen == false);

    Serial.print((currentTime - lastShutterChangedTime) / 1000);
    lastShutterChangedTime = currentTime;

    Serial.print(" ");
    if (isOpen)
    {
      Serial.println("open shutter");
    }
    else
    {
      Serial.println("close shutter");
    }
  }

  shutterWasOpen = isOpen;
}

void addTimestamp(const Timestamp &timestamp)
{
  uint32_t lastTimestampTime = 0;
  if (timestamps.count() > 0)
  {
    lastTimestampTime = timestamps.last().endTime;
  }

  timestamps.push_back(Timestamp{
      lastTimestampTime + timestamp.endTime,
      timestamp.shutterOpen});
}

bool shoudOpenShutter()
{
  const uint32_t cycleTime = currentTime - cycleStartTime;

  for (int32_t i = 0; i < timestamps.count(); i++)
  {
    if (cycleTime < timestamps[i].endTime)
    {
      return timestamps[i].shutterOpen;
    }
  }

  cycleStartTime += timestamps.last().endTime;

  return shoudOpenShutter();
}

void startStop(const bool onlyStop = false)
{
  const bool wasStarted = isStarted;
  isStarted = !(isStarted || onlyStop);

  if (wasStarted == isStarted)
  {
    return;
  }

  digitalWrite(startedLedPin, !isStarted);

  if (isStarted)
  {
    cycleStartTime = currentTime;

    Serial.println("Starting");
  }
  else
  {
    Serial.println("Stopping");
  }
}

bool startButtonDown = false;
bool shutterButtonDown = false;
bool startButtonWasDown = false;
bool shutterButtonWasDown = false;

const uint16_t filterSize = 20;
csso::cycle_array<bool, filterSize> startButtonDownArray(false);
csso::cycle_array<bool, filterSize> shutterButtonDownArray(false);

void filterState(const csso::cycle_array<bool, filterSize> &arr, bool &value)
{
  uint16_t count = 0;
  for (uint16_t i = 0; i < arr.count(); ++i)
  {
    if (arr[i])
    {
      ++count;
    }
  }

  if (count < 5)
  {
    value = false;
  }
  if (count > filterSize - 5)
  {
    value = true;
  }
}

void updateButtons()
{
  const bool isStartButtonDown = digitalRead(startButtonPin);
  const bool isShutterButtonDown = digitalRead(shutterButtonPin);

  startButtonDownArray.push(isStartButtonDown);
  shutterButtonDownArray.push(isShutterButtonDown);

  filterState(startButtonDownArray, startButtonDown);
  filterState(shutterButtonDownArray, shutterButtonDown);
}

void setup()
{
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
  addTimestamp({2 * 1000, true});
  addTimestamp({delayBetweenShotsMs, false});
  addTimestamp({4 * 1000, true});
  addTimestamp({delayBetweenShotsMs, false});
  addTimestamp({6 * 1000, true});

  Serial.begin(9600);
  Serial.println("op!");

  startStop(true);
}

void loop()
{
  updateButtons();

  const bool isStartButtonPressed = startButtonDown && !startButtonWasDown;
  startButtonWasDown = startButtonDown;
  currentTime = millis();

  if (shutterButtonDown)
  {
    startStop(true);
    shutter(true);
    return;
  }

  if (isStartButtonPressed)
  {
    Serial.println("StartButtonPressed");

    startStop();
  }

  if (!isStarted)
  {
    shutter(false);
    return;
  }

  const bool isShutterOpen = shoudOpenShutter();
  shutter(isShutterOpen);
}
