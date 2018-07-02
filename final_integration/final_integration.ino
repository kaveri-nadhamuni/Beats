#include <U8g2lib.h>
#include <WiFiClientSecure.h>
#include <base64.h>
#include <math.h>
#include <mpu9255_esp32.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
//WiFiClientSecure is a big library. It can take a bit of time to do that first compile

// Set up the oled object
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled(U8G2_R0, 5, 17, 16);

#define DELAY 1000
#define SAMPLE_FREQ 8000                           // Hz, telephone sample rate
#define SAMPLE_DURATION 5                         // duration of fixed sampling
#define NUM_SAMPLES SAMPLE_FREQ*SAMPLE_DURATION    // number of of samples
#define ENC_LEN (NUM_SAMPLES + 2 - ((NUM_SAMPLES + 2) % 3)) / 3 * 4  // Encoded length of clip


/* STATES */
#define INITIALIZE 0
#define RECORD_TIME 1
#define BEGIN_RUN 2
#define DISPLAY_RUN 3
#define DISPLAY_HEART 4
#define DISPLAY_STEPS 5
#define SEND_DATA 6

int state = 0;
int flag;
/* CONSTANTS */
//Prefix to POST request:
const String PREFIX = "{\"config\":{\"encoding\":\"MULAW\",\"sampleRateHertz\":8000,\"languageCode\": \"en-US\", \"profanityFilter\": true, \"speechContexts\": [{\"phrases\":[\"hours\", \"minutes\", \"seconds\",\"MIT\"]}]}, \"audio\": {\"content\":\"";
const String SUFFIX = "\"}}"; //suffix to POST request
const int AUDIO_IN = A0; //pin where microphone is connected
const int BUTTON_PIN = 15; //pin where button is connected
const int BUTTON_PIN_2 = 2;
const String API_KEY = "AIzaSyC2nT5F69sBBaldwhMkcf_nLxzpexAMslg";
MPU9255 imu; //imu object called, appropriately, imu
const int LOOP_SPEED = 4; //milliseconds
const int response_timeout = 6000;
int primary_timer = 0;

float x, y, z; //variables for grabbing x,y,and z values
float totalA;
float a_older = 0;
float a_oldest = 0;

int last_step_timer = 0;
int timeBetweenSteps = 1000;
int state2 = 0;
int timeBetweenPushes = 1000;
int last_push_timer = 0;
int last_push_timer_2 = 0;
const int timeout = 1000;

//may not need
int step_count = 0;
float step_current = 0.0;
float step_older = 0.0;
float step_oldest = 0.0;

//new average filter variables
int filter_time = 5000;
int filter_timer = 0;
int filter_current = 0;
int filter_older = 0;
int filter_oldest = 0;
int filter_unit_step = 0;

//storing heart and steps array
int step_array[720];
int step_index = 0;

//storing send info
String heart;
String to_send_steps;
String kerb;


/* Global variables*/
int button_state; //used for containing button state and detecting edges
int old_button_state; //used for detecting button edges
int button; //used for detecting button edges
unsigned long time_since_sample;      // used for microsecond timing
unsigned long timer;
int run_time = 0;
String str_run_time = "";
int temp_time = 0;
String speech_data; //global used for collecting speech data
const char* ssid     = "6s08";     // your network SSID (name of wifi network)
const char* password = "iesc6s08"; // your network password
const char*  server = "speech.google.com";  // Server URL
bool pressed = false;
bool pressed2 = false;

//EKG variables
int pulse_pin = A7;
const int THRESHOLD = 550; // Adjust this number to avoid noise when idle
const int SAMPLE_RATE=10; //100Hz sampling rate
const int LATEST_HEARTRATE=6; 
const int stored_size=5000;
const int max_sample_count=720; //the maximum number of heartrates we will collect, corresponds to sampling every 5 seconds for 1 hour
float threshold = 1.3; 
float stored_values[stored_size]={};        //to store values of latest readings from the pulse sensor

int heartrate_index=1;
float heartrate_values[max_sample_count]={};      //to store values of heartrates during run
float latest_heartrate[LATEST_HEARTRATE]={};

int pulse_counter=0; //to keep track of number of pulses detected, resets after stroed_size 
unsigned long pulse_timer;  //to ensure that there is a delay after pulse is detected

