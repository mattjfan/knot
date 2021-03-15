// Copyright (c) 2020 Matthew Fan

#include <Arduino_MKRGPS.h>
#include <MKRGSM.h>
#include "config.h"
#include "arduino_secrets.h"

#define SEDATED false // block things from happening, for testing
#define DEBUG_GSM false
#define SILENT_MODE false
#define DEBUG true
//#define PRINT_DEBUG(msg) (DEBUG ? Serial.println(msg) : () })
#ifdef DEBUG
  #define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

#define TIMEOUT_MS_ARMED 15 * 1000
#define TIMEOUT_MS_UNARMED 10 * 60 * 1000
#define IDLE_WINDOW_MS 10*1000
#define IDLE_WINDOW_DISTANCE_M 80
#define ARMED_MODE_PIN 2
#define IS_ARMED() (!digitalRead(ARMED_MODE_PIN))

#define ARMED_GEOFENCE_RADIUS_M 80

unsigned long global_epoch = 0;
unsigned long global_millis = 0;

typedef struct Timestamp {
  unsigned long ms;
  unsigned long epoch;
  Timestamp() {}
  Timestamp(unsigned long ms_) : ms(ms_), epoch(global_epoch) {}
  Timestamp(unsigned long ms_, unsigned long epoch_) : ms(ms_), epoch(epoch_) {}
} Timestamp;

typedef struct Position {
  float lat;
  float lng;
  Timestamp ts;
  bool is_set;
} Position;

void check_epoch() {
  if (millis() < global_millis) {
    global_epoch++;
  }
  global_millis = millis();
}

GSM gsmAccess(DEBUG_GSM);
GSM_SMS sms;

Position position = {0};
Position last_sent_position = {0};
Position armed_position = {0};
bool message_queued = false;
bool armed = false;
bool armed_message_sent = false;
Timestamp last_gps_measurement_ts = {0};
//bool force_immediate_gps_update = false;
bool queued_armed_gps_update = false;

void send_sms_message( const char* msg, const char* remote_number = SECRET_DEFAULT_REMOTE_NUMBER) {
  sms.beginSMS(remote_number);
  sms.print(msg);
  sms.endSMS();
}

void send_sms_message(String msg, const char* remote_number = SECRET_DEFAULT_REMOTE_NUMBER) {
    // TODO: check for String size before sending
  sms.beginSMS(remote_number);
  sms.print(msg.c_str());
  sms.endSMS();
}

String get_position_href() {
  return get_position_href(position);
}

String get_position_href(Position p) {
  return  "https://www.google.com/maps/place/" + String(p.lat) + "," + String(p.lng);
}

unsigned int get_time_delta_ms(Timestamp t0, Timestamp t1) {
  if (t0.epoch == t1.epoch) {
    return t1.ms - t0.ms;
  } else if (t1.epoch == t0.epoch + 1 &&
      t1.ms < t0.ms) {
    return (((unsigned int) -1) - t0.ms) + t1.ms;
  }
  return (unsigned int) -1;
}
unsigned int get_ms_since(Timestamp t0) {
  return get_time_delta_ms(t0, Timestamp(millis()));
}

class BaseTracker {
  public:
    BaseTracker() {}
    void pause() { paused = true; }
    void unpause() { paused = false; }
        // TODO use #ifdef to provide safety if secret number isn't provided
    static void send_sms_message_( const char* msg, const char* remote_number = SECRET_DEFAULT_REMOTE_NUMBER) {
      sms.beginSMS(remote_number);
      sms.print(msg);
      sms.endSMS();
    }
    static void send_sms_message_(String msg, const char* remote_number = SECRET_DEFAULT_REMOTE_NUMBER) {
      // TODO: check for String size before sending
      sms.beginSMS(remote_number);
      sms.print(msg.c_str());
      sms.endSMS();
    }
  protected:
    Timestamp last_update_ts = {0};
    Position last_update_position = {0};
    bool sent_message = false;
    bool paused = false;

    double get_distance_(Position position_a, Position position_b) {
      double R = 6371.;
      double delta_lat = (position_b.lat - position_a.lat) / 180 * PI;
      double delta_lng = (position_b.lng - position_a.lng) / 180 * PI;
      double a =
        sin(delta_lat / 2) * sin(delta_lat / 2) +
        sin(delta_lng / 2) * sin(delta_lng / 2) +
        cos(position_a.lat * PI / 180) * cos(position_b.lat * PI / 180);
      return 2 * R * asin(sqrt(a))* 1000; // in meters
    }
    double get_distance_from_last_position_() {
      return get_distance_(position, last_update_position);
    }
};

