#include "_time.h"


void timeInit() {
  Wire.setClock(100);
  Wire.begin(RTCsda, RTCscl);

  RTCReadReg();
  RTCReadTime();
  
  //Configuration NTP
  if (((RTCReg[0x0F]) & 2) == 0) {
    //debug("Configure NTP");
    //wifiStart();
    if (WiFi.status() == WL_CONNECTED) {
      //GetNTPConfigureRTC();
    }

    debug("Reset");
    RTCReset();

    debug("Configure timer");
    //debug(wakeUpPeriod);
    RTCConfigureTimer();
  }


  if (RTCReg[1] & 0b01000000) {
    wakeUpByRTCAlarm = true;
    wakeUpForConfig = false;
  } else { 
    wakeUpByRTCAlarm = false;
    wakeUpForConfig = true;    
  }
}

byte bcd2int(byte in) {
  return (in & 0xF) + (10 * ((in & 0xF0) >> 4));
}

byte BCD(byte in) {
  return ((in / 10) << 4) | (in % 10);
}



void RTCReset() {
  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.write(0x58);
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x01);
  Wire.write(0b00000010);
  //Wire.write(0b10000000); //battery switch over standard, no battery monitor
  Wire.write(0b11100000); //no battery switch over, no battery monitor
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  /*Wire.write(0x03);
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.write(0x00);*/
  Wire.write(0x06);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
}

void RTCConfigureTimer() {

  Wire.beginTransmission(0x68);
  Wire.write(0x0F);
  Wire.write(0b00111010); //TAM : permanent interrupt, CLKOUT Disabled,  timer A countdown, timer B disabled
  //Wire.write(0b10110010); //TAM : permanent interrupt, CLKOUT 1Hz,  timer A countdown, timer B disabled
  Wire.write(0b00000011); //TAQ : 1time.nist.go/60Hz
  //Wire.write(0b00000010); //TAQ : 1Hz

  int wakeUpPeriod ;

  if (SPIFFS.exists("/measurementFrequency.txt")) {
    int measurementFrequency = readSetting("measurementFrequency").toInt();
    wakeUpPeriod = 1440 / measurementFrequency;
    File logfile = SPIFFS.open("/RTCConfigureTimer.txt", "a");
    logfile.println(wakeUpPeriod);
    logfile.close();
  } else {
    wakeUpPeriod = 1;
  }
  
  
  Wire.write(wakeUpPeriod); //T_A
  //Wire.write(30); //T_A
  //Wire.write(60); //T_A
  Wire.endTransmission();
}

void RTCSetTime() {

  Wire.beginTransmission(0x68);
  Wire.write(0x03);

  Wire.write(BCD(second()));
  Wire.write(BCD(minute()));
  Wire.write(BCD(hour()));
  Wire.write(BCD(day()));
  Wire.write(weekday());
  Wire.write(BCD(month()));
  Wire.write(BCD(year() % 100));

  Wire.endTransmission();
}


void RTCReadTime() {
  RTCReadReg();
  setTime(bcd2int(RTCReg[5] & 0b00111111),
          bcd2int(RTCReg[4] & 0b01111111),
          bcd2int(RTCReg[3] & 0b01111111),
          bcd2int(RTCReg[6] & 0b00111111),
          bcd2int(RTCReg[8] & 0b00011111),
          bcd2int(RTCReg[9]));
}



void RTCClearInterrupt() {
  delay(100); //for Serial to finish flush

  //WiFi.forceSleepBegin();
  //delay(10);

  Wire.beginTransmission(0x68);
  Wire.write(0x01);
  Wire.write(0b00111010); //clear CTAF
  Wire.endTransmission();

  //ESP.deepSleep(10000000, WAKE_RF_DISABLED);
}