unsigned long sample_timer; //how often you sample 

//to indicate that it is in calibration mode
unsigned long calibration_timer;

//plotting the heartrate trend
float last_vals[10]={0,0,0,0,0,0,0,0,0,0};
unsigned long step_timer;       //step timer that continuously plots within step function
int step_increment;
float last_portion=0;
int graph_state=0;
int i=0;

int ekg_time = 5000;
int ekg_timer = 0;
int ekg_current = 0;
int ekg_older = 0;
int ekg_oldest = 0;
int ekg_unit_step = 0;

WiFiClientSecure client; //global WiFiClient Secure object

//Below is the ROOT Certificate for Google Speech API authentication (we're doing https so we need this)
//don't change this!!
const char* root_ca = \
                      "-----BEGIN CERTIFICATE-----\n" \
                      "MIIDVDCCAjygAwIBAgIDAjRWMA0GCSqGSIb3DQEBBQUAMEIxCzAJBgNVBAYTAlVT\n" \
                      "MRYwFAYDVQQKEw1HZW9UcnVzdCBJbmMuMRswGQYDVQQDExJHZW9UcnVzdCBHbG9i\n" \
                      "YWwgQ0EwHhcNMDIwNTIxMDQwMDAwWhcNMjIwNTIxMDQwMDAwWjBCMQswCQYDVQQG\n" \
                      "EwJVUzEWMBQGA1UEChMNR2VvVHJ1c3QgSW5jLjEbMBkGA1UEAxMSR2VvVHJ1c3Qg\n" \
                      "R2xvYmFsIENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2swYYzD9\n" \
                      "9BcjGlZ+W988bDjkcbd4kdS8odhM+KhDtgPpTSEHCIjaWC9mOSm9BXiLnTjoBbdq\n" \
                      "fnGk5sRgprDvgOSJKA+eJdbtg/OtppHHmMlCGDUUna2YRpIuT8rxh0PBFpVXLVDv\n" \
                      "iS2Aelet8u5fa9IAjbkU+BQVNdnARqN7csiRv8lVK83Qlz6cJmTM386DGXHKTubU\n" \
                      "1XupGc1V3sjs0l44U+VcT4wt/lAjNvxm5suOpDkZALeVAjmRCw7+OC7RHQWa9k0+\n" \
                      "bw8HHa8sHo9gOeL6NlMTOdReJivbPagUvTLrGAMoUgRx5aszPeE4uwc2hGKceeoW\n" \
                      "MPRfwCvocWvk+QIDAQABo1MwUTAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBTA\n" \
                      "ephojYn7qwVkDBF9qn1luMrMTjAfBgNVHSMEGDAWgBTAephojYn7qwVkDBF9qn1l\n" \
                      "uMrMTjANBgkqhkiG9w0BAQUFAAOCAQEANeMpauUvXVSOKVCUn5kaFOSPeCpilKIn\n" \
                      "Z57QzxpeR+nBsqTP3UEaBU6bS+5Kb1VSsyShNwrrZHYqLizz/Tt1kL/6cdjHPTfS\n" \
                      "tQWVYrmm3ok9Nns4d0iXrKYgjy6myQzCsplFAMfOEVEiIuCl6rYVSAlk6l5PdPcF\n" \
                      "PseKUgzbFbS9bZvlxrFUaKnjaZC2mqUPuLk/IH2uSrW4nOQdtqvmlKXBx4Ot2/Un\n" \
                      "hw4EbNX/3aBd7YdStysVAq45pmp06drE57xNNB6pXE0zX5IJL4hmXXeXxx12E6nV\n" \
                      "5fEWCRE11azbJHFwLJhWC9kXtNHjUStedejV0NxPNO3CBWaAocvmMw==\n" \
                      "-----END CERTIFICATE-----\n";


