#include <SPI.h>

//  Command Definitions, data sheet pg. 40
//  System Commands
#define WAKEUP_CMD  0x02
#define STANDBY_CMD 0x04
#define RESET_CMD   0x06
#define START_CMD   0x08
#define STOP_CMD    0x0A

//  Data Read Commands
#define SDATAC_CMD  0x11
#define RDATA_CMD   0x12

//  Register Read & Write
#define PREG_CMD    0x20
#define WREG_CMD    0x40

//  PIN
const byte RESET_PIN = 3;
const byte START_PIN = 4;
const byte CS_PIN = 5;
const byte DRDY_PIN = 6;
const byte CLKSEL_PIN = 7;

//  Device Characteristics
//  Variables are set by getDeviceCharacteristics function
byte totalChannels = 0;

void setup() {
  Serial.begin(9600);

  //  SPI Set Up
  SPI.begin();
  //  data sheet pg.12 -> SPI setting: CPOL = 0, CPHA = 1
  //  https://en.wikipedia.org/wiki/Serial_Peripheral_Interface
  SPI.setDataMode(SPI_MODE1);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.setBitOrder(MSBFIRST);

  //  Pin Set Up
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  pinMode(START_PIN, OUTPUT);
  digitalWrite(START_PIN, LOW);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, LOW);
  pinMode(DRDY_PIN, INPUT);
  //  Used to select internal or external clock
  //  Pull CLKSEL_PIN low if external clock signal is provided
  pinMode(CLKSEL_PIN, OUTPUT);
  digitalWrite(CLKSEL_PIN, LOW);

  //  data sheet ------ pg.62
  //  Configured to generate internal test signals
  startPowerUpSequence();
  //  enable bias drive
  enableBiasSense();
  //  get total channel number
  getDeviceID();

  //  Uncomment to enable differential input (Both P and N electrode)
//  enableDifferentialInput();

  //  Use one electrode (P side) to record
  enableSingleEndedInput();

  digitalWrite(START_PIN, HIGH);
  delay(10);
}

void loop() {
//  See data in serial monitor
  dataToSerialMonitor();
}

//  Power up sequence ------ data sheet pg.62
//  The sequence is modified the generate internal test data
void startPowerUpSequence() {
  Serial.println("Waking up and entering test mode...");
  digitalWrite(CLKSEL_PIN, HIGH);
  delay(10);
  digitalWrite(RESET_PIN, HIGH);
  //  wait for VCAP1 to settle to 1.1V
  delay(500);
  SPI.transfer(RESET_CMD);
  delayMicroseconds(36);
  //  Device wakes up automatically at RDATAC (Read Data Continous) mode.
  //  Send SDATAC (Stop Data Continous) command in order to write to registers.
  SPI.transfer(SDATAC_CMD);
  delayMicroseconds(10);
  //  CONFIG1
  //  CLK_EN = 1 -> enable internal clock output
  //  DR = 110 -> fMOD/4096
  writeToRegister(0x96, 0x01);
  delay(10);
  //  CONFIG2 ------ data sheet pg.47
  //  configured to generate internal test signal at DC (direct current)
  writeToRegister(0xD3, 0x02);
  delay(10);
  //  CONFIG3 ------ data sheet pg.48
  //  PDB_REFBUF = 1 -> use internal reference
  //  BIASREF_INT = 1 -> enable internal reference signal, this is used to establish
  //  common ground and prevent significant voltage offset
  writeToRegister(0xEC, 0x03);
  delay(50);
  //  CHnSET ------ data sheet pg.50
  //  All the channels' gain are set to 24 and generate internal test signals
  writeToRegister(0b01100101, 0x05);
  delayMicroseconds(8);
  writeToRegister(0b01100101, 0x06);
  delayMicroseconds(8);
  writeToRegister(0b01100101, 0x07);
  delayMicroseconds(8);
  writeToRegister(0b01100101, 0x08);
  delayMicroseconds(8);
  writeToRegister(0b01100101, 0x09);
  delayMicroseconds(8);
  writeToRegister(0b01100101, 0x0A);
  delayMicroseconds(8);
  writeToRegister(0b01100101, 0x0B);
  delayMicroseconds(8);
  writeToRegister(0b01100101, 0x0C);
  delayMicroseconds(8);
}

void enableDifferentialInput() {
  Serial.println("Enabling differential input...");
  writeToRegister(0b01100000, 0x05);
  delayMicroseconds(8);
  writeToRegister(0b01100000, 0x06);
  delayMicroseconds(8);
  writeToRegister(0b01100000, 0x07);
  delayMicroseconds(8);
  writeToRegister(0b01100000, 0x08);
  delayMicroseconds(8);
  writeToRegister(0b01100000, 0x09);
  delayMicroseconds(8);
  writeToRegister(0b01100000, 0x0A);
  delayMicroseconds(8);
  writeToRegister(0b01100000, 0x0B);
  delayMicroseconds(8);
  writeToRegister(0b01100000, 0x0C);
  delayMicroseconds(8);
}