void RTCReadReg() {
  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.requestFrom(0x68, 20);

  for (int i = 0; Wire.available() && i < 20; i++) {
    RTCReg[i] = Wire.read();

    /*switch (i) {
      case 3: seconds = (RTCReg[i]) & 0b01111111; break;
      case 4: minutes = (RTCReg[i]) & 0b01111111; break;
      case 5: hours = (RTCReg[i]) & 0b00111111; break;
      }*/
  }

  if (debugRTCReg) {
    String dbg = "RTC register : ";
    for (int i = 0; i < 20; i++) {
      if (RTCReg[i] < 0x10) dbg = dbg + "0";
      dbg = dbg + String(RTCReg[i], HEX) + " ";        // print the character
    }
    debug(dbg);
  }
}


// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address, WiFiUDP udp)
{
  File logfile = SPIFFS.open("/ntp.txt", "a");
  logfile.print("sending ntp packet...");
  logfile.close();
    
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  
  logfile = SPIFFS.open("/ntp.txt", "a");
  logfile.print("apres memset");
  logfile.close();

  
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  logfile = SPIFFS.open("/ntp.txt", "a");
  logfile.print("beginPacket");
  logfile.close();

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123

  logfile = SPIFFS.open("/ntp.txt", "a");
  logfile.print("write packet...");
  logfile.close();

  udp.write(packetBuffer, NTP_PACKET_SIZE);


    logfile = SPIFFS.open("/ntp.txt", "a");
  logfile.print("end packet...");
  logfile.close();

  udp.endPacket();
}

void GetNTPConfigureRTC(IPAddress& timeServerIP) {
  // A UDP instance to let us send and receive packets over UDP
  File logfile = SPIFFS.open("/ntp.txt", "a");
  logfile.println("ntp call");

  logfile.println("Starting UDP");

  udpNtpClient.begin(udpNtpClientPort);
  logfile.print("Local port: ");
  logfile.println(udpNtpClient.localPort());
  logfile.close();

  //get a random server from the pool
  //WiFi.hostByName(ntpServerName, timeServerIP);

  int cb = 0;
  int retry = 0;
  sendNTPpacket(timeServerIP, udpNtpClient); // send an NTP packet to a time server
  delay(100);
  cb = udpNtpClient.parsePacket();

  while ((!cb) && (retry++ < 5)) {
    sendNTPpacket(timeServerIP, udpNtpClient);
    delay(500);
    cb = udpNtpClient.parsePacket();

    logfile = SPIFFS.open("/ntp.txt", "a");
    logfile.println("wait...");
    logfile.close();
  }


  logfile = SPIFFS.open("/ntp.txt", "a");
  logfile.print("packet received, length=");
  logfile.println(cb);


  // We've received a packet, read the data from it
  udpNtpClient.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

  //the timestamp starts at byte 40 of the received packet and is four bytes,
  // or two words, long. First, esxtract the two words:

  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;




  logfile.print("Seconds since Jan 1 1900 = ");
  logfile.println(secsSince1900);
  logfile.close();
  setTime(secsSince1900 - 2208988800);

  RTCSetTime();

  /*int h, m, s;

    h = (secsSince1900  % 86400L) / 3600;
    m = (secsSince1900  % 3600) / 60;
    s = secsSince1900 % 60;

    debug("NTP time : ", true);
    debug(h, true);
    debug(":", true);
    debug(m, true);
    debug(":", true);
    debug(s, true);
    debug("");


    Wire.beginTransmission(0x68);
    Wire.write(0x03);
    Wire.write(BCD(s));
    Wire.write(BCD(m));
    Wire.write(BCD(h));
    Wire.endTransmission();*/


}


void ntpServerInit() {
  udpNtpServer.begin(NTP_PORT);

}