void setup() {
  Serial.begin(115200);               // Set up serial port
  speech_data.reserve(PREFIX.length() + ENC_LEN + SUFFIX.length());
  WiFi.begin(ssid, password); //attempt to connect to wifi
  //WiFi.begin("6.s08", "");
  int count = 0; //count used for Wifi check times
  while (WiFi.status() != WL_CONNECTED && count < 6) {
    delay(1000);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.print("Connected to ");
    Serial.println(ssid);
    delay(500);
  } else { //if we failed to connect just ry again.
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP
  }

  oled.begin();
  oled.setFont(u8g2_font_5x7_tf);  //set font on oled
  oled.setCursor(0, 15);
  oled.print("Ready. Press to Record");
  oled.setCursor(0, 30);
  oled.print("See serial monitor");
  oled.setCursor(0, 45);
  oled.print("for debugging information");
  oled.sendBuffer();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  old_button_state = digitalRead(BUTTON_PIN);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);
  setup_imu();
  display_STARTUP();
  delay(2000);
  primary_timer = millis();
  filter_timer = millis();

  //EKG initiation
  for (int i=0; i<stored_size; i++){
    stored_values[i]=0;
  }

  for (int i=0; i<max_sample_count; i++){
    heartrate_values[i]=0;
  }

  for (int i=0; i<LATEST_HEARTRATE; i++){
    latest_heartrate[i]=0;
  }
  pulse_timer=millis(); //initialize timer that ensures that there is a wait between pulses being detected
  ekg_timer = millis();
  calibration_timer=millis();

  //for plotting
  step_timer=millis();
  step_increment=0;

}

//main body of code
void loop() {
  updateButton();
  //  Serial.println(String(state));
  switch (state) {
    // waiting for button to record time
    case 0: //press button to record time
      oled.clearBuffer();
      oled_print("Press Left Button", 5, 15);
      oled_print("to Continue", 5, 25);
      oled.sendBuffer();
      if (pressed) {

        state = 1;
      }
      break;
    //button down state
    case 1:
      if (!pressed) {
        state = 2;
        oled.clearBuffer();
      }
      delay(250);
      break;
    //recording starts after button release
    case 2:
      oled.clearBuffer();
      oled_print("Hold Down Button to", 5, 15);
      oled_print("Begin Recording Audio", 5, 25);
      oled.sendBuffer();
      //speech_to_text() if not 0 move to state 3 otherwise stay
      //button_state = digitalRead(BUTTON_PIN);
      //if (!button_state && button_state != old_button_state)
      button_state = digitalRead(BUTTON_PIN);
      if (!button_state && button_state != old_button_state) {
        temp_time = speech_to_text();
        if (temp_time != 0) {
          run_time = temp_time;
          state = 3;
        }
        else {
          oled.clearBuffer();
          oled.setCursor(0, 30);
          oled.print("I was unable to understand your time");
          oled.setCursor(0, 45);
          oled.print("Please Repeat Your Time");
          oled.sendBuffer();
        }
      }
      old_button_state = button_state;
      //record_audio;
      //immediately goes to state 3 for testing purposes
      //for testing purposes
      //run_time = 20000;
      //state = 3;
      break;
    //sending request, making timer, record run
    case 3:
      displayConfirmation();
      //left button
      if (!digitalRead(BUTTON_PIN)) {
        timer = millis();
        //we start the run
        flag = 1;
        i=0;
        state =  4;
        calibration_timer=millis();
        oled.clearBuffer();
      }
      //right button
      else if (!digitalRead(BUTTON_PIN_2)) {
        state = 2;
      }

      //send HTTP request
      //if(input is not correct){
      //state =  2;
      //}
      //if(input is correct){
      //define runningTimer;

      break;
    //displaying ekg
    case 4:
      graphEKG();
      oled.sendBuffer();
      //StepCounter;
      //EKGdata;
      //displayEKG;
      if (pressed) {
        state = 5;

      }
      if (millis() - timer > run_time) {
        state = 7;
      }
      //transitioning between ekg to steps
      break;
    case 5:
      oled_print_at("5: button transition state 4->7", 5, 15);
      if (!pressed) {
        state = 7;

      }
      if (millis() - timer > run_time) {
        state = 8;
      }
      //displaying steps
      break;
    case 6:
      oled_print_at("6: button transition state 7->4", 5, 15);
      if (!pressed) {
        state = 4;
        i=0;
        oled.clearBuffer();
      }
      if (millis() - timer > run_time) {
        state = 8;
      }
      //displaying steps
      break;
    case 7:
      //    oled_print_at("7: ongoing, display steps",5,15);
      //StepCounter;
      //EKGdata;
      displaySteps();
      if (pressed) {
        state = 6;
      }
      if (millis() - timer > run_time) {
        state = 8;
      }

      break;
    //run is finished, send the data, display final stats
    case 8:
      flag = 0;
      oled_print_at("8: send data, display final stats", 5, 15);
      heart = arr_to_string_heart(heartrate_values, heartrate_index);
      Serial.println("step_index in 8" + String(step_index));
      to_send_steps = arr_to_string_steps(step_array, step_index);
      Serial.println("actual stuff" + to_send_steps);
      kerb = "shannen";
      post_STAT(heart, to_send_steps, kerb);
      delay(3000);
      state = 9;
      //send data;
      //turn off EKG;
      break;
    case 9:
      reset_RUN();
      oled_print_at("9: Press the button to run again", 5, 15);
      if (pressed) {
        state = 0;
      }
      break;
  }
  if (flag == 1) {
    stepCounter();
    EKG();
  }
}

