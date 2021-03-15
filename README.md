# knot
![KNOT](/res/knot_hero_new.png)

**Knot is a DIY SMS-based asset tracking device to mitigate STOLEN or LOST property by tracking them with cellular (GSM 1400) and GPS.**

I wanted some way to track my bike, but couldn't find any really great options available on the internet. I figured it would be pretty easy to build one off-the-shelf using the Arduino MKR GSM 1400 that I could customize and integrate into my bike, and that's what this project is. KNOT just uses SMS so there's virtually no set up or cloud / IOT overhead.

## Setup
Setup is pretty straightforward. I'll add a wiring schematic when I've finished the hardware.

To get set up on the software side, you'll need to install the MKRGSM and MKRGPS libraries in your Arduino IDE, and then add an `arduino_secrets.h` file to the sketch `src/driver` folder with the following defined constants (set them accordingly for your needs)
```c++
#define SECRET_DEVICE_NAME "YOUR_DEVICE" // what you want your device to be called
#define SECRET_DEFAULT_REMOTE_NUMBER "+15555555555" //your phone number full name w/ + prepended, as string
#define PINNUMBER "" // SIM pin number. Currently not supported. TODO
```

Then just flash the firmware to your device, and make sure you have a valid SIM card installed. After that, you should be ready to go if the hardware is installed properly.

## Command Reference
Knot responds to SMS text commands (they can be anycased, but can't include additional characters). Just text the command to the # associated with the SIM. The following keywords are recognized:
| Command | Effect |
| --- | ----------- |
| subscribe | Enable automatic updates for when the device has been moved to a new location. Will only send updates when the device hasn't moved from it's last known location for a given amount of time. These can be configured in the firmware. |
| unsubscribe | Disable automatic updates if they were turned on with the *subscribe* command. |
| update | Sends a text message with the current location|

## Parts List
Working parts list. Will update when final V1 is assembled
- (1x) [Arduino MKR GSM 1400](https://store.arduino.cc/usa/mkr-gsm-1400)
- (1x) [Arduino MKR GPS Shield](https://store.arduino.cc/usa/mkr-gps-shield)
- (1x) [SIM Card](https://store.hologram.io/store/global-iot-sim-card/17/) (I used a hologram.io SIM, but any SIM valid in your region that supports SMS and a phone number should work fine)
- (1x) [3.7V 6500 mAh Rechargable LiPo battery](https://www.amazon.com/gp/product/B07TXHX3QT/ref=ppx_yo_dt_b_asin_title_o09_s01?ie=UTF8&psc=1) (You should be fine going for a larger battery as well, it's just a tradeoff between weight/size and battery life. Also note the 3.7V TTL, since the MKR boards don't support higher voltages)
- (1x) micro UFL GSM antenna
- (1x) Rocker switch (or other switch, for arming the device)
- (1x optional) Locking electrical switch (for power on/off)
- (1x optional) micro-usb cable to splice to external / on-device power so that the device doesn't need to be charged separately
- (TBD) assorted hardware
- (TBD) 3d printed parts