/**
 * Singleton used to better manage alerts
 * for when the device is armed. 
 * Main callback to put in loop is probably tick();
 */
class ArmedTracker : public BaseTracker {
  public:
    ArmedTracker() {}
    void update_position() { // callback for global position update queued
      last_update_position = position;
    }
    void tick() {
      if (paused == IS_ARMED()) { // if pin is diff than internal state
        paused = !paused; // set state to match pin state from last measurement (may have changed)
        if (!paused) {
          // TODO: set up and arm system
          last_update_position = position;
          queued_armed_gps_update = true; // global variable to queue a gps update
          sent_message = false;
        } else {
          // TODO: clean up and unarm system
          sent_message = false;
        }
      } else {
        if (paused || sent_message) { // ignore tick if paused or completed
          return;
        }
        if (has_left_armed_geofence()) {
          send_sms_message_(String("WARNING: I was removed from geofence while in ARMED mode.\nIf this wasn't you please respond SUBSCRIBE to receive location updates.\n") +
            get_position_href()
          );
          sent_message = true;
        }
      }
    }
  protected:
    bool has_left_armed_geofence() {
      return get_distance_from_last_position_() > ARMED_GEOFENCE_RADIUS_M;
    }
};

class DynamicTracker : public BaseTracker {
  public:
      // DynamicTracker() {}
    DynamicTracker(unsigned int idle_window_ms, float idle_distance_m, unsigned min_moving_update_ms) :
      BaseTracker(), idle_window_ms_(idle_window_ms), idle_distance_m_(idle_distance_m),
      min_moving_update_ms_(min_moving_update_ms) {}

    void init() {
      sent_message = false;
      last_update_position = position;
      last_update_ts = Timestamp(millis());

      paused = false;
    }

    void stop() { // idk if this really does anything warranting an explicit function
      paused = true;
    }
    
    // main callback
    void tick() {
      if (paused) {
        return;
      }
      // compute rolling window jitter and send an update when jitter has slowed
      // TODO: what's the diff between last_update_ts and last_update_position.ts ?
      if (get_distance_from_last_position_() > idle_distance_m_) {
        last_update_ts = Timestamp(millis());
        last_update_position = position;
        sent_message = false;
      } else if (get_ms_since(last_update_ts) > idle_window_ms_) {
        if (!sent_message) {
          send_sms_message_(String("I've stopped moving. Here is my current location: ") + get_position_href());
          sent_message = true;
        }
      } 
    }
  protected:
    unsigned int idle_window_ms_ = (unsigned int) -1;
    float idle_distance_m_ = 1000;
    unsigned int min_moving_update_ms_ = (unsigned int) -1;
    bool sent_idled_message = true;
    // Timestamp last_update_ts = {0};
    // Position last_update_position = {0};
};

ArmedTracker armedTracker;
DynamicTracker dynamicTracker(IDLE_WINDOW_MS, IDLE_WINDOW_DISTANCE_M, 60 * 60 * 1000);

String get_last_updated_time_string() {
  return "Last updated " + String(position.ts.epoch * 50) + " days and " + String((int) (position.ts.ms / 1000)) + " seconds";
}

// void send_message(const char* remote_number) {
// //  char msg[MAX_SMS_MSG_LENGTH];
//   if (position.lat <= 0 || position.lng <= 0) { // don't send message if we don't have any data
//     message_queued = false;
//     return;
//   }
//   String msg;
//   msg += "KNOT Location Update for " + String(SECRET_DEVICE_NAME) + ":\n";
//   if (position.lat > 0 && position.lng > 0) {
//     msg += "  lat: " + String(position.lat) + "\n";
//     msg += "  lng: " + String(position.lng) + "\n";
//     msg += "  " + get_last_updated_time_string() + "\n";
//     msg += "https://www.google.com/maps/place/" + String(position.lat) + "," + String(position.lng);

//   } else {     // TODO: add option to just not send any message if no data available
//     // TODO: would be cool if we could just queue up a message for when it does become available instead
//     msg += "  no position available :(";
//   }
//   // get send time?
//   // TODO: add check for 
// //  msg += "";
//   sms.beginSMS(remote_number);
//   sms.print(msg.c_str());
//   sms.endSMS();
// }

/**
 * 
 */