void EKG(){
  if (millis()-calibration_timer>60000){
    //Serial.print("one minute has passed");
    //calibration_timer=millis();
  }
  if (millis()-sample_timer>SAMPLE_RATE){
    peak();    
    sample_timer=millis();
  }
}

void post_STAT(String heart, String steps, String kerb) {
  Serial.println("called postscore");
  WiFiClient client; //instantiate a client object
  if (client.connect("iesc-s1.mit.edu", 80)) { //try to connect to  host
    // This will send the request to the server
    // If connected, fire off HTTP GET:
    String to_send = "heartRate=" + heart + "&steps=" + steps + "&kerb=" + kerb;
    client.println("POST /608dev/sandbox/doryshen/heartRate/heartRate.py HTTP/1.1");
    client.println("Host: iesc-s1.mit.edu");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Content-Length: " + String(to_send.length()));
    client.print("\r\n");
    client.print(to_send);
    Serial.println("to_send " + to_send);
    unsigned long count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      String line = client.readStringUntil('\n');
      Serial.println(line);
      if (line == "\r") { //found a blank line!
        //headers have been received! (indicated by blank line)
        break;
      }
      if (millis() - count > response_timeout) break;
    }
    count = millis();
    String op; //create empty String object
    while (client.available()) { //read out remaining text (body of response)
      op += (char)client.read();
    }
    Serial.println('returned' + String(op));
    client.stop();
    Serial.println();
    Serial.println("-----------");
  } else {
    Serial.println("connection failed");
    Serial.println("wait 0.5 sec...");
    client.stop();
    delay(500);
  }

}
void stepCounter() {
  imu.readAccelData(imu.accelCount);
  x = imu.accelCount[0] * imu.aRes;
  y = imu.accelCount[1] * imu.aRes;
  z = imu.accelCount[2] * imu.aRes;
  totalA = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));
  float a_current = totalA;
  float y = (1.0 / 3.0) * (a_current + a_older + a_oldest);
  // Serial.println("y " + String(y));
  // Serial.println(String(totalA) + ", " + String(y) + ", " + String(step_count) + ", " + String(state));
  // oled_print_at("Total A: "+ String(totalA),0,15);
  a_oldest = a_older;
  a_older = a_current;
  if (state2 == 0) {
    //  Serial.println("state 0");
    //delta steps in past 5 seconds??
    if (millis() - filter_timer >= filter_time) {
      filter_oldest = filter_older;
      filter_older = filter_current;
      filter_current = step_count - filter_unit_step;
      step_array[step_index] = filter_current;
      filter_unit_step = step_count;
      filter_timer = millis();
      step_index = step_index + 1;
     Serial.println("giant array " + arr_to_string_steps(step_array, step_index));
    }

    if (y > 1.2) {
      step_count += 1;
      last_step_timer = millis();
      Serial.println("step_count should increment " + String(step_count));
      state2 = 1;
    }
    //   averageSteps();

  } else { //state 1
    if (millis() - last_step_timer > timeBetweenSteps) {
      state2 = 0;
    }
  }
  while (millis() - primary_timer < LOOP_SPEED); //wait for primary timer to increment
  primary_timer = millis();
}