void enableSingleEndedInput() {
  Serial.println("Enabling single ended input...");
  writeToRegister(0b00100110, 0x15);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x05);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x06);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x07);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x08);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x09);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x0A);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x0B);
  delayMicroseconds(8);
  writeToRegister(0b01100110, 0x0C);
  delayMicroseconds(8);
}

void enableBiasSense() {
  Serial.println("Enabling bias sense...");
  writeToRegister(0b11111111, 0x0D);
  delayMicroseconds(8);
  writeToRegister(0b11111111, 0x0E);
  delayMicroseconds(8);
}

//  This returns device ID, something like 00111110
//  The last two bits indicate how many channels is availble
//  00 -> 4 channels, 01 -> 6 channels, 10 -> 8 channels
void getDeviceID() {
  byte deviceID = readRegister(0x00);
  const byte numberOfChannels = deviceID & 0x03;
  if (numberOfChannels == 0x00) {
    totalChannels = 4;
  }
  if (numberOfChannels == 0x01) {
    totalChannels = 6;
  }
  if (numberOfChannels == 0x2) {
    totalChannels = 8;
  }
}

void writeToRegister(byte command, byte address) {
  byte _address = WREG_CMD | address;
  SPI.transfer(_address);
  delayMicroseconds(8);
  //  read 1 register
  SPI.transfer(0x00);
  //  No Operation (NOP) to retrieve the data
  SPI.transfer(command);
}

byte readRegister(byte address) {
  byte _address = PREG_CMD | address;
  SPI.transfer(_address);
  delayMicroseconds(8);
  //  read 1 register
  SPI.transfer(0x00);
  //  No Operation (NOP) to retrieve the data
  byte rData = SPI.transfer(0x00);
  delayMicroseconds(2);
  return rData;
}

void readData() {
  SPI.transfer(RDATA_CMD);
}

// Display data that's being transfered in serial monitor
// Mostly used for debuging and testing
void dataToSerialMonitor() {
  //  3 bytes per channel + 3 bytes status data at the beginning + 1 byte header
  //  data sheet ------ pg.39
  long stat = 0x00FFFFFF;
  long channelData [totalChannels];
  Serial.print("TOTAL CHANNEL | ");
  Serial.println(totalChannels);
  readData();
  //  1000 ms / 250 sps = 4 ms per sample
  delay(4);
  readIncomingData(stat, channelData);
  Serial.print("TIME PASSED | ");
  Serial.println(millis());
  Serial.print("STAT | ");
  Serial.println(stat, HEX);
  for (byte i = 0; i < totalChannels; i += 1) {
    Serial.print("Channel ");
    Serial.print(i + 1);
    Serial.print(channelData[i], HEX);
    Serial.print(" | ");
    Serial.print(channelData[i]);
    Serial.print(" | ");
    Serial.println(convertChannelData(channelData[i]), 7);
  }
}

// Aggregates every 3 bytes to according channel and display ADC converted value
// Mostly used for reading data in serial monitor & internal test
void readIncomingData(long &stat, long channelData[]) {
  //  3 bytes per channel + 3 bytes status data at the beginning
  //  data sheet ------ pg.39
  int bytesToRead = 3 + (3 * totalChannels);
  //  Bit mask: long is 32 bits variable, only select 0 - 23rd bits
  long mask = 0x00FFFFFF;
  //  Map channel 1 through n to 1 iteration
  //  e.g channel 1 is bytes in range 2 - 6
  byte currentChannel = 1;
  byte firstChannelByte = (currentChannel + 1) * 3 - 4;
  byte lastChannelByte = (currentChannel + 1) * 3;
  for (int i = 0; i < bytesToRead; i += 1) {
    byte data = SPI.transfer(0x00);
    if (i < 3) {
      stat = stat << 8;
      stat = stat | data;
    }
    if (i == 2) {
      stat = stat & mask;
    }
    if (i > firstChannelByte && i < lastChannelByte) {
      channelData[currentChannel - 1L] = channelData[currentChannel - 1L] << 8;
      channelData[currentChannel - 1L] = channelData[currentChannel - 1L] | data;
    }
    if (i == (lastChannelByte - 1)) {
      channelData[currentChannel - 1] = channelData[currentChannel - 1] & mask;
      currentChannel = currentChannel + 1;
      firstChannelByte = (currentChannel + 1) * 3 - 4;
      lastChannelByte = (currentChannel + 1) * 3;
    }
  }
}

void processIncomingData(byte channelData[]) {
  byte bytesToRead = totalChannels * 3 + 3;
  for (int i = 0; i < bytesToRead; i += 1) {
    channelData[i] = SPI.transfer(0x00);
  }
}

float convertChannelData(long data) {
  float lsb = (2 * 4.5 / 24) / (pow(2, 24));
  long maxValue = pow(2L, 23L) - 1L;
  if (data > maxValue) {
    long negativeValue = data - maxValue - 1;
    return (negativeValue - pow(2L, 23L)) * lsb;
  }
  return data * lsb;
}