void ntpServerProcess() {
  // if there's data available, read a packet
  int packetSize = udpNtpServer.parsePacket();
  if (packetSize)
  {
    udpNtpServer.read(packetBuffer, NTP_PACKET_SIZE);
    IPAddress Remote = udpNtpServer.remoteIP();
    int PortNum = udpNtpServer.remotePort();


    File logfile = SPIFFS.open("/ntp.txt", "a");
    logfile.print("NTP packet received...");
    logfile.println(String(Remote));
    logfile.close();

    packetBuffer[0] = 0b00100100;   // LI, Version, Mode
    packetBuffer[1] = 1 ;   // stratum
    packetBuffer[2] = 6 ;   // polling minimum
    packetBuffer[3] = 0xFA; // precision

    packetBuffer[7] = 0; // root delay
    packetBuffer[8] = 0;
    packetBuffer[9] = 8;
    packetBuffer[10] = 0;

    packetBuffer[11] = 0; // root dispersion
    packetBuffer[12] = 0;
    packetBuffer[13] = 0xC;
    packetBuffer[14] = 0;


    unsigned long timestamp = now() + 2208988800;
    unsigned long tempval;

    packetBuffer[12] = 71; //"G";
    packetBuffer[13] = 80; //"P";
    packetBuffer[14] = 83; //"S";
    packetBuffer[15] = 0; //"0";

    // reference timestamp
    tempval = timestamp;
    packetBuffer[16] = (tempval >> 24) & 0XFF;
    tempval = timestamp;
    packetBuffer[17] = (tempval >> 16) & 0xFF;
    tempval = timestamp;
    packetBuffer[18] = (tempval >> 8) & 0xFF;
    tempval = timestamp;
    packetBuffer[19] = (tempval) & 0xFF;

    packetBuffer[20] = 0;
    packetBuffer[21] = 0;
    packetBuffer[22] = 0;
    packetBuffer[23] = 0;


    //copy originate timestamp from incoming UDP transmit timestamp
    packetBuffer[24] = packetBuffer[40];
    packetBuffer[25] = packetBuffer[41];
    packetBuffer[26] = packetBuffer[42];
    packetBuffer[27] = packetBuffer[43];
    packetBuffer[28] = packetBuffer[44];
    packetBuffer[29] = packetBuffer[45];
    packetBuffer[30] = packetBuffer[46];
    packetBuffer[31] = packetBuffer[47];

    //receive timestamp
    packetBuffer[32] = (tempval >> 24) & 0XFF;
    tempval = timestamp;
    packetBuffer[33] = (tempval >> 16) & 0xFF;
    tempval = timestamp;
    packetBuffer[34] = (tempval >> 8) & 0xFF;
    tempval = timestamp;
    packetBuffer[35] = (tempval) & 0xFF;

    packetBuffer[36] = 0;
    packetBuffer[37] = 0;
    packetBuffer[38] = 0;
    packetBuffer[39] = 0;

    //transmitt timestamp
    packetBuffer[40] = (tempval >> 24) & 0XFF;
    tempval = timestamp;
    packetBuffer[41] = (tempval >> 16) & 0xFF;
    tempval = timestamp;
    packetBuffer[42] = (tempval >> 8) & 0xFF;
    tempval = timestamp;
    packetBuffer[43] = (tempval) & 0xFF;

    packetBuffer[44] = 0;
    packetBuffer[45] = 0;
    packetBuffer[46] = 0;
    packetBuffer[47] = 0;


    // Reply to the IP address and port that sent the NTP request

    udpNtpServer.beginPacket(Remote, PortNum);
    udpNtpServer.write(packetBuffer, NTP_PACKET_SIZE);
    udpNtpServer.endPacket();


  }
}

String nowStr() {
  String ret;
  time_t dt = now();

  ret = String((day(dt) < 10 ? "0" : "") );
  ret += String(day(dt));
  ret += "-";
  ret += String((month(dt) < 10 ? "0" : "") );
  ret += String(month(dt) );
  ret += "-";
  ret += String(year(dt));
  ret += " ";
  ret += String((hour(dt) < 10 ? "0" : ""));
  ret += String(hour(dt));
  ret += ":";
  ret += String((minute(dt) < 10 ? "0" : ""));
  ret += String(minute(dt));
  ret += ":";
  ret += String((second(dt) < 10 ? "0" : "") );
  ret += String(second(dt));



  return ret;
}