//can repackage the new one in here later
void averageSteps() {
  float y_1 = (1.0 / 3.0) * (step_count + step_older + step_oldest);
  Serial.println("y_1 " + String(y_1));
  step_oldest = step_older;
  step_older = step_current;
  step_current = y_1;
  Serial.println("step current " + String(step_current));
  Serial.println("rounded? " + String((int) round(step_current)));
  step_count = (int) round(step_current);
  Serial.println("step count " + String(step_count));
}

void displaySteps() {
  oled.clearBuffer();
  oled_print("Current steps: " + String(step_count), 5, 15);
  //  oled_print("Steps per minute: " + String(delta_step * 6), 5, 30);
  oled_print("Filtered steps/min: " + String((filter_current + filter_older + filter_oldest) * 4), 5, 30);
  Serial.println("Current steps: " + String(step_count));
  oled.sendBuffer();
}

void displayConfirmation(){
    oled.clearBuffer();
    oled.setCursor(0, 15);
    oled.print("3: set time, record run");
    oled.setCursor(0, 30);
    oled.print("Is " + String(str_run_time) + " correct?");
    oled.setCursor(0, 45);
    oled.print("Left Button to Confirm");
    oled.setCursor(0, 60);
    oled.print("Right Button to Repeat");
    oled.sendBuffer();
}

void updateButton() { //this function here for clarity
  pressed = !digitalRead(BUTTON_PIN);
  pressed2 = !digitalRead(BUTTON_PIN_2);
}

int getSeconds(String op) {
  Serial.println(op);
  if ((op.indexOf("hour") != -1) || (op.indexOf("minute") != -1) || (op.indexOf("second") != -1)) {
    oled.clearBuffer();
    int space_index = op.indexOf(" ");
    int num;
    if ((op.indexOf("a") != -1) || (op.indexOf("one") != -1)) {
      num = 1;
    }
    else {
      String num_string = op.substring(0, space_index);
      num = num_string.toInt();
    }
    int units = 1000;
    if (op.indexOf("hour") != -1) {
      units *= 3600;
    }
    else if (op.indexOf("minute") != -1) {
      units *= 60;
    }
    oled.print(String(units * num));
    oled.sendBuffer();
    return units * num;
  }
  return 0;
}

int speech_to_text() {
  int seconds = 0;
  //button_state = digitalRead(BUTTON_PIN);
  //if (!button_state && button_state != old_button_state) {
  if (true) {
    //client.setCACert(root_ca);
    delay(200);
    Serial.println("listening...");
    oled.clearBuffer();    //clear the screen contents
    oled.drawStr(0, 15, "listening...");
    oled.sendBuffer();     // update the screen
    record_audio();
    Serial.println("sending...");
    oled.clearBuffer();    //clear the screen contents
    oled.drawStr(0, 15, "sending...");
    oled.sendBuffer();     // update the screen
    Serial.print("\nStarting connection to server...");
    delay(300);
    bool conn = false;
    for (int i = 0; i < 10; i++) {
      if (client.connect(server, 443)); {
        conn = true;
        break;
      }
      Serial.print(".");
      delay(300);
    }
    if (!conn) {
      Serial.println("Connection failed!");
      return 0;
    } else {
      Serial.println("Connected to server!");
      // Make a HTTP request:
      delay(200);
      client.println("POST https://speech.googleapis.com/v1/speech:recognize?key=" + API_KEY + "  HTTP/1.1");
      client.println("Host: speech.googleapis.com");
      client.println("Content-Type: application/json");
      client.println("Cache-Control: no-cache");
      client.println("Content-Length: " + String(speech_data.length()));
      client.print("\r\n");
      int len = speech_data.length();
      int ind = 0;
      int jump_size = 3000;
      while (ind < len) {
        delay(100);//experiment with this number!
        if (ind + jump_size < len) client.print(speech_data.substring(ind, ind + jump_size));
        else client.print(speech_data.substring(ind));
        ind += jump_size;
      }
      //client.print("\r\n\r\n");
      unsigned long count = millis();
      while (client.connected()) {
        String line = client.readStringUntil('\n');
        Serial.print(line);
        if (line == "\r") { //got header of response
          Serial.println("headers received");
          break;
        }
        if (millis() - count > 4000) break;
      }
      Serial.println("Response...");
      count = millis();
      while (!client.available()) {
        delay(100);
        Serial.print(".");
        if (millis() - count > 4000) break;
      }
      Serial.println();
      Serial.println("-----------");
      String op;
      while (client.available()) {
        op += (char)client.read();
      }
      Serial.println(op);
      int trans_id = op.indexOf("transcript");
      if (trans_id != -1) {
        int foll_coll = op.indexOf(":", trans_id);
        int starto = foll_coll + 2; //starting index
        int endo = op.indexOf("\"", starto + 1); //ending index
        oled.clearBuffer();    //clear the screen contents
        oled.setCursor(0, 15);
        oled.print(op.substring(starto + 1, endo));
        oled.sendBuffer();     // update the screen
        delay(2000);
        str_run_time = op.substring(starto + 1, endo);
        Serial.println("str run time" + String(str_run_time));
        seconds = getSeconds(op.substring(starto + 1, endo));
      }
      Serial.println("-----------");
      client.stop();
      Serial.println("done");
    }
  }
  return seconds;
}