void handle_incoming_message(char* msg, char* remote_number) {
  DEBUG_PRINTLN("---------------------");
  DEBUG_PRINT("Remote #: ");
  DEBUG_PRINTLN(remote_number);
  DEBUG_PRINT("msg: ");
  DEBUG_PRINTLN(msg);
  DEBUG_PRINTLN("---------------------");

  String msg_string = String(msg);
  msg_string.toLowerCase(); // TODO: implement handling of different messages
  if (!strcmp(msg_string.c_str(), "help")) {
    send_sms_message("You can text UPDATE, SUBSCRIBE, or UNSUBSCRIBE. Checkout https://github.com/mattjfan/knot for more detailed info.");
//  } else if (!strcmp(msg_string.c_str(), "arm")) {
//    // remotely arm device
  } else if (!strcmp(msg_string.c_str(), "subscribe")) {
    if (!SILENT_MODE) {
      send_sms_message("You are now subscribed to automatic updates. Reply with UNSUBSCRIBE to stop updates.");
    }
    dynamicTracker.init();
  } else if (!strcmp(msg_string.c_str(), "unsubscribe")) {
    if (!SILENT_MODE) {
      send_sms_message("You have been unsubscribed from automatic updates.");
    }
    dynamicTracker.stop();
  } else if (!strcmp(msg_string.c_str(), "update")) {
    send_sms_message("Current location: " + get_position_href());
  } else if (!strcmp(msg_string.c_str(), "debug")) {
    // 
  } else {
    // Don't do anything for now
  }
}

void check_for_messages() {
  if (sms.available()) {
    DEBUG_PRINTLN("FOUND MESSAGE");
    char remote_number[20] = {0};
    sms.remoteNumber(remote_number, 20);
    char message[MAX_SMS_MSG_LENGTH] = {0}; // SMS size + null byte
    char* msg_ptr = message;
    char c;
    int msg_size_ct = 0; // to prevent segfault from longer than expected message
    while((int)(c=sms.read()) != 255) {
      if (msg_size_ct++ >= MAX_SMS_MSG_LENGTH - 1) {
        // ERROR: message is too long to fit in buffer
        DEBUG_PRINTLN("ERROR: SMS msg received is too large to fit in buffer. Truncated to fit.");
        break;
      }

      *msg_ptr = c;
      msg_ptr++;
    }
    handle_incoming_message(message,remote_number);
    sms.flush();
  }
}

void update_GPS_position(int timeout_ms = 1000) {
  if (!queued_armed_gps_update && ((armed && get_ms_since(last_gps_measurement_ts) < TIMEOUT_MS_ARMED) ||
      (!armed && get_ms_since(last_gps_measurement_ts) < TIMEOUT_MS_UNARMED)
      )) {
        // TODO: put into standby if not already
    return;
  }
  GPS.wakeup();
  unsigned long start_ms = millis();
  while (!GPS.available()) {
    if (millis() - start_ms > timeout_ms) {
      DEBUG_PRINT("GPS Acquisition timed out after " + String(timeout_ms) + " ms");
      // NOTE: we're not going into standby here to speed acq if timeout is too small
      return;
    }
  }
  last_gps_measurement_ts = Timestamp(millis());
  position.lat = GPS.latitude();
  position.lng = GPS.longitude();  
  position.ts = last_gps_measurement_ts;
  position.is_set = true;
  
  DEBUG_PRINT("GPS position set"); // TODO: more descriptive debug
  if (queued_armed_gps_update) {
    // armed_position = position; 
    armedTracker.update_position();
    queued_armed_gps_update = false;
  }
  GPS.standby(); // put GPS back into sleeping mode
}

void setup() {

  while(SEDATED) {}
  pinMode(ARMED_MODE_PIN, INPUT_PULLUP);
  // put your setup code here, to run once:
  #ifdef DEBUG
    Serial.begin(9600);
    while(!Serial) {}
  #endif

  DEBUG_PRINTLN("starting...");
  if (!GPS.begin(GPS_MODE_SHIELD)) {
    DEBUG_PRINTLN("Failed to initialize GPS!");
    while(1) {}
  }
  DEBUG_PRINT("Connecting to GSM... ");
  while(1) {
//    DEBUG_PRINTLN(".");
    if (gsmAccess.begin() == GSM_READY) {
      DEBUG_PRINTLN("GSM connected.");
      break;
    } else {
      DEBUG_PRINTLN("connecting...");
      delay(1000);
    }
  }
  dynamicTracker.pause(); // subscriptions disabled by default
}

void loop() {
  update_GPS_position(1000); // one sec timeout
  check_for_messages(); // TODO: add options for low power
  check_epoch();
  armedTracker.tick();
  dynamicTracker.tick();
  delay(1000); // Lower frequency of ticks to reduce power usage
}