//function used to record audio at sample rate for a fixed nmber of samples
void record_audio() {
  int sample_num = 0;    // counter for samples
  int enc_index = PREFIX.length() - 1;  // index counter for encoded samples
  float time_between_samples = 1000000 / SAMPLE_FREQ;
  int value = 0;
  uint8_t raw_samples[3];   // 8-bit raw sample data array
  String enc_samples;     // encoded sample data array
  time_since_sample = micros();
  Serial.println(NUM_SAMPLES);
  speech_data = PREFIX;
  while (sample_num < NUM_SAMPLES && !digitalRead(BUTTON_PIN)) {
    //while (sample_num<NUM_SAMPLES) {   //read in NUM_SAMPLES worth of audio data
    value = analogRead(AUDIO_IN);  //make measurement
    raw_samples[sample_num % 3] = mulaw_encode(value - 1241); //remove 1.0V offset (from 12 bit reading)
    sample_num++;
    if (sample_num % 3 == 0) {
      speech_data += base64::encode(raw_samples, 3);
    }

    // wait till next time to read
    while (micros() - time_since_sample <= time_between_samples); //wait...
    time_since_sample = micros();
  }
  speech_data += SUFFIX;
}

//fill in functions later
void reset_RUN() {
  primary_timer = millis();
  //  filter_timer = millis();
  x, y, z; //variables for grabbing x,y,and z values
  totalA;
  a_older = 0;
  a_oldest = 0;
  step_count = 0;
  filter_time = 5000;
  filter_timer = 0;
  filter_current = 0;
  filter_older = 0;
  filter_oldest = 0;
  filter_unit_step = 0;
  step_array[720];
}
void display_STARTUP() {
  oled.clearBuffer();
  String message = "runner fitness pal thingy!";
  oled_print_at(message, 3, 10);
  oled.sendBuffer();
}

void oled_print_at(String input, int x, int y) {
  oled.clearBuffer();
  oled.setCursor(x, y);
  oled.print(input);
  oled.sendBuffer();
}

//to use within display functions (doesnt have clear and send)
void oled_print(String input, int x, int y) {
  oled.setCursor(x, y);
  oled.print(input);
}

int8_t mulaw_encode(int16_t sample) {
  const uint16_t MULAW_MAX = 0x1FFF;
  const uint16_t MULAW_BIAS = 33;
  uint16_t mask = 0x1000;
  uint8_t sign = 0;
  uint8_t position = 12;
  uint8_t lsb = 0;
  if (sample < 0)
  {
    sample = -sample;
    sign = 0x80;
  }
  sample += MULAW_BIAS;
  if (sample > MULAW_MAX)
  {
    sample = MULAW_MAX;
  }
  for (; ((sample & mask) != mask && position >= 5); mask >>= 1, position--)
    ;
  lsb = (sample >> (position - 4)) & 0x0f;
  return (~(sign | ((position - 5) << 4) | lsb));
}

void setup_imu() {
  if (imu.readByte(MPU9255_ADDRESS, WHO_AM_I_MPU9255) == 0x73) {
    imu.initMPU9255();
  } else {
    while (1) Serial.println("NOT FOUND"); // Loop forever if communication doesn't happen
  }
  imu.getAres(); //call this so the IMU internally knows its range/resolution
}

String arr_to_string_steps(int step_array[720], int len) {
  String output = "[";
  for (int i = 0; i < len; i++) {
    output += String(step_array[i]) + ", ";
  }
  output += "]";
  return output;

}

String arr_to_string_heart(float heartrate_values[720], int len) {
  String output = "[";
  for (int i = 0; i < heartrate_index; i++) {
    output += String(heartrate_values[i]) + ", ";
  }
  output += "]";
  return output;

}

float averaging_filter(float input) {
  //averaging the values from the EKG to help calculate the thresholds
    float sum=0;
    stored_values[0]=input;
    for (int i=0; i<=stored_size; i++){
        sum=sum+stored_values[i];
    }
    
    for (int i=stored_size; i>0; i=i-1){
        stored_values[i]=stored_values[i-1];
    }
   
    return sum/stored_size;
    
}
$$$start
float shift_heartrates(float input) {
  //shift the heartrates to get the latest heartrates that can be used to calculate the BPMs
    float sum=0;
    latest_heartrate[0]=input;
    for (int i=0; i<=LATEST_HEARTRATE; i++){
        sum=sum+latest_heartrate[i];
    }
    
    for (int i=LATEST_HEARTRATE; i>0; i=i-1){
        latest_heartrate[i]=latest_heartrate[i-1];
    }
   
    return sum;
    
}
$$$end

void peak(){
  //Serial.println("peak is being called");
  //function to get signal from EKG and then calculate the BPM
  uint16_t raw_reading = analogRead(pulse_pin);
  float scaled_reading = raw_reading*3.3/512;
  
  float average= averaging_filter(scaled_reading);
  //Serial.println(String(scaled_reading)+" "+String(average*threshold));
  if (millis()-pulse_timer>300 && scaled_reading>(average*threshold)){
    Serial.println("pulse detected "+ String(pulse_counter)+String(average));
    pulse_counter+=1;
    pulse_timer=millis();
  }
  if (millis()-ekg_timer>=ekg_time){
    //ekg_current = pulse_counter-ekg_current;
    int heartbeats_thirty= shift_heartrates(pulse_counter);
    pulse_counter=0;
    ekg_timer = millis();
    int heartrate=(heartbeats_thirty)*2; 
    Serial.println("BPM:"+String(heartrate));
    heartrate_values[heartrate_index]=heartrate;
    //Serial.println("BPM:"+String(heartrate_values[heartrate_index]));
    heartrate_index+=1;
  
  }
}

void graphEKG(){
  //graph the heartrates
  //Serial.println("graph_state"+String(graph_state));
  switch(graph_state){
    case 0:
      Serial.println(String(i));
      draw_axes();
      if (i>=25){
        i=0;
        oled.clearBuffer();
      }
      graph_state=1;
      i+=1;
      last_portion=0;
      break;
    case 1:
      if (millis()-step_timer<5000){
        graph_state=1;
      }else{
        int x_0=i*5;
        int x_1=(i+1)*5;
        int y_0=64-heartrate_values[heartrate_index-2]/3.0;
        int y_1=64-heartrate_values[heartrate_index-1]/3.0;

        //correct outliers
        if (y_1<0){
          y_1=0;
        }else if(y_1>64){
          y_1=64;
        }
        if (y_0<0){
          y_0=0;
        }else if(y_0>64){
          y_0=64;
        }
        
        oled.drawLine(x_0,y_0,x_1,y_1);
        step_timer=millis();
        oled.setDrawColor(0);
        oled.drawBox(70,0,58,10);
        oled.setDrawColor(1);
        oled.setCursor(70,10);
        
        if (millis()-calibration_timer<60000){
          oled.print("Calibrating..");
        }else{
          oled.print("BPM: "+String(int(heartrate_values[heartrate_index-1])));
        }
        
        oled.sendBuffer();
        graph_state=0;
      }
      break;
  }
}

void draw_axes(){
  oled.drawLine(0,0,0,64);
  for (int i=0; i<20;i++){
    int axe_y=64-i*10/3.0;
    oled.drawLine(0,axe_y, 2, axe_y);
    if (i%5==0){
       oled.setFont(u8g2_font_u8glib_4_tf);
       oled.setCursor(3, axe_y);
       oled.print(String(10*i));
    }
  }
  oled.sendBuffer();
  oled.setFont(u8g2_font_5x7_tf);
}







